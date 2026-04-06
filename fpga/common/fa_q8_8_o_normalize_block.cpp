#include "fa_q8_8_o_normalize_block.hpp"

#include "fa_q8_8_fixed_point.hpp"
#include "fa_q8_8_recip_nr_q16_16.hpp"

namespace fpga {
namespace fa {

void o_normalize_block_row(const int32_t row_acc[kHeadDim], uint32_t row_l, int16_t o_row[kHeadDim]) {
#pragma HLS INLINE off
  const uint32_t recip = recip_nr_rtl_q16_16(row_l);
  for (int d = 0; d < kHeadDim; ++d) {
#pragma HLS UNROLL factor=8
    if (row_l == 0) {
      o_row[d] = (row_acc[d] >= 0) ? 32767 : -32768;
    } else {
      const int64_t num = static_cast<int64_t>(row_acc[d]) << 8;
      const __int128 norm_mul_q32_32 =
          static_cast<__int128>(num) * static_cast<int64_t>(static_cast<int32_t>(recip));
      const __int128 norm_rounded_q32_32 =
          (norm_mul_q32_32 >= 0) ? (norm_mul_q32_32 + static_cast<__int128>(2147483648ll))
                                 : (norm_mul_q32_32 - static_cast<__int128>(2147483648ll));
      const int64_t norm_result = static_cast<int64_t>(norm_rounded_q32_32 >> 32);
      o_row[d] = sat_s16(static_cast<int32_t>(norm_result));
    }
  }
}

}  // namespace fa
}  // namespace fpga
