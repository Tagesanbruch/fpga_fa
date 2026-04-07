#include "fa_q8_8_qk_dotprod_slice.hpp"
#include "fa_q8_8_dot.hpp"

namespace fpga {
namespace fa {

void qk_dotprod_slice_pair(const int16_t q0[kHeadDim],
                           const int16_t q1[kHeadDim],
                           const int16_t k_row[kHeadDim],
                           int64_t &dp0,
                           int64_t &dp1) {
  fpga::qmath::dotprod_q8_8_pair(q0, q1, k_row, kHeadDim, dp0, dp1);
}

}  // namespace fa
}  // namespace fpga
