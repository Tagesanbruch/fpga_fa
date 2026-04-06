# llama.cpp Integration Runtime

This directory now contains the queue-oriented runtime shared by:

- the standalone FPGA/XRT runner path
- the `ggml-fpga` backend inside `inference/xcomp/llama.cpp`

Current scope:

- queue host-side attention tasks and drain them through either:
  - software HLS reference (`run_attention_tiled_hls`)
  - strict Q8.8 reference (`run_attention_strict`)
  - XRT (`fa_attention_kernel`) when compiled with XRT enabled
- keep the board-facing ABI aligned with the HLS kernel:
  - `seq_len == kv_len`
  - `D = 64`
  - `seq_len <= 256`
  - dense Q8.8 buffers

Current limitation:

- this runtime only accelerates square single-block attention today
- decode-style `q_len != kv_len`, arbitrary masks, sinks, and softcap still fall back to CPU in llama.cpp

Relevant entrypoints:

- `fa_task_queue_runtime.hpp`
- `fa_task_queue_runtime.cpp`
- `inference/xcomp/llama.cpp/ggml/src/ggml-fpga/ggml-fpga.cpp`
