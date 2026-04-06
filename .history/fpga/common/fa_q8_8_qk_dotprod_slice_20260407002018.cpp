#include "fa_q8_8_qk_dotprod_slice.hpp"

namespace fpga {
namespace fa {

void qk_dotprod_slice_pair(const int16_t q0[kHeadDim],
                           const int16_t q1[kHeadDim],
                           const int16_t k_row[kHeadDim],
                           bool row1_valid,
                           int64_t &dp0,
                           int64_t &dp1) {
#pragma HLS INLINE off
#pragma HLS ARRAY_PARTITION variable=q0 cyclic factor=8 dim=1
#pragma HLS ARRAY_PARTITION variable=q1 cyclic factor=8 dim=1
#pragma HLS ARRAY_PARTITION variable=k_row cyclic factor=8 dim=1

  int64_t partial0[8];
  int64_t partial1[8];
#pragma HLS ARRAY_PARTITION variable=partial0 complete dim=1
#pragma HLS ARRAY_PARTITION variable=partial1 complete dim=1

  for (int i = 0; i < 8; ++i) {
#pragma HLS UNROLL
    partial0[i] = 0;
    partial1[i] = 0;
  }

  if (row1_valid) {
    for (int blk = 0; blk < (kHeadDim / 8); ++blk) {
#pragma HLS PIPELINE II=1
      for (int lane = 0; lane < 8; ++lane) {
#pragma HLS UNROLL
        const int d = blk * 8 + lane;
        const int32_t qk_mul0 = static_cast<int32_t>(q0[d]) * static_cast<int32_t>(k_row[d]);
#pragma HLS bind_op variable=qk_mul0 op=mul impl=dsp
        const int32_t qk_mul1 = static_cast<int32_t>(q1[d]) * static_cast<int32_t>(k_row[d]);
#pragma HLS bind_op variable=qk_mul1 op=mul impl=dsp
        partial0[lane] += qk_mul0;
        partial1[lane] += qk_mul1;
      }
    }
  } else {
    for (int blk = 0; blk < (kHeadDim / 8); ++blk) {
#pragma HLS PIPELINE II=1
      for (int lane = 0; lane < 8; ++lane) {
#pragma HLS UNROLL
        const int d = blk * 8 + lane;
        const int32_t qk_mul0 = static_cast<int32_t>(q0[d]) * static_cast<int32_t>(k_row[d]);
#pragma HLS bind_op variable=qk_mul0 op=mul impl=dsp
        partial0[lane] += qk_mul0;
      }
    }
  }

  dp0 = 0;
  dp1 = 0;
  for (int i = 0; i < 8; ++i) {
#pragma HLS UNROLL
    dp0 += partial0[i];
    dp1 += partial1[i];
  }
}

}  // namespace fa
}  // namespace fpga
