# Future `inference/native` Integration

Planned immediate follow-up:

1. keep `fpga/host/xrt_runner` as the kernel-facing runtime
2. factor out a small reusable XRT helper from that CLI
3. call that helper from a new `inference/native` attention path
4. preserve the current CPU/Q8.8 path as the software fallback

This directory is a placeholder so the native bridge can land without mixing
XRT-specific code directly into the HLS kernel directory.
