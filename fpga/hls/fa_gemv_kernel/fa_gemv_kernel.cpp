#include "fa_q8_8_linear.hpp"

extern "C" {

void fa_gemv_kernel(const int16_t *x,
                    const int16_t *w,
                    int16_t *y,
                    int in_dim,
                    int out_dim,
                    uint32_t *profile) {
#pragma HLS INTERFACE m_axi port=x offset=slave bundle=gmem0 depth=256
#pragma HLS INTERFACE m_axi port=w offset=slave bundle=gmem1 depth=65536
#pragma HLS INTERFACE m_axi port=y offset=slave bundle=gmem2 depth=256
#pragma HLS INTERFACE m_axi port=profile offset=slave bundle=gmem3 depth=8

#pragma HLS INTERFACE s_axilite port=x bundle=control
#pragma HLS INTERFACE s_axilite port=w bundle=control
#pragma HLS INTERFACE s_axilite port=y bundle=control
#pragma HLS INTERFACE s_axilite port=in_dim bundle=control
#pragma HLS INTERFACE s_axilite port=out_dim bundle=control
#pragma HLS INTERFACE s_axilite port=profile bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

  fpga::linear::run_gemv_tiled_hls(x, w, y, in_dim, out_dim, profile);
}

}  // extern "C"
