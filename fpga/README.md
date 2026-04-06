# FPGA XRT Scaffold

This directory contains the XRT-native bring-up scaffold for KV260 using a
prebuilt platform. The working assumptions are:

- no hand-written Block Design
- no Vivado GUI flow
- no custom platform maintenance in this repository
- HLS kernel + `v++` link + XRT host as the primary path
- all Xilinx toolchains live on the remote server, not on the local macOS host
- if a local Python helper is needed later, it should use the repository-managed `uv` environment

Directory layout:

- `common/`: shared fixed-point helpers and the strict Q8.8 attention reference
- `hls/fa_attention_kernel/`: single-kernel HLS implementation and local csim
- `vitis/kv260/`: `v++` config and xclbin build entrypoints
- `host/xrt_runner/`: minimal XRT native host CLI
- `remote/`: sync/build/artifact helper scripts for the remote Xilinx machine
- `integration/`: placeholders for future `inference/native` and `llama.cpp` bridges

Recommended first steps:

1. `make fpga-kernel-csim`
2. Set `KV260_PLATFORM=/abs/path/to/kv260.xpfm`
3. Run `fpga/vitis/kv260/build_xclbin.sh`
4. Copy the generated `xclbin` and build `fpga/host/xrt_runner`
