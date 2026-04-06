# 2026-04-06 GEMV/GEMM Exploration Report

## Summary

This iteration explored whether GEMV/GEMM should be merged into the existing HLS FlashAttention kernel or developed as a separate module.

Conclusion:

- **Do not merge GEMV/GEMM directly into the current attention HLS kernel.**
- **A separate GEMV kernel is the right next step.**
- GEMM should be treated as a later extension built on top of a cleaner linear-algebra path, rather than being forced into the current attention datapath.

The reason is straightforward:

- the current HLS attention kernel is already LUT-heavy (`102052 LUT`, `87%`)
- its latency is still far from the RTL mainline target
- mixing GEMV/GEMM logic into the same kernel would increase control complexity, memory muxing, and integration risk before the attention path is board-proven

A standalone Q8.8 GEMV HLS experiment kernel was therefore implemented, validated, synthesized, and exported to XO.

## Why not merge into the current attention kernel

### 1. Resource headroom is not actually comfortable

Current attention HLS synthesis result:

- BRAM: `115 (39%)`
- DSP: `96 (7%)`
- FF: `73490 (31%)`
- LUT: `102052 (87%)`
- Fmax: `273.97 MHz`

This does **not** describe a kernel with enough logic headroom for additional GEMV/GEMM scheduling, descriptor parsing, or shared compute-path multiplexing.

### 2. Architectural reuse is only partial

What can be reused conceptually:

- Q8.8 multiply-accumulate behavior
- DSP-oriented inner product structure
- XRT/kernel ABI style
- queue-oriented host runtime idea

What does **not** reuse cleanly in the current attention kernel:

- online softmax state
- row-wise `m/l/acc` context
- normalize path
- tile scheduling around `Q/K/V`
- causal/mask-specific control logic

So the real reusable part is the **dot/MAC engine style**, not the whole attention HLS kernel.

### 3. System risk

If GEMV/GEMM is forced into the current attention kernel now, the likely outcome is:

- higher LUT usage
- more complicated AXI access patterns
- worse HLS scheduling behavior
- harder debug path for both attention and linear layers

That is not a good trade before the current attention XRT/KV260 path is fully closed on-board.

## Implemented experiment: standalone GEMV HLS kernel

Added shared Q8.8 linear math support:

- `fpga/common/fa_q8_8_linear.hpp`
- `fpga/common/fa_q8_8_linear.cpp`

Added standalone GEMV HLS kernel:

- `fpga/hls/fa_gemv_kernel/fa_gemv_kernel.cpp`
- `fpga/hls/fa_gemv_kernel/tb/test_fa_gemv_kernel.cpp`
- `fpga/hls/fa_gemv_kernel/Makefile`
- `fpga/hls/fa_gemv_kernel/run_hls.tcl`

Added root convenience targets:

- `make fpga-gemv-csim`
- `make fpga-gemv-xo`

## GEMV kernel definition

Current kernel interface:

- input vector `x` (Q8.8)
- input matrix `w` (row-major, Q8.8)
- output vector `y` (Q8.8)
- scalar args: `in_dim`, `out_dim`
- optional `profile`

Current design choices:

- local buffered input vector (`x_local`)
- tiled output rows with `kTileOut = 32`
- 8-way inner-loop unroll for MACs
- DSP binding on the inner product multiply

## Numerical validation

Command:

```bash
make fpga-gemv-csim
```

Result:

```text
[CASE] small_uniform_64x64 exact=yes mae=0.002550 maxae=0.005814 macs=4096
[CASE] rect_uniform_64x128 exact=yes mae=0.002183 maxae=0.005768 macs=8192
[CASE] rect_gaussian_64x128 exact=yes mae=0.002392 maxae=0.005768 macs=8192
[CASE] wide_uniform_128x64 exact=yes mae=0.002608 maxae=0.005554 macs=8192
[CASE] wide_gaussian_128x128 exact=yes mae=0.002645 maxae=0.005844 macs=16384
[CASE] max_uniform_256x256 exact=yes mae=0.002351 maxae=0.005768 macs=65536
[SUMMARY] exact_vs_strict=PASS worst_case=wide_gaussian_128x128 worst_mae=0.002645 worst_maxae=0.005844
```

