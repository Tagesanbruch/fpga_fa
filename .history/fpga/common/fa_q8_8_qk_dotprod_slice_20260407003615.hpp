#pragma once

#include <cstdint>

#include "fa_q8_8_attention.hpp"

namespace fpga {
namespace fa {

void qk_dotprod_slice_pair(const int16_t q0[kHeadDim],
                           const int16_t q1[kHeadDim],
                           const int16_t k_row[kHeadDim],
                           int64_t &dp0,
                           int64_t &dp1);

}  // namespace fa
}  // namespace fpga
