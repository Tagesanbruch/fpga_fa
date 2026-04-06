#pragma once

#include <cstdint>

#include "fa_q8_8_attention.hpp"

namespace fpga {
namespace fa {

void o_normalize_block_row(const int32_t row_acc[kHeadDim], uint32_t row_l, int16_t o_row[kHeadDim]);

}  // namespace fa
}  // namespace fpga
