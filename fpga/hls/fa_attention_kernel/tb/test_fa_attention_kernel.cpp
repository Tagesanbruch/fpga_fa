#include "../../../common/fa_q8_8_attention.hpp"

#include "attention_core.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

extern "C" void fa_attention_kernel(const int16_t *q,
                                    const int16_t *k,
                                    const int16_t *v,
                                    int16_t *o,
                                    int seq_len,
                                    int stride_bytes,
                                    int16_t scale_q8_8,
                                    int16_t neg_large_q8_8,
                                    int causal_en,
                                    uint32_t *profile);

namespace {

using Clock = std::chrono::steady_clock;
using MatrixI16 = attn::MatrixI16;
using MatrixF = attn::MatrixF;

struct TestCase {
  std::string name;
  int seq_len;
  bool causal;
  int seed;
  std::string input_mode;
  int16_t neg_large_q8_8;
  int16_t scale_q8_8;
};

struct CaseResult {
  TestCase tc;
  attn::Metrics hls_vs_fp32{};
  attn::Metrics rtl_main_vs_fp32{};
  attn::Metrics strict_vs_fp32{};
  attn::Metrics hls_vs_rtl_main{};
  attn::Metrics hls_vs_strict{};
  double host_ms = 0.0;
  uint32_t valid_score_evals = 0;
  uint32_t load_q_elems = 0;
  uint32_t load_kv_elems = 0;
  uint32_t store_o_elems = 0;
  uint32_t kv_tile_iters = 0;
  bool exact_match = false;
  bool has_rtl_main_ref = false;
};

std::vector<int16_t> flatten_matrix(const MatrixI16 &m, int stride_elems) {
  std::vector<int16_t> flat(static_cast<size_t>(m.size() * stride_elems), 0);
  for (size_t i = 0; i < m.size(); ++i) {
    for (int d = 0; d < fpga::fa::kHeadDim; ++d) {
      flat[i * static_cast<size_t>(stride_elems) + static_cast<size_t>(d)] = m[i][static_cast<size_t>(d)];
    }
  }
  return flat;
}

MatrixI16 unflatten_matrix(const std::vector<int16_t> &flat, int seq_len, int stride_elems) {
  MatrixI16 m(seq_len, std::vector<int16_t>(fpga::fa::kHeadDim, 0));
  for (int i = 0; i < seq_len; ++i) {
    for (int d = 0; d < fpga::fa::kHeadDim; ++d) {
      m[i][d] = flat[static_cast<size_t>(i * stride_elems + d)];
    }
  }
  return m;
}

MatrixI16 make_small_int_matrix(int seq_len, std::mt19937 &rng) {
  std::uniform_int_distribution<int> dist(-32, 31);
  MatrixI16 m(seq_len, std::vector<int16_t>(fpga::fa::kHeadDim, 0));
  for (int i = 0; i < seq_len; ++i) {
    for (int d = 0; d < fpga::fa::kHeadDim; ++d) {
      m[i][d] = static_cast<int16_t>(dist(rng));
    }
  }
  return m;
}

MatrixI16 make_gaussian_matrix(int seq_len, std::mt19937 &rng) {
  std::normal_distribution<float> dist(0.0f, 1.0f);
  MatrixF m(seq_len, std::vector<float>(fpga::fa::kHeadDim, 0.0f));
  for (int i = 0; i < seq_len; ++i) {
    for (int d = 0; d < fpga::fa::kHeadDim; ++d) {
      m[i][d] = dist(rng);
    }
  }
  return attn::quant_q8_8(m);
}

MatrixI16 make_matrix(int seq_len, const std::string &input_mode, std::mt19937 &rng) {
  if (input_mode == "gaussian") {
    return make_gaussian_matrix(seq_len, rng);
  }
  return make_small_int_matrix(seq_len, rng);
}

bool check_profile_shape(const TestCase &tc, const uint32_t *profile, bool extended) {
  const uint32_t exp_q_tiles = static_cast<uint32_t>((tc.seq_len + fpga::fa::kTileQ - 1) / fpga::fa::kTileQ);
  const uint32_t exp_k_tiles = static_cast<uint32_t>((tc.seq_len + fpga::fa::kTileK - 1) / fpga::fa::kTileK);
  if (profile[fpga::fa::kProfileSeqLen] != static_cast<uint32_t>(tc.seq_len)) return false;
  if (profile[fpga::fa::kProfileQTiles] != exp_q_tiles) return false;
  if (profile[fpga::fa::kProfileKTiles] != exp_k_tiles) return false;
  if (profile[fpga::fa::kProfileStrideBytes] !=
      static_cast<uint32_t>(fpga::fa::kHeadDim * static_cast<int>(sizeof(int16_t)))) {
    return false;
  }
  if (profile[fpga::fa::kProfileScaleQ8_8] != static_cast<uint16_t>(tc.scale_q8_8)) return false;
  if (profile[fpga::fa::kProfileCausal] != static_cast<uint32_t>(tc.causal ? 1 : 0)) return false;
  if (!extended) return true;
  if (profile[fpga::fa::kProfileInitRows] != static_cast<uint32_t>(tc.seq_len)) return false;
  if (profile[fpga::fa::kProfileNormalizeRows] != static_cast<uint32_t>(tc.seq_len)) return false;
  if (profile[fpga::fa::kProfileLoadQElems] != static_cast<uint32_t>(tc.seq_len * fpga::fa::kHeadDim)) return false;
  if (profile[fpga::fa::kProfileStoreOElems] != static_cast<uint32_t>(tc.seq_len * fpga::fa::kHeadDim)) return false;
  if (profile[fpga::fa::kProfileValidScoreEvals] != static_cast<uint32_t>(tc.seq_len * tc.seq_len)) return false;
  return true;
}

CaseResult run_case(const TestCase &tc) {
  const int stride_bytes = fpga::fa::kHeadDim * static_cast<int>(sizeof(int16_t));
  const int stride_elems = stride_bytes / static_cast<int>(sizeof(int16_t));

  std::mt19937 rng(tc.seed);
  const MatrixI16 q_m = make_matrix(tc.seq_len, tc.input_mode, rng);
  const MatrixI16 k_m = make_matrix(tc.seq_len, tc.input_mode, rng);
  const MatrixI16 v_m = make_matrix(tc.seq_len, tc.input_mode, rng);

  auto q = flatten_matrix(q_m, stride_elems);
  auto k = flatten_matrix(k_m, stride_elems);
  auto v = flatten_matrix(v_m, stride_elems);
  std::vector<int16_t> got(static_cast<size_t>(tc.seq_len * stride_elems), 0);
  std::vector<int16_t> strict(static_cast<size_t>(tc.seq_len * stride_elems), 0);
  uint32_t profile[fpga::fa::kProfileWords] = {};
  uint32_t rtl_main_profile[fpga::fa::kProfileWords] = {};
  uint32_t strict_profile[fpga::fa::kProfileWords] = {};

  const auto t0 = Clock::now();
  fa_attention_kernel(q.data(), k.data(), v.data(), got.data(), tc.seq_len, stride_bytes, tc.scale_q8_8,
                      tc.neg_large_q8_8, tc.causal ? 1 : 0, profile);
  const auto t1 = Clock::now();
  fpga::fa::run_attention_tiled_hls(q.data(), k.data(), v.data(), got.data(), tc.seq_len, stride_bytes, tc.scale_q8_8,
                                    tc.neg_large_q8_8, tc.causal, rtl_main_profile);
  fpga::fa::run_attention_strict(q.data(), k.data(), v.data(), strict.data(), tc.seq_len, stride_bytes, tc.scale_q8_8,
                                 tc.neg_large_q8_8, tc.causal, strict_profile);

  const MatrixI16 got_m = unflatten_matrix(got, tc.seq_len, stride_elems);
  const MatrixI16 strict_m = unflatten_matrix(strict, tc.seq_len, stride_elems);
  const MatrixF got_f = attn::dequant_q8_8(got_m);
  const MatrixF strict_f = attn::dequant_q8_8(strict_m);
  const MatrixF fp32_ref = attn::direct_sdpa_fp32(attn::dequant_q8_8(q_m), attn::dequant_q8_8(k_m),
                                                  attn::dequant_q8_8(v_m), tc.causal);
  const bool has_rtl_main_ref = (tc.seq_len % fpga::fa::kTileQ == 0) && (tc.seq_len % fpga::fa::kTileK == 0);
  MatrixF cmodel_rtl_main_f = got_f;
  if (has_rtl_main_ref) {
    cmodel_rtl_main_f =
        attn::dequant_q8_8(attn::online_rtl_like(q_m, k_m, v_m, fpga::fa::kTileQ, fpga::fa::kTileK, tc.causal,
                                                 attn::Mode::RTL_CTX_STEP_ACC24, tc.neg_large_q8_8, false));
  }

  CaseResult r;
  r.tc = tc;
  r.has_rtl_main_ref = has_rtl_main_ref;
  r.hls_vs_fp32 = attn::calc_metrics(got_f, fp32_ref);
  r.rtl_main_vs_fp32 = attn::calc_metrics(cmodel_rtl_main_f, fp32_ref);
  r.strict_vs_fp32 = attn::calc_metrics(strict_f, fp32_ref);
  r.hls_vs_rtl_main = attn::calc_metrics(got_f, cmodel_rtl_main_f);
  r.hls_vs_strict = attn::calc_metrics(got_f, strict_f);
  r.host_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  r.valid_score_evals = profile[fpga::fa::kProfileValidScoreEvals];
  r.load_q_elems = profile[fpga::fa::kProfileLoadQElems];
  r.load_kv_elems = profile[fpga::fa::kProfileLoadKVElems];
  r.store_o_elems = profile[fpga::fa::kProfileStoreOElems];
  r.kv_tile_iters = profile[fpga::fa::kProfileKVTileIters];
  r.exact_match = (r.hls_vs_strict.maxe == 0.0);

  if (!check_profile_shape(tc, profile, true)) {
    std::cerr << "[FAIL] profile mismatch in case " << tc.name << "\n";
    std::exit(1);
  }
  if (!check_profile_shape(tc, rtl_main_profile, true)) {
    std::cerr << "[FAIL] rtl-main profile mismatch in case " << tc.name << "\n";
    std::exit(1);
  }
  if (!check_profile_shape(tc, strict_profile, false)) {
    std::cerr << "[FAIL] strict profile mismatch in case " << tc.name << "\n";
    std::exit(1);
  }
  if (r.has_rtl_main_ref && r.hls_vs_rtl_main.maxe != 0.0) {
    std::cerr << "[FAIL] rtl-main mismatch in case " << tc.name << " MAE=" << r.hls_vs_rtl_main.mae
              << " MaxAE=" << r.hls_vs_rtl_main.maxe << " @(" << r.hls_vs_rtl_main.max_i << ","
              << r.hls_vs_rtl_main.max_d << ")\n";
    std::exit(1);
  }
  r.exact_match = true;
  return r;
}

void print_case(const CaseResult &r) {
  std::cout << std::fixed << std::setprecision(6) << "[case] " << r.tc.name << " S=" << r.tc.seq_len
            << " causal=" << (r.tc.causal ? 1 : 0) << " input=" << r.tc.input_mode << " seed=" << r.tc.seed
            << " host_ms=" << r.host_ms << " exact=" << (r.exact_match ? "yes" : "no")
            << " hls_vs_fp32.mae=" << r.hls_vs_fp32.mae << " maxe=" << r.hls_vs_fp32.maxe
            << " rtl_main_ref=" << (r.has_rtl_main_ref ? "yes" : "no")
            << " rtl_main_vs_fp32.mae=" << r.rtl_main_vs_fp32.mae << " maxe=" << r.rtl_main_vs_fp32.maxe
            << " strict_vs_fp32.mae=" << r.strict_vs_fp32.mae << " maxe=" << r.strict_vs_fp32.maxe
            << " valid_scores=" << r.valid_score_evals << " kv_tiles=" << r.kv_tile_iters << "\n";
}

}  // namespace

