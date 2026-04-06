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
- XRT development headers/libraries available when building `fpga/host/xrt_runner`

Typical flow:

1. `make -C fpga/hls/fa_attention_kernel csim`
2. `make -C fpga/hls/fa_attention_kernel xo`
3. `fpga/vitis/kv260/build_xclbin.sh`
4. Copy `fpga/build/kv260/fa_attention_kernel.xclbin` to the board
5. Build and run `fpga/host/xrt_runner`

Current server gap on `2026-04-06`:

- no `kv260*.xpfm` was found under `/home/luci/tools/Xilinx` or `/3.2T/work`
- installed base platforms are limited to `zcu102/zcu104/vck190/vmk180/vek280`
- `build_xclbin.sh` will stop until a KV260 acceleration platform is installed or pointed to explicitly
