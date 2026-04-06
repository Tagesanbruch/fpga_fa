# HLS <-> CModel Alignment Notes

This note explains how the new HLS kernel maps to the existing software golden model.

## Reference Source

The kernel is intentionally aligned to:

- `cmodel/csrc/attention_kernels.cpp`
- `attn::online_rtl_like(...)`
- `Mode::RTL_STRICT`

It is not aligned to:

- AXI-Lite register semantics in `rtl/top/fa_attention_ip_top.sv`
- DMA FSM behavior
- cycle-accurate top-level counters

## Matching Rules

The HLS kernel keeps the following behaviors identical to the strict reference:

1. `Q/K/V/O` are all `Q8.8` (`int16_t`)
2. `D=64`, `TQ=32`, `TK=64`
3. score scaling uses the caller-provided `scale_q8_8`
4. causal masking uses `neg_large_q8_8`
5. exponent uses `exp_pwl_q1_15`
6. reciprocal uses `recip_nr_rtl_q16_16`
7. row accumulation follows the strict `row_acc_rtl` update path
8. normalization uses the same rounded Q32.32 -> Q8.8 conversion path

## Deliberate Differences

These parts are intentionally *not* preserved from the RTL top:

- no AXI-Lite register file
- no separate DMA reader/writer modules
- no performance counters tied to bus handshakes
- no requirement for cycle-by-cycle equivalence

The kernel only preserves algorithmic and fixed-point behavior.

## Validation Contract

The intended validation order is:

1. local csim: HLS kernel vs `Mode::RTL_STRICT`
2. HLS export/link: build `xo/xclbin`
3. XRT board run: kernel output vs shared strict reference

If csim passes but board output diverges, the first debugging target should be
buffer layout, stride handling, or XRT BO synchronization, not the algorithm.

## Current Regression Scope

The current local csim regression uses sequence lengths that are divisible by
both `TQ=32` and `TK=64`:

- `64`
- `128`
- `256`

This is deliberate, because the existing `cmodel` strict path is the cleanest
golden source for those sizes. The HLS kernel itself accepts arbitrary
`seq_len <= 256`; if partial-tile coverage becomes important later, add a
second regression path with a standalone partial-tile reference instead of
changing the current strict-alignment contract.
