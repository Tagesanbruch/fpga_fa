#pragma once

#include <cstdint>

#include "fa_q8_8_attention.hpp"

namespace fpga {
namespace fa {

void init_row_context_rf(int16_t row_m[kTileQ],
                         uint32_t row_l[kTileQ],
                         int32_t row_acc[kTileQ][kHeadDim],
                         int valid_q,
                         int16_t neg_large_q8_8);

}  // namespace fa
}  // namespace fpga
