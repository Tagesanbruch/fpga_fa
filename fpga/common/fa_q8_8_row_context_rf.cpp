#include "fa_q8_8_row_context_rf.hpp"

namespace fpga {
namespace fa {

void init_row_context_rf(int16_t row_m[kTileQ],
                         uint32_t row_l[kTileQ],
                         int32_t row_acc[kTileQ][kHeadDim],
                         int valid_q,
                         int16_t neg_large_q8_8) {
#pragma HLS INLINE off
  for (int d = 0; d < kHeadDim; ++d) {
#pragma HLS PIPELINE II=1
    for (int qi = 0; qi < kTileQ; qi += kRowPar) {
      if (qi < valid_q) row_acc[qi][d] = 0;
      if ((qi + 1) < valid_q) row_acc[qi + 1][d] = 0;
    }
  }

  for (int qi = 0; qi < kTileQ; qi += kRowPar) {
#pragma HLS PIPELINE II=1
    if (qi < valid_q) {
      row_m[qi] = neg_large_q8_8;
      row_l[qi] = 0;
    }
    if ((qi + 1) < valid_q) {
      row_m[qi + 1] = neg_large_q8_8;
      row_l[qi + 1] = 0;
    }
  }
}

}  // namespace fa
}  // namespace fpga
