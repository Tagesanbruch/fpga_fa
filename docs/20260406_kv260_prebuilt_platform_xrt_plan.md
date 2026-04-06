# KV260 Prebuilt Platform + XRT Native Plan

This file records the implementation direction agreed during planning:

- use a prebuilt KV260 acceleration platform
- avoid hand-written BD and Vivado GUI work
- use a single HLS attention kernel
- use `v++` to produce `xo/xclbin`
- use XRT native C++ as the host/runtime API
- do not use OpenCL as the primary software interface
- assume Xilinx tools live only on the remote server
- keep local helper scripts Python-free unless they are explicitly wired into the repo-managed `uv` environment

Repository landing zones:

- `fpga/common/`
- `fpga/hls/fa_attention_kernel/`
- `fpga/vitis/kv260/`
- `fpga/host/xrt_runner/`
- `fpga/remote/`
- `fpga/integration/llama/`

Immediate deliverables:

1. a strict-Q8.8 HLS kernel aligned to `cmodel` semantics
2. local csim against the existing strict reference
3. a `v++` link path for KV260 using an existing platform
4. a minimal XRT board-side runner
