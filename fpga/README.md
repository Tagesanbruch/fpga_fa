# FPGA XRT Scaffold

This directory contains the XRT-native bring-up scaffold for KV260 using a
prebuilt platform. The working assumptions are:

- no hand-written Block Design
- no Vivado GUI flow
- no custom platform maintenance in this repository
- HLS kernel + `v++` link + XRT host as the primary path
- all Xilinx toolchains live on the remote server, not on the local macOS host
- if a local Python helper is needed later, it should use the repository-managed `uv` environment

Current server status on `2026-04-06`:

- `Vivado/Vitis/Vitis_HLS 2023.1` are installed under `/home/luci/tools/Xilinx`
- the HLS `xo` flow can be driven locally from this repository
- the installed Vitis base platforms include `zcu102/zcu104/vck190/vmk180/vek280`, but not `kv260`
- XRT development headers/libraries were not found under `/usr`, `/opt`, or `/home/luci/tools/Xilinx`

Directory layout:

- `common/`: shared fixed-point helpers and the strict Q8.8 attention reference
- `hls/fa_attention_kernel/`: single-kernel HLS implementation and local csim
- `platforms/kv260/`: repository-local KV260 platform workspace (`xsa`/`xpfm` preparation)
- `vitis/kv260/`: `v++` config and xclbin build entrypoints
- `host/xrt_runner/`: minimal XRT native host CLI
- `remote/`: sync/build/artifact helper scripts for the remote Xilinx machine
- `integration/`: placeholders for future `inference/native` and `llama.cpp` bridges

Recommended first steps:

1. `make fpga-kernel-csim`
2. `make fpga-kernel-xo`
3. Set `KV260_PLATFORM=/abs/path/to/kv260.xpfm`
4. Run `fpga/vitis/kv260/build_xclbin.sh`
5. Copy the generated `xclbin` and build `fpga/host/xrt_runner`
