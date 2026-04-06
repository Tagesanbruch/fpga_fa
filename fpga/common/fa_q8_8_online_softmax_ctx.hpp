#pragma once

#include <cstdint>

#include "fa_q8_8_attention.hpp"

namespace fpga {
namespace fa {

void online_softmax_ctx_row_step(int16_t score_q8_8,
                                 int16_t value_q8_8,
                                 int16_t &row_m,
                                 uint32_t &row_l,
                                 int32_t &row_acc);

void online_softmax_ctx_acc_row_step(int16_t score_q8_8,
                                     const int16_t v_row[kHeadDim],
                                     int16_t &row_m,
                                     uint32_t &row_l,
                                     int32_t row_acc[kHeadDim]);

}  // namespace fa
}  // namespace fpga
