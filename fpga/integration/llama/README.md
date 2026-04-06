# Future llama.cpp Integration

This directory is intentionally a placeholder.

Planned direction:

- keep the XRT-native host/runtime as the only board-facing API
- add a thin bridge that marshals single-head attention buffers into the XRT runner
- integrate that bridge into a `llama.cpp` custom backend or a small adapter layer
- avoid adopting llama.cpp's existing OpenCL backend, since it targets GPUs rather than Xilinx FPGA kernels

Suggested order:

1. validate the standalone XRT runner on KV260
2. wire the same runtime into `inference/native`
3. add a `llama.cpp` proof-of-concept backend
