#include "fa_q8_8_dot.hpp"

namespace fpga {
namespace qmath {

int64_t dotprod_q8_8_single(const int16_t lhs[kDotMaxDim], const int16_t *rhs, int dim) {
#pragma HLS INLINE off
  int64_t partial[kDotLanes];
#pragma HLS ARRAY_PARTITION variable=partial complete dim=1

  for (int lane = 0; lane < kDotLanes; ++lane) {
#pragma HLS UNROLL
    partial[lane] = 0;
  }

  for (int i = 0; i < dim; ++i) {
#pragma HLS UNROLL factor=8
    const int32_t prod = static_cast<int32_t>(lhs[i]) * static_cast<int32_t>(rhs[i]);
#pragma HLS bind_op variable=prod op=mul impl=dsp
    partial[i & (kDotLanes - 1)] += prod;
  }

  int64_t dp = 0;
  for (int lane = 0; lane < kDotLanes; ++lane) {
#pragma HLS UNROLL
    dp += partial[lane];
  }
  return dp;
}

void dotprod_q8_8_pair(const int16_t lhs0[kDotMaxDim],
                       const int16_t lhs1[kDotMaxDim],
                       const int16_t *rhs,
                       int dim,
                       int64_t &dp0,
                       int64_t &dp1) {
#pragma HLS INLINE off
  int64_t partial0[kDotLanes];
  int64_t partial1[kDotLanes];
#pragma HLS ARRAY_PARTITION variable=partial0 complete dim=1
#pragma HLS ARRAY_PARTITION variable=partial1 complete dim=1

  for (int lane = 0; lane < kDotLanes; ++lane) {
#pragma HLS UNROLL
    partial0[lane] = 0;
    partial1[lane] = 0;
  }

  for (int i = 0; i < dim; ++i) {
#pragma HLS UNROLL factor=8
    const int16_t rhs_val = rhs[i];
    const int32_t prod0 = static_cast<int32_t>(lhs0[i]) * static_cast<int32_t>(rhs_val);
    const int32_t prod1 = static_cast<int32_t>(lhs1[i]) * static_cast<int32_t>(rhs_val);
#pragma HLS bind_op variable=prod0 op=mul impl=dsp
#pragma HLS bind_op variable=prod1 op=mul impl=dsp
    partial0[i & (kDotLanes - 1)] += prod0;
    partial1[i & (kDotLanes - 1)] += prod1;
  }

  dp0 = 0;
  dp1 = 0;
  for (int lane = 0; lane < kDotLanes; ++lane) {
#pragma HLS UNROLL
    dp0 += partial0[lane];
    dp1 += partial1[lane];
  }
}

}  // namespace qmath
}  // namespace fpga
