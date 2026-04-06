# KV260 Server Environment Status

Date: `2026-04-06`

This note records the actual server-side state after moving the FPGA/XRT work
onto the Linux machine.

## Confirmed Toolchain

- `Vivado 2023.1`: `/home/luci/tools/Xilinx/Vivado/2023.1`
- `Vitis 2023.1`: `/home/luci/tools/Xilinx/Vitis/2023.1`
- `Vitis HLS 2023.1`: `/home/luci/tools/Xilinx/Vitis_HLS/2023.1`
- `v++` works after sourcing `/home/luci/tools/Xilinx/Vitis/2023.1/settings64.sh`
- `vitis_hls` works after sourcing `/home/luci/tools/Xilinx/Vitis_HLS/2023.1/settings64.sh`

## HLS Flow Status

- `make fpga-kernel-csim` already passes
- `make fpga-kernel-xo` now completes successfully
- generated artifact:
  - `fpga/hls/fa_attention_kernel/build/fa_attention_kernel.xo`
- synthesis summary from `csynth_design`:
  - estimated Fmax: `273.97 MHz`
  - loop constraints: not fully satisfied
- the `xo` export path needed several compatibility fixes for this exact Vitis HLS 2023.1 install:
  - `run_hls.tcl` previously called `open_project` with an absolute path
  - Vitis HLS 2023.1 rejects that usage because the project name cannot contain `/`
  - this build expects the Tcl file as a positional argument to `vitis_hls`, not a Vivado-style `-f` workflow
  - `export_design` in this version uses `-output`, not `-xo_path`
  - the script now changes into `build/` first, then opens `fa_attention_kernel_hls`

## KV260 Platform Status

No `kv260*.xpfm` was found under:

- `/home/luci/tools/Xilinx`
- `/opt`
- `/3.2T/work`

Installed Vitis base platforms currently visible on this machine:

- `xilinx_zcu102_base_202310_1`
- `xilinx_zcu102_base_dfx_202310_1`
- `xilinx_zcu104_base_202310_1`
- `xilinx_vck190_base_202310_1`
- `xilinx_vck190_base_dfx_202310_1`
- `xilinx_vmk180_base_202310_1`
- `xilinx_vek280_es1_base_202310_1`

Impact:

- `fpga/vitis/kv260/build_xclbin.sh` cannot complete until a KV260 acceleration
  platform is installed or `KV260_PLATFORM` is pointed at one manually

## XRT Development Package Status

No XRT development headers or libraries were found under:

- `/usr`
- `/opt`
- `/home/luci`

Specifically missing:

- `xrt/xrt_bo.h` or `experimental/xrt_bo.h`
- `libxrt_coreutil.so`

Impact:

- `fpga/host/xrt_runner` cannot currently be built on this machine
- board-side runtime may still exist elsewhere, but the host-side development
  package is not present in the current server image

## Legacy KV260 Vivado Project Facts

The historical project at `/3.2T/work/flash_attn/kv260-fa` confirms the
existing RTL integration used:

- device: `xck26-sfvc784-2LV-c`
- one custom module-ref IP: `fa_attention_ip_top_v`
- one control interconnect: `smartconnect_ctrl`
- one data interconnect: `smartconnect_data`
- one PS block: `zynq_ultra_ps_e_0`
- common PL clock/reset: `pl_clk0`, `pl_resetn0`
- data path: custom `m_axi` -> `smartconnect_data` -> `S_AXI_HPC0_FPD`
- control path: PS `M_AXI_HPM0_FPD` / `M_AXI_HPM1_FPD` -> `smartconnect_ctrl`

This gives us the minimum board-level topology if the prebuilt-platform route
stalls and we have to fall back to a custom platform.

## Current Practical Conclusion

The repository is now in a better place mechanically:

- HLS environment loading is scripted
- `xo` generation no longer depends on manual `source`
- missing KV260 platform and missing XRT dev files now fail early with explicit messages

But the XRT deployment line is still blocked by two external dependencies:

1. a KV260 acceleration platform (`.xpfm`)
2. an XRT development installation for compiling the native host runner