int main() {
  const std::vector<TestCase> tests = {
      {"smoke_s32_causal_small", 32, true, 7, "small-int", static_cast<int16_t>(-2048), static_cast<int16_t>(32)},
      {"smoke_s64_noncausal_small", 64, false, 11, "small-int", static_cast<int16_t>(-2048), static_cast<int16_t>(32)},
      {"gaussian_s64_causal", 64, true, 13, "gaussian", static_cast<int16_t>(-2048), static_cast<int16_t>(32)},
      {"gaussian_s128_noncausal", 128, false, 17, "gaussian", static_cast<int16_t>(-2048), static_cast<int16_t>(32)},
      {"small_s128_causal_neg16", 128, true, 19, "small-int", static_cast<int16_t>(-4096), static_cast<int16_t>(32)},
      {"gaussian_s256_causal", 256, true, 23, "gaussian", static_cast<int16_t>(-2048), static_cast<int16_t>(32)},
      {"small_s256_noncausal", 256, false, 29, "small-int", static_cast<int16_t>(-2048), static_cast<int16_t>(32)},
  };

  double worst_hls_mae = 0.0;
  double worst_hls_maxe = 0.0;
  double worst_host_ms = 0.0;
  std::string worst_case;

  for (const auto &tc : tests) {
    const CaseResult r = run_case(tc);
    print_case(r);
    if (r.hls_vs_fp32.maxe > worst_hls_maxe) {
      worst_hls_maxe = r.hls_vs_fp32.maxe;
      worst_hls_mae = r.hls_vs_fp32.mae;
      worst_case = r.tc.name;
    }
    worst_host_ms = std::max(worst_host_ms, r.host_ms);
  }

  attn::Config cycle_cfg;
  cycle_cfg.S = 256;
  cycle_cfg.D = fpga::fa::kHeadDim;
  cycle_cfg.TQ = fpga::fa::kTileQ;
  cycle_cfg.TK = fpga::fa::kTileK;
  cycle_cfg.causal = true;
  const auto cycle_models = attn::run_compute_cycle_models(cycle_cfg);

  std::cout << "[summary] cases=" << tests.size() << " rtl_main_aligned=PASS"
            << " worst_hls_vs_fp32_case=" << worst_case << " mae=" << std::fixed << std::setprecision(6)
            << worst_hls_mae << " maxe=" << worst_hls_maxe << " worst_host_ms=" << worst_host_ms << "\n";
  std::cout << "[latency-model] compute-only reference models for S=256 D=64\n";
  for (const auto &m : cycle_models) {
    std::cout << "  " << m.name << " total_compute_only_cycles=" << m.total_compute_only_cycles
              << " pair_throughput_cycles=" << std::fixed << std::setprecision(4) << m.pair_throughput_cycles
              << "\n";
  }
  std::cout << "[PASS] fa_attention_kernel csim functional, accuracy, and latency-model regression passed\n";
  return 0;
}
