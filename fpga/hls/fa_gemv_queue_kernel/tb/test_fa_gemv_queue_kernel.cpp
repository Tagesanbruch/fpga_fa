#include "fa_q8_8_linear.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

int16_t q8_8_from_float(float value) {
  const long scaled = std::lround(static_cast<double>(value) * 256.0);
  const long clamped = std::max<long>(-32768, std::min<long>(32767, scaled));
  return static_cast<int16_t>(clamped);
}

struct QueueCase {
  std::string name;
  int task_count;
  int in_dim;
  int out_dim;
  bool gaussian;
};

void fill_random(std::vector<int16_t> &buf, bool gaussian, std::mt19937 &rng) {
  std::uniform_real_distribution<float> uni_dist(-1.0f, 1.0f);
  std::normal_distribution<float> gauss_dist(0.0f, 0.35f);
  for (auto &v : buf) {
    v = q8_8_from_float(gaussian ? gauss_dist(rng) : uni_dist(rng));
  }
}

bool run_case(const QueueCase &cfg,
              double &worst_mae,
              double &worst_maxae,
              std::string &worst_case,
              double &best_queue_speedup) {
  using clock = std::chrono::high_resolution_clock;

  std::mt19937 rng(static_cast<uint32_t>(20260407 + cfg.task_count * 17 + cfg.in_dim * 31 + cfg.out_dim * 13));
  const int x_task_elems = cfg.in_dim;
  const int w_task_elems = cfg.in_dim * cfg.out_dim;
  const int y_task_elems = cfg.out_dim;

  std::vector<fpga::linear::GemvTaskDesc> tasks(cfg.task_count);
  std::vector<int16_t> x_all(static_cast<size_t>(cfg.task_count * x_task_elems));
  std::vector<int16_t> w_all(static_cast<size_t>(cfg.task_count * w_task_elems));
  std::vector<int16_t> y_strict(static_cast<size_t>(cfg.task_count * y_task_elems), 0);
  std::vector<int16_t> y_hls_queue(static_cast<size_t>(cfg.task_count * y_task_elems), 0);
  std::vector<int16_t> y_hls_serial(static_cast<size_t>(cfg.task_count * y_task_elems), 0);
  std::vector<float> y_fp32(static_cast<size_t>(cfg.task_count * y_task_elems), 0.0f);
  uint32_t queue_profile[fpga::linear::kProfileWords] = {};

  for (int task_idx = 0; task_idx < cfg.task_count; ++task_idx) {
    tasks[task_idx] = {static_cast<uint32_t>(task_idx * x_task_elems),
                       static_cast<uint32_t>(task_idx * w_task_elems),
                       static_cast<uint32_t>(task_idx * y_task_elems),
                       static_cast<uint16_t>(cfg.in_dim),
                       static_cast<uint16_t>(cfg.out_dim)};
  }

  fill_random(x_all, cfg.gaussian, rng);
  fill_random(w_all, cfg.gaussian, rng);

  for (int task_idx = 0; task_idx < cfg.task_count; ++task_idx) {
    const auto &task = tasks[task_idx];
    for (int row = 0; row < cfg.out_dim; ++row) {
      float acc = 0.0f;
      for (int i = 0; i < cfg.in_dim; ++i) {
        const float xv = static_cast<float>(x_all[task.x_offset_elems + i]) / 256.0f;
        const float wv = static_cast<float>(w_all[task.w_offset_elems + row * cfg.in_dim + i]) / 256.0f;
        acc += xv * wv;
      }
      y_fp32[task.y_offset_elems + row] = acc;
    }
  }

  auto strict_begin = clock::now();
  fpga::linear::run_gemv_queue_strict(x_all.data(), w_all.data(), y_strict.data(), tasks.data(), cfg.task_count, nullptr);
  auto strict_end = clock::now();

  auto serial_begin = clock::now();
  for (int task_idx = 0; task_idx < cfg.task_count; ++task_idx) {
    const auto &task = tasks[task_idx];
    fpga::linear::run_gemv_tiled_hls(x_all.data() + task.x_offset_elems,
                                     w_all.data() + task.w_offset_elems,
                                     y_hls_serial.data() + task.y_offset_elems,
                                     task.in_dim,
                                     task.out_dim,
                                     nullptr);
  }
  auto serial_end = clock::now();

  auto queue_begin = clock::now();
  fpga::linear::run_gemv_queue_tiled_hls(x_all.data(), w_all.data(), y_hls_queue.data(), tasks.data(), cfg.task_count,
                                         queue_profile);
  auto queue_end = clock::now();

  const double strict_ms = std::chrono::duration<double, std::milli>(strict_end - strict_begin).count();
  const double serial_hls_ms = std::chrono::duration<double, std::milli>(serial_end - serial_begin).count();
  const double queue_hls_ms = std::chrono::duration<double, std::milli>(queue_end - queue_begin).count();
  const double queue_speedup = serial_hls_ms / std::max(queue_hls_ms, 1e-9);

  bool exact = true;
  bool queue_matches_serial = true;
  double mae = 0.0;
  double maxae = 0.0;
  for (int task_idx = 0; task_idx < cfg.task_count; ++task_idx) {
    const auto &task = tasks[task_idx];
    for (int row = 0; row < cfg.out_dim; ++row) {
      const auto idx = static_cast<size_t>(task.y_offset_elems + row);
      if (y_hls_queue[idx] != y_strict[idx]) {
        exact = false;
      }
      if (y_hls_queue[idx] != y_hls_serial[idx]) {
        queue_matches_serial = false;
      }
      const double hls_v = static_cast<double>(y_hls_queue[idx]) / 256.0;
      const double ae = std::abs(hls_v - static_cast<double>(y_fp32[idx]));
      mae += ae;
      maxae = std::max(maxae, ae);
    }
  }
  mae /= static_cast<double>(cfg.task_count * cfg.out_dim);

  std::cout << "[CASE] " << cfg.name
            << " exact=" << (exact ? "yes" : "no")
            << " serial_match=" << (queue_matches_serial ? "yes" : "no")
            << " mae=" << std::fixed << std::setprecision(6) << mae
            << " maxae=" << maxae
            << " strict_ms=" << strict_ms
            << " serial_hls_ms=" << serial_hls_ms
            << " queue_hls_ms=" << queue_hls_ms
            << " queue_speedup=" << queue_speedup
            << " task_count=" << queue_profile[fpga::linear::kProfileTaskCount]
            << " total_macs=" << queue_profile[fpga::linear::kProfileMacs]
            << "\n";

  if (mae > worst_mae) {
    worst_mae = mae;
    worst_case = cfg.name;
  }
  if (maxae > worst_maxae) {
    worst_maxae = maxae;
  }
  best_queue_speedup = std::max(best_queue_speedup, queue_speedup);
  return exact && queue_matches_serial;
}

}  // namespace

int main() {
  const std::vector<QueueCase> cases = {
      {"uniform_q4_64x64", 4, 64, 64, false},
      {"gaussian_q4_64x128", 4, 64, 128, true},
      {"uniform_q8_128x128", 8, 128, 128, false},
      {"gaussian_q8_256x256", 8, 256, 256, true},
  };

  bool all_pass = true;
  double worst_mae = 0.0;
  double worst_maxae = 0.0;
  double best_queue_speedup = 0.0;
  std::string worst_case;

  for (const auto &cfg : cases) {
    all_pass &= run_case(cfg, worst_mae, worst_maxae, worst_case, best_queue_speedup);
  }

  std::cout << "[SUMMARY] exact_vs_strict=" << (all_pass ? "PASS" : "FAIL")
            << " worst_case=" << worst_case
            << " worst_mae=" << std::fixed << std::setprecision(6) << worst_mae
            << " worst_maxae=" << worst_maxae
            << " best_queue_speedup=" << best_queue_speedup
            << "\n";

  return all_pass ? 0 : 1;
}
