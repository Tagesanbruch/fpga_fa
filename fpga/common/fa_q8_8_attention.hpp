#pragma once

#include <cstdint>

namespace fpga {
namespace fa {

constexpr int kMaxSeqLen = 256;
constexpr int kHeadDim = 64;
constexpr int kTileQ = 32;
constexpr int kTileK = 64;
constexpr int kProfileWords = 8;

enum ProfileIndex : int {
  kProfileSeqLen = 0,
  kProfileQTiles = 1,
  kProfileKTiles = 2,
  kProfileScoreEvals = 3,
  kProfileNormRows = 4,
  kProfileStrideBytes = 5,
  kProfileScaleQ8_8 = 6,
  kProfileCausal = 7,
};

void clear_profile(uint32_t *profile);

void run_attention_strict(const int16_t *q,
                          const int16_t *k,
                          const int16_t *v,
                          int16_t *o,
                          int seq_len,
                          int stride_bytes,
                          int16_t scale_q8_8,
                          int16_t neg_large_q8_8,
                          bool causal,
                          uint32_t *profile);

}  // namespace fa
}  // namespace fpga
