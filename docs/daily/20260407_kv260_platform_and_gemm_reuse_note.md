# 2026-04-07 KV260 Platform Generation and GEMM/GEMV Reuse Note

## Summary

This note answers three practical questions:

1. Does `xpfm` have to be created manually in Vitis/Vivado GUI?
2. If not, what project or script can be used as the starting point?
3. With LUT reduced to the current level, should we reuse the attention dot-product datapath for GEMV/GEMM, and how should that be done efficiently?

Short answers:

- `xpfm` does **not** have to be created manually through the GUI.
- We **can** generate the hardware handoff (`.xsa`) and then the platform (`.xpfm`) through Tcl / scripted flows.
- The most directly reusable part is **not the whole attention kernel**, but the **dot-product / MAC micro-kernel** inside attention.
- For efficiency, GEMV/GEMM should reuse the **lane-level MAC datapath** and local buffering strategy, but keep **separate outer control/dataflow** from FlashAttention.

## 1. Current Platform Assets in the Workspace

A search across `/3.2T/work` shows:

- no existing KV260 `.xpfm`
- no existing KV260 `.xsa`
- but there **is** an existing KV260 Vivado project:

  - [kv260-fa.xpr](/3.2T/work/flash_attn/kv260-fa/kv260-fa.xpr)

This is the most useful current starting point if we want to inspect or export the hardware platform.

Additional evidence that this Vivado project is board-relevant:

- it contains a Block Design artifact:
  - `/3.2T/work/flash_attn/kv260-fa/kv260-fa.ip_user_files/mem_init_files/design_fa.bda`
- and multiple synthesized BD components for:
  - `fa_attention_ip_top`
  - `smartconnect_ctrl`
  - `smartconnect_data`
  - `zynq_ultra_ps_e`

So the project is already very close to the "board shell + IP integration" side we need.

## 2. Does `xpfm` Need Manual GUI Creation?

No. The GUI is **not required**.

There are two separable steps:

1. **Vivado side**
   - open or create the board hardware design
   - generate bitstream if needed
   - export hardware handoff as `.xsa`

2. **Vitis side**
   - consume:
     - `.xsa`
     - `xilinx-zynqmp-common-v2023.1`
   - generate the embedded platform / platform component
   - produce the usable platform (`.xpfm`)

Both steps can be automated.

### 2.1 Vivado side can be scripted

There are two practical paths:

#### Path A: Use the existing KV260 project

Open:

- [kv260-fa.xpr](/3.2T/work/flash_attn/kv260-fa/kv260-fa.xpr)

Then either:

- export hardware manually once, or
- use Tcl in that project to generate/export an `.xsa`

This is the lowest-risk path if the project already matches the board integration we want.

#### Path B: Regenerate the board design from Tcl

This is cleaner long-term, but only if we are ready to fully script the platform shell.

Given the current state, Path A is the faster route to first `xsa`.

### 2.2 Vitis platform creation can also be scripted

The tutorial uses GUI workflow, but conceptually the inputs are just:

- board hardware handoff (`.xsa`)
- common image (`Image`, `rootfs.ext4`, sysroot from `sdk.sh`)

So the GUI is not the real requirement; it is just one front-end for building the platform component.

## 3. What Still Needs to Be Downloaded / Prepared

Current status:

- board runtime is already usable
- Vivado/Vitis toolchains are already installed
- missing platform artifacts are:
  - KV260 `.xsa`
  - KV260 `.xpfm`

### 3.1 Common image package

Still needed:

- `xilinx-zynqmp-common-v2023.1_05080224.tar.gz`

This is **not** the platform itself. It provides:

- `Image`
- `rootfs.ext4`
- `sdk.sh`
- sysroots / boot assets

It is one input for platform creation.

### 3.2 Current copy progress

Current file found:

- `third_party/xilinx-zynqmp-common-v2023.1_05080224.tar.gz`

Current size check:

- actual current file size: `653M`
- expected package size (per user note): `2.34 GB`

Conclusion:

- the copy is **still in progress / not complete yet**
- do **not** run MD5 verification yet

Expected MD5 after copy completes:

- `2fd2964c0bf6e6ebd10132b5a1352220`

## 4. Practical Platform Generation Plan

The shortest practical route is:

1. Wait for the common image tar to finish copying.
2. Open or script-export hardware from:
   - [kv260-fa.xpr](/3.2T/work/flash_attn/kv260-fa/kv260-fa.xpr)