Interpretation:

- HLS-style GEMV path is **bit-exact** to the strict fixed-point reference in all tested cases
- FP32 error is low and stable
- the Q8.8 linear path itself is healthy enough for further hardware exploration

## HLS synthesis result

Command:

```bash
make fpga-gemv-xo
```

Generated artifact:

- `fpga/hls/fa_gemv_kernel/build/fa_gemv_kernel.xo`

XO size:

- GEMV XO: `428401 bytes`
- Attention XO: `2750804 bytes`

### GEMV HLS synthesis summary

From `fpga/hls/fa_gemv_kernel/build/fa_gemv_kernel_hls/solution1/syn/report/csynth.rpt`:

- BRAM: `36 (12%)`
- DSP: `15 (1%)`
- FF: `7183 (3%)`
- LUT: `9419 (8%)`
- Fmax: `273.97 MHz`

Inner compute kernel:

- `run_gemv_tiled_hls_Pipeline_VITIS_LOOP_44_2`
- Final II: `8`
- Depth: `10`
- DSP in this pipelined inner loop: `8`

## Comparison with attention HLS kernel

### Attention kernel

- BRAM: `115 (39%)`
- DSP: `96 (7%)`
- FF: `73490 (31%)`
- LUT: `102052 (87%)`
- Fmax: `273.97 MHz`

### GEMV kernel

- BRAM: `36 (12%)`
- DSP: `15 (1%)`
- FF: `7183 (3%)`
- LUT: `9419 (8%)`
- Fmax: `273.97 MHz`

### Practical takeaway

This comparison strongly supports a **multi-kernel strategy**:

- keep FlashAttention as its own specialized kernel
- add GEMV as a separate kernel
- potentially add GEMM later as another dedicated kernel
- unify them at the runtime / queue / host API level instead of physically merging them into one HLS kernel

## What the GEMV synthesis tells us

### Positive

- Very small LUT footprint relative to attention
- Very small BRAM footprint relative to attention
- Bit-exact vs strict Q8.8 reference
- Clean kernel ABI for later XRT integration

### Negative / current bottleneck

The main loop still lands at `II = 8`, not `II = 1`.

Primary reasons visible from HLS report:

- carried dependence on the scalar accumulator `acc`
- matrix reads from `gmem1` do not form a clean fully-aligned streaming pattern for HLS
- outer/inner loop structure is still closer to a simple dot-product kernel than a more aggressive matrix engine

So this kernel is a **good experiment baseline**, but not yet a high-throughput production GEMV engine.

## What this implies for GEMM

GEMM should not be approached by stretching this current GEMV kernel into a monolithic mega-kernel immediately.

A better sequence is:

1. stabilize standalone GEMV kernel architecture
2. improve GEMV II / memory schedule
3. decide whether GEMM should be:
   - a batched GEMV-style kernel, or
   - a separate tiled/systolic matrix kernel

Given the current HLS evidence, the more realistic next step is:

- **separate GEMM module**, not “merge GEMM into attention”

## Recommendation

### Short-term

- keep current attention HLS kernel independent
- keep current GEMV experiment kernel independent
- connect both through the shared host/runtime layer later

### Mid-term

- improve GEMV memory access pattern and II
- explore whether `x_local` + weight tiling should become a more regular small matrix engine
- then decide whether GEMM deserves a dedicated HLS kernel

### Integration direction

For llama.cpp / runtime integration, the clean architecture is:

- `fa_attention_kernel` for supported flash attention cases
- `fa_gemv_kernel` for selected linear layers / matvec-heavy stages
- a queue-capable host runtime dispatching to different kernels

That is much safer than forcing all three into one HLS block.

## Next Steps

1. Add an XRT host runner for `fa_gemv_kernel`.
2. Improve GEMV II by reducing accumulator dependence and improving weight memory layout.
3. Decide whether to prototype GEMM as:
   - tiled batched GEMV, or
   - a new small systolic-style kernel.
4. Only after that, decide if llama.cpp should offload selected GEMV-heavy paths in addition to flash attention.
