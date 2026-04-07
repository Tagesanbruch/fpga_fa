#include <cstdint>

extern "C" {

void add_seq_kernel(const uint32_t* in, uint32_t* out, int length, uint32_t base_add) {
#pragma HLS INTERFACE m_axi port = in bundle = gmem0 depth = 4096
#pragma HLS INTERFACE m_axi port = out bundle = gmem1 depth = 4096
#pragma HLS INTERFACE s_axilite port = in bundle = control
#pragma HLS INTERFACE s_axilite port = out bundle = control
#pragma HLS INTERFACE s_axilite port = length bundle = control
#pragma HLS INTERFACE s_axilite port = base_add bundle = control
#pragma HLS INTERFACE s_axilite port = return bundle = control

  for (int i = 0; i < length; ++i) {
#pragma HLS PIPELINE II = 1
    const uint32_t value = in[i];
    out[i] = value + base_add + static_cast<uint32_t>(i);
  }
}

}  // extern "C"
