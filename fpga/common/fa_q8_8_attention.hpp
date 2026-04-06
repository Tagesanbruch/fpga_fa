#pragma once

#include <cstdint>

namespace fpga {
namespace fa {

constexpr int kMaxSeqLen = 256;
constexpr int kHeadDim = 64;
constexpr int kTileQ = 32;
constexpr int kTileK = 64;
constexpr int kRowPar = 2;
constexpr int kSoftmaxCtxs = 4;
constexpr int kQPairBatchRows = kRowPar * kSoftmaxCtxs;
constexpr int kQPairBatchCount = kTileQ / kQPairBatchRows;
constexpr int kProfileWords = 16;

enum ProfileIndex : int {
  kProfileSeqLen = 0,
  kProfileQTiles = 1,
  kProfileKTiles = 2,
  kProfileScoreEvals = 3,
  kProfileNormRows = 4,
  kProfileStrideBytes = 5,
  kProfileScaleQ8_8 = 6,
  kProfileCausal = 7,
  kProfileValidScoreEvals = 8,
  kProfileLoadQElems = 9,
  kProfileLoadKVElems = 10,
  kProfileStoreOElems = 11,
  kProfileInitRows = 12,
  kProfileNormalizeRows = 13,
  kProfileKVTileIters = 14,
  kProfileReserved0 = 15,
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

void run_attention_tiled_hls(const int16_t *q,
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
