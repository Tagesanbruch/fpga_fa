#include "fa_q8_8_online_softmax_ctx.hpp"

#include "fa_q8_8_fixed_point.hpp"

namespace fpga {
namespace fa {

void online_softmax_ctx_row_step(int16_t score_q8_8,
                                 int16_t value_q8_8,
                                 int16_t &row_m,
                                 uint32_t &row_l,
                                 int32_t &row_acc) {
#pragma HLS INLINE off
  const int16_t m_old = row_m;
  const int16_t m_new = (score_q8_8 > m_old) ? score_q8_8 : m_old;
  const int16_t diff_old = static_cast<int16_t>(m_old - m_new);
  const int16_t diff_new = static_cast<int16_t>(score_q8_8 - m_new);
  const uint16_t exp_old = exp2_ctx_q1_15(diff_old);
  const uint16_t exp_new = exp2_ctx_q1_15(diff_new);
  const uint64_t l_scaled_mul = static_cast<uint64_t>(row_l) * exp_old;
#pragma HLS bind_op variable=l_scaled_mul op=mul impl=dsp
  const uint32_t l_scaled = static_cast<uint32_t>(l_scaled_mul >> 15);
  const uint32_t l_term = static_cast<uint32_t>(exp_new) << 1;
  row_l = to_u32(static_cast<uint64_t>(l_scaled) + l_term);
  row_m = m_new;

  const int64_t acc_old_mul = static_cast<int64_t>(row_acc) * static_cast<int64_t>(exp_old);
#pragma HLS bind_op variable=acc_old_mul op=mul impl=dsp
  const int32_t acc_old_sc = static_cast<int32_t>(acc_old_mul >> 15);
  const int64_t pv_mul = static_cast<int64_t>(exp_new) * static_cast<int64_t>(static_cast<int32_t>(value_q8_8));
#pragma HLS bind_op variable=pv_mul op=mul impl=dsp
  const int32_t pv_term = static_cast<int32_t>(pv_mul >> 7);
  row_acc = to_s32(static_cast<int64_t>(acc_old_sc) + static_cast<int64_t>(pv_term));
}

void online_softmax_ctx_acc_row_step(int16_t score_q8_8,
                                     const int16_t v_row[kHeadDim],
                                     int16_t &row_m,
                                     uint32_t &row_l,
                                     int32_t row_acc[kHeadDim]) {
#pragma HLS INLINE off
#pragma HLS ARRAY_PARTITION variable=v_row cyclic factor=8 dim=1
#pragma HLS ARRAY_PARTITION variable=row_acc cyclic factor=8 dim=1
#pragma HLS DEPENDENCE variable=row_acc inter false
  const int16_t m_old = row_m;
  const int16_t m_new = (score_q8_8 > m_old) ? score_q8_8 : m_old;
  const int16_t diff_old = static_cast<int16_t>(m_old - m_new);
  const int16_t diff_new = static_cast<int16_t>(score_q8_8 - m_new);
  const uint16_t exp_old = exp2_ctx_q1_15(diff_old);
  const uint16_t exp_new = exp2_ctx_q1_15(diff_new);
  const uint64_t l_scaled_mul = static_cast<uint64_t>(row_l) * exp_old;
#pragma HLS bind_op variable=l_scaled_mul op=mul impl=dsp
  const uint32_t l_scaled = static_cast<uint32_t>(l_scaled_mul >> 15);
  const uint32_t l_term = static_cast<uint32_t>(exp_new) << 1;
  row_l = to_u32(static_cast<uint64_t>(l_scaled) + l_term);
  row_m = m_new;

  for (int blk = 0; blk < (kHeadDim / 8); ++blk) {
#pragma HLS PIPELINE II=1
    for (int lane = 0; lane < 8; ++lane) {
#pragma HLS UNROLL
      const int d = blk * 8 + lane;
      const int64_t acc_old_mul = static_cast<int64_t>(row_acc[d]) * static_cast<int64_t>(exp_old);
#pragma HLS bind_op variable=acc_old_mul op=mul impl=dsp
      const int32_t acc_old_sc = static_cast<int32_t>(acc_old_mul >> 15);
      const int64_t pv_mul = static_cast<int64_t>(exp_new) * static_cast<int64_t>(static_cast<int32_t>(v_row[d]));
#pragma HLS bind_op variable=pv_mul op=mul impl=dsp
      const int32_t pv_term = static_cast<int32_t>(pv_mul >> 7);
      row_acc[d] = to_s32(static_cast<int64_t>(acc_old_sc) + static_cast<int64_t>(pv_term));
    }
  }
}

}  // namespace fa
}  // namespace fpga
