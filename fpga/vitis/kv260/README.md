# KV260 Vitis Link Notes

This directory assumes a prebuilt KV260 acceleration platform already exists.
It intentionally avoids:

- creating a custom Vivado project
- maintaining a Block Design in this repository
- exporting a custom XSA/platform from local sources

Operational assumptions:

- Xilinx tools are available only on the remote build server
- local macOS work is limited to source edits and standard C++ validation
- if a local Python helper is added later, it should be run through the repo's `uv` environment

Expected environment:

- `KV260_PLATFORM=/abs/path/to/kv260*.xpfm`
- `vitis_hls` and `v++` available on `PATH`
- XRT installed on the target board

Typical flow:

1. `make -C fpga/hls/fa_attention_kernel csim`
2. `fpga/vitis/kv260/build_xclbin.sh`
3. Copy `fpga/build/kv260/fa_attention_kernel.xclbin` to the board
4. Build and run `fpga/host/xrt_runner`
