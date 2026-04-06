#include "../../../common/fa_q8_8_attention.hpp"

#include "attention_core.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>
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

std::vector<int16_t> make_random_matrix(int seq_len, int stride_elems, std::mt19937 &rng) {
  std::uniform_real_distribution<float> dist(-1.5f, 1.5f);
  std::vector<int16_t> data(static_cast<size_t>(seq_len * stride_elems), 0);
  for (int i = 0; i < seq_len; ++i) {
    for (int d = 0; d < fpga::fa::kHeadDim; ++d) {
      data[static_cast<size_t>(i * stride_elems + d)] = attn::float_to_q8_8(dist(rng));
    }
  }
  return data;
}

attn::MatrixI16 to_matrix(const std::vector<int16_t> &flat, int seq_len, int stride_elems) {
  attn::MatrixI16 m(seq_len, std::vector<int16_t>(fpga::fa::kHeadDim, 0));
  for (int i = 0; i < seq_len; ++i) {
    for (int d = 0; d < fpga::fa::kHeadDim; ++d) {
      m[i][d] = flat[static_cast<size_t>(i * stride_elems + d)];
    }
  }
  return m;
}

bool run_case(int seq_len, bool causal, int seed) {
  const int stride_bytes = fpga::fa::kHeadDim * static_cast<int>(sizeof(int16_t));
  const int stride_elems = stride_bytes / static_cast<int>(sizeof(int16_t));
  const int16_t scale_q8_8 = static_cast<int16_t>(32);
  const int16_t neg_large_q8_8 = static_cast<int16_t>(-2048);

  std::mt19937 rng(seed);
  auto q = make_random_matrix(seq_len, stride_elems, rng);
  auto k = make_random_matrix(seq_len, stride_elems, rng);
  auto v = make_random_matrix(seq_len, stride_elems, rng);
  std::vector<int16_t> got(static_cast<size_t>(seq_len * stride_elems), 0);
  uint32_t profile[fpga::fa::kProfileWords] = {};

  fa_attention_kernel(q.data(), k.data(), v.data(), got.data(), seq_len, stride_bytes, scale_q8_8, neg_large_q8_8,
                      causal ? 1 : 0, profile);

  const auto q_m = to_matrix(q, seq_len, stride_elems);
  const auto k_m = to_matrix(k, seq_len, stride_elems);
  const auto v_m = to_matrix(v, seq_len, stride_elems);
  const auto ref = attn::online_rtl_like(q_m, k_m, v_m, fpga::fa::kTileQ, fpga::fa::kTileK, causal,
                                         attn::Mode::RTL_STRICT, neg_large_q8_8, false);

  for (int i = 0; i < seq_len; ++i) {
    for (int d = 0; d < fpga::fa::kHeadDim; ++d) {
      const int16_t actual = got[static_cast<size_t>(i * stride_elems + d)];
      const int16_t expected = ref[i][d];
      if (actual != expected) {
        std::cerr << "[FAIL] seq_len=" << seq_len << " causal=" << causal << " seed=" << seed << " mismatch at ("
                  << i << "," << d << "): got=" << actual << " exp=" << expected << "\n";
        return false;
      }
    }
  }

  if (profile[fpga::fa::kProfileSeqLen] != static_cast<uint32_t>(seq_len)) {
    std::cerr << "[FAIL] profile seq_len mismatch\n";
    return false;
  }

  return true;
}

}  // namespace

int main() {
  const bool ok = run_case(64, true, 7) && run_case(128, false, 11) && run_case(256, true, 23);
  if (!ok) return 1;
  std::cout << "[PASS] fa_attention_kernel local csim matches cmodel strict reference\n";
  return 0;
}
