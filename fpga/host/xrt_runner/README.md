# XRT Native Runner

This is a minimal board-side CLI for launching the HLS attention kernel through
the native XRT C++ API.

Features:

- loads an `xclbin`
- allocates BOs for `Q/K/V/O/profile`
- fills random Q8.8 inputs
- launches the kernel
- optionally verifies the result with the shared strict Q8.8 reference

Example:

```bash
make
./fa_xrt_runner --xclbin /lib/firmware/xilinx/fa_attention_kernel.xclbin --seq-len 64 --verify
```
