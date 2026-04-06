#include "../../common/fa_q8_8_attention.hpp"

#include <cstdint>

extern "C" {

void fa_attention_kernel(const int16_t *q,
                         const int16_t *k,
                         const int16_t *v,
                         int16_t *o,
                         int seq_len,
                         int stride_bytes,
                         int16_t scale_q8_8,
                         int16_t neg_large_q8_8,
                         int causal_en,
                         uint32_t *profile) {
#pragma HLS INTERFACE m_axi port = q offset = slave bundle = gmem0 depth = 16384
#pragma HLS INTERFACE m_axi port = k offset = slave bundle = gmem1 depth = 16384
#pragma HLS INTERFACE m_axi port = v offset = slave bundle = gmem2 depth = 16384
#pragma HLS INTERFACE m_axi port = o offset = slave bundle = gmem3 depth = 16384
#pragma HLS INTERFACE m_axi port = profile offset = slave bundle = gmem4 depth = 8

#pragma HLS INTERFACE s_axilite port = q bundle = control
#pragma HLS INTERFACE s_axilite port = k bundle = control
#pragma HLS INTERFACE s_axilite port = v bundle = control
#pragma HLS INTERFACE s_axilite port = o bundle = control
#pragma HLS INTERFACE s_axilite port = seq_len bundle = control
#pragma HLS INTERFACE s_axilite port = stride_bytes bundle = control
#pragma HLS INTERFACE s_axilite port = scale_q8_8 bundle = control
#pragma HLS INTERFACE s_axilite port = neg_large_q8_8 bundle = control
#pragma HLS INTERFACE s_axilite port = causal_en bundle = control
#pragma HLS INTERFACE s_axilite port = profile bundle = control
#pragma HLS INTERFACE s_axilite port = return bundle = control

  fpga::fa::run_attention_tiled_hls(q, k, v, o, seq_len, stride_bytes, scale_q8_8, neg_large_q8_8, causal_en != 0,
                                    profile);
}

}  // extern "C"
