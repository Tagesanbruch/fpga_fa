#include "fa_q8_8_linear.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
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

struct CaseConfig {
  std::string name;
  int in_dim;
  int out_dim;
  bool gaussian;
};

bool run_case(const CaseConfig &cfg, double &worst_mae, double &worst_maxae, std::string &worst_case) {
  std::mt19937 rng(static_cast<uint32_t>(1000 + cfg.in_dim * 17 + cfg.out_dim * 31));
  std::uniform_real_distribution<float> uni_dist(-1.0f, 1.0f);
  std::normal_distribution<float> gauss_dist(0.0f, 0.35f);

  std::vector<int16_t> x(cfg.in_dim);
  std::vector<int16_t> w(cfg.in_dim * cfg.out_dim);
  std::vector<int16_t> y_strict(cfg.out_dim);
  std::vector<int16_t> y_hls(cfg.out_dim);
  std::vector<float> y_fp32(cfg.out_dim, 0.0f);
  uint32_t profile[fpga::linear::kProfileWords] = {};

  auto sample = [&](void) {
    return cfg.gaussian ? gauss_dist(rng) : uni_dist(rng);
  };

  for (int i = 0; i < cfg.in_dim; ++i) {
    x[i] = q8_8_from_float(sample());
  }
  for (int i = 0; i < cfg.in_dim * cfg.out_dim; ++i) {
    w[i] = q8_8_from_float(sample());
  }

  for (int row = 0; row < cfg.out_dim; ++row) {
    float acc = 0.0f;
    for (int i = 0; i < cfg.in_dim; ++i) {
      const float xv = static_cast<float>(x[i]) / 256.0f;
      const float wv = static_cast<float>(w[row * cfg.in_dim + i]) / 256.0f;
      acc += xv * wv;
    }
    y_fp32[row] = acc;
  }

  fpga::linear::run_gemv_strict(x.data(), w.data(), y_strict.data(), cfg.in_dim, cfg.out_dim, nullptr);
  fpga::linear::run_gemv_tiled_hls(x.data(), w.data(), y_hls.data(), cfg.in_dim, cfg.out_dim, profile);

  bool exact = true;
  double mae = 0.0;
  double maxae = 0.0;
  for (int row = 0; row < cfg.out_dim; ++row) {
    if (y_strict[row] != y_hls[row]) {
      exact = false;
    }
    const double hls_v = static_cast<double>(y_hls[row]) / 256.0;
    const double ae = std::abs(hls_v - static_cast<double>(y_fp32[row]));
    mae += ae;
    maxae = std::max(maxae, ae);
  }
  mae /= static_cast<double>(cfg.out_dim);

  std::cout << "[CASE] " << cfg.name
            << " exact=" << (exact ? "yes" : "no")
            << " mae=" << std::fixed << std::setprecision(6) << mae
            << " maxae=" << maxae
            << " macs=" << profile[fpga::linear::kProfileMacs]
            << "\n";

  if (mae > worst_mae) {
    worst_mae = mae;
    worst_case = cfg.name;
  }
  if (maxae > worst_maxae) {
    worst_maxae = maxae;
  }

  return exact;
}

}  // namespace

int main() {
  const std::vector<CaseConfig> cases = {
      {"small_uniform_64x64", 64, 64, false},
      {"rect_uniform_64x128", 64, 128, false},
      {"rect_gaussian_64x128", 64, 128, true},
      {"wide_uniform_128x64", 128, 64, false},
      {"wide_gaussian_128x128", 128, 128, true},
      {"max_uniform_256x256", 256, 256, false},
  };

  bool all_exact = true;
  double worst_mae = 0.0;
  double worst_maxae = 0.0;
  std::string worst_case;

  for (const auto &cfg : cases) {
    all_exact &= run_case(cfg, worst_mae, worst_maxae, worst_case);
  }

  std::cout << "[SUMMARY] exact_vs_strict=" << (all_exact ? "PASS" : "FAIL")
            << " worst_case=" << worst_case
            << " worst_mae=" << std::fixed << std::setprecision(6) << worst_mae
            << " worst_maxae=" << worst_maxae
            << "\n";

  return all_exact ? 0 : 1;
}
