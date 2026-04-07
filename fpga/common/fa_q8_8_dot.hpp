#pragma once

#include <cstdint>

namespace fpga {
namespace qmath {

constexpr int kDotMaxDim = 256;
constexpr int kDotLanes = 8;

int64_t dotprod_q8_8_single(const int16_t lhs[kDotMaxDim], const int16_t *rhs, int dim);

void dotprod_q8_8_pair(const int16_t lhs0[kDotMaxDim],
                       const int16_t lhs1[kDotMaxDim],
                       const int16_t *rhs,
                       int dim,
                       int64_t &dp0,
                       int64_t &dp1);

}  // namespace qmath
}  // namespace fpga
