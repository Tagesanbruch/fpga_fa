#include "fa_q8_8_linear.hpp"

extern "C" {

void fa_gemv_queue_kernel(const int16_t *x_all,
                          const int16_t *w_all,
                          int16_t *y_all,
                          const fpga::linear::GemvTaskDesc *tasks,
                          int task_count,
                          uint32_t *profile) {
#pragma HLS INTERFACE m_axi port=x_all offset=slave bundle=gmem0 depth=4096
#pragma HLS INTERFACE m_axi port=w_all offset=slave bundle=gmem1 depth=65536
#pragma HLS INTERFACE m_axi port=y_all offset=slave bundle=gmem2 depth=4096
#pragma HLS INTERFACE m_axi port=tasks offset=slave bundle=gmem3 depth=16
#pragma HLS INTERFACE m_axi port=profile offset=slave bundle=gmem4 depth=16

#pragma HLS INTERFACE s_axilite port=x_all bundle=control
#pragma HLS INTERFACE s_axilite port=w_all bundle=control
#pragma HLS INTERFACE s_axilite port=y_all bundle=control
#pragma HLS INTERFACE s_axilite port=tasks bundle=control
#pragma HLS INTERFACE s_axilite port=task_count bundle=control
#pragma HLS INTERFACE s_axilite port=profile bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

  fpga::linear::run_gemv_queue_tiled_hls(x_all, w_all, y_all, tasks, task_count, profile);
}

}  // extern "C"