3. Produce a KV260 `.xsa`
4. Unpack the common image tar
5. Run `sdk.sh` to install the sysroot
6. Create the KV260 custom platform (`.xpfm`) using:
   - `.xsa`
   - `Image`
   - `rootfs.ext4`
   - installed sysroot
7. Run:
   - [build_xclbin.sh](/3.2T/work/fpga-fa/fpga/vitis/kv260/build_xclbin.sh)

## 5. GEMM/GEMV Reuse: Clarifying the Meaning of "Reuse"

The important correction is:

- the intended reuse is **not** "instantiate another separate compute block next to attention"
- the intended reuse is:
  - the attention kernel already contains dot-product work
  - can that same datapath be abstracted and reused for GEMV/GEMM?

That interpretation is correct.

## 6. Is Reuse Reasonable After LUT Reduction?

Yes, **micro-kernel reuse is now more reasonable than before**, but only in a constrained sense.

Why it is more reasonable now:

- current HLS top resources are around:
  - `LUT 43379 (37%)`
  - `DSP 353 (28%)`
  - `BRAM 165 (57%)`
- compared with the earlier `~87% LUT` stage, this is much healthier
- there is now enough headroom to consider extracting and repurposing a shared arithmetic core

Why it is still not trivial:

- BRAM is already above half (`57%`)
- current attention latency is still far from RTL parity
- the bottleneck is not just arithmetic count, but control/dataflow structure

So:

- **reusing the arithmetic kernel makes sense**
- **reusing the whole attention HLS kernel for GEMV/GEMM does not**

## 7. What Part of Attention Is Actually Reusable?

The reusable parts are:

1. **Q·K row dot-product slice**
   - current HLS leaf:
     - `qk_dotprod_slice_pair`
   - conceptually this is already a lane-parallel inner-product engine

2. **P·V accumulation style MAC datapath**
   - the accumulation path in `online_softmax_ctx_acc_row_step`
   - not reusable as-is for GEMV/GEMM control, but the MAC lane structure is relevant

3. **Local tile buffering / row-stationary streaming**
   - the way Q/K/V local data is staged maps naturally to vector/matrix streaming

## 8. What Is Not Reusable As-Is

These should **not** be forced into GEMV/GEMM reuse directly:

- online softmax context machinery
- causal masking
- reciprocal / normalize path
- score packet / retire logic
- attention-specific batch/context scheduling

Those are attention-specific control structures.

## 9. How to Reuse the Dot-Product Datapath Efficiently

The efficient path is:

### 9.1 Reuse the inner MAC slice, not the whole top-level kernel

Create a parameterized arithmetic micro-kernel that exposes:

- vector input stream / local buffer
- matrix row stream
- lane-parallel MAC reduction
- configurable accumulation width

Then specialize it into:

- attention Q·K
- GEMV
- future small-tiled GEMM inner products

### 9.2 GEMV is the most natural first reuse target

GEMV maps very naturally to the current attention dot-product style:

- broadcast one vector
- stream matrix rows
- compute row-wise dot products

This is close to what Q·K already does.

So the first reuse target should be:

- **GEMV**, not full GEMM

### 9.3 GEMM needs a different outer schedule

GEMM can still reuse the lane MAC datapath, but needs:

- blocked tile scheduling
- either output-stationary or row/col-stationary orchestration
- more aggressive local buffering
- likely different read/write traffic balance

So for GEMM:

- the **MAC lanes** can be shared
- the **controller/dataflow shell** should remain separate

## 10. Recommended Architecture Direction

The recommended next architecture is:

1. keep `fa_attention_kernel` as the attention-specific top
2. factor out a shared low-level dot/MAC building block
3. build a separate `fa_gemv_kernel` on top of that shared micro-kernel
4. only after GEMV is stable, evaluate whether GEMM should be built from:
   - repeated GEMV-like tiles, or
   - a dedicated tiled GEMM shell

This gives real reuse without forcing a poor top-level fusion.

## 11. Recommendation

Near-term priority should still be:

1. finish common image copy
2. export KV260 `.xsa`
3. build KV260 `.xpfm`
4. get the attention operator running on board

Only after the board path is unblocked should we invest more effort in:

- shared arithmetic-core refactoring for GEMV/GEMM

Because at that point we will know whether the remaining bottleneck is:

- arithmetic throughput
- DDR bandwidth
- or launch/control overhead

That measurement will determine how far the reuse effort is worth pushing.
