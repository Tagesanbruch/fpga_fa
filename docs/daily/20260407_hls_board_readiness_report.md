# 2026-04-07 HLS Resource Recheck and KV260 Board Readiness

## Summary

This round rechecked the post-optimization HLS reports and board-side runtime status.

Key conclusions:

- The current HLS kernel **did reduce LUT usage substantially**, but the real number is **`43379 LUT (37%)`**, not "about 30%".
- Functional behavior remains stable:
  - `make fpga-kernel-csim` still passes.
  - Worst-case accuracy remains:
    - `MAE=0.002854`
    - `MaxAE=0.045567`
- The KV260 board already has the runtime stack needed for an XRT-based operator test:
  - Ubuntu 22.04
  - `xmutil`
  - `xrt`
  - `zocl`
  - board-side XRT libraries and headers
- The immediate blocker for running the HLS operator on board is still **xclbin generation**:
  - no `kv260*.xpfm` was found in the current server workspace/tool install
  - no `fa_attention_kernel.xclbin` exists yet

## 1. HLS Resource Recheck

Source:

- [csynth.rpt](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/build/fa_attention_kernel_hls/solution1/syn/report/csynth.rpt)

Current top-level resource summary:

- `BRAM 165 (57%)`
- `DSP 353 (28%)`
- `FF 43372 (18%)`
- `LUT 43379 (37%)`
- Estimated latency: `786727 cycles`
- Target clock: `6.765 ns` effective due to negative slack
- Slack: `-3.11 ns`

Interpretation:

- This is a large improvement from the earlier `~87% LUT` stage.
- It is **not yet ~30% LUT**. On this device, `43379 LUT` maps to **37%**.
- The gain appears real and is reflected directly in the current synthesis report, not only in notes.

## 2. Functional / Accuracy Regression Check

Command:

```bash
make fpga-kernel-csim
```

Result:

- PASS
- All 7 regression cases still match the current RTL-main-aligned reference.

Worst case remains:

- case: `gaussian_s256_causal`
- `MAE=0.002854`
- `MaxAE=0.045567`

Regression summary:

- `rtl_main_aligned=PASS`
- `worst_hls_vs_fp32_case=gaussian_s256_causal`
- `worst_host_ms=12.931441`

Interpretation:

- The LUT reduction did **not** introduce a visible functional regression in the current HLS C-level regression suite.
- Numerically the design stayed on the same "main-aligned" track as before this resource drop.

## 3. Performance Status

The current HLS top-level report shows:

- total latency: `786727 cycles`

This is better than the earlier multi-million-cycle stage, but still far from the RTL target reported in the project notes:

- RTL reference level: `85928 cycles`

So the current gap is still approximately:

- `786727 / 85928 ~= 9.16x`

Conclusion:

- Resource reduction is real.
- Functional correctness is preserved.
- System latency is still much too high for "RTL-level parity".

## 4. KV260 Board Runtime Readiness

Board-side facts from the latest user-provided shell output:

- OS: `Ubuntu 22.04.4 LTS`
- `xmutil` present
- `xrt` package installed
- `zocl` kernel module loaded
- `libxrt_coreutil.so` present
- XRT headers present under `/usr/include/xrt`
- existing firmware app directories already present:
  - `/lib/firmware/xilinx/kv260_llm_cl`
  - `/lib/firmware/xilinx/swan`

This means the board is already suitable for a minimal XRT kernel launch flow once we have:

1. a valid `fa_attention_kernel.xclbin`
2. a runnable board-side host binary

## 5. Board-Side Issue Observed

`xbutil examine` currently fails with:

```text
symbol lookup error: /usr/bin/unwrapped/xbutil2: undefined symbol: _ZN8xrt_core9mem_writeEPKNS_6deviceExxj
```

Interpretation:

- `xbutil` on the board appears to be mismatched against the installed XRT runtime libraries.
- This is inconvenient for diagnostics, but it does **not** necessarily block:
  - `xrt::device`
  - `xrt::xclbin`
  - `device.load_xclbin(...)`
  - direct kernel launches from our native XRT runner

So `xbutil` being broken is not currently the main blocker.

## 6. Current Blocking Item for Board Inference

Server-side checks still show:

- no `kv260*.xpfm` found in the current workspace or installed Xilinx base platforms
- no generated `fa_attention_kernel.xclbin`

Current repository flow assumes:

- [build_xclbin.sh](/3.2T/work/fpga-fa/fpga/vitis/kv260/build_xclbin.sh)
- `KV260_PLATFORM=/abs/path/to/kv260*.xpfm`

But that platform is still missing.

Therefore the current operator-on-board path is blocked by:

- **missing KV260 acceleration platform for `v++ --link`**

not by:

- HLS csim
- board XRT runtime
- board firmware loading capability

## 7. Practical Next Step

The shortest path to a real board test is:

1. Obtain or reconstruct a usable KV260 acceleration platform (`.xpfm`)
2. Run:

```bash
fpga/vitis/kv260/build_xclbin.sh
```

3. Transfer the generated `fa_attention_kernel.xclbin` to the board
4. Build `fpga/host/xrt_runner` natively on the board
5. Run:

```bash
./fa_xrt_runner --xclbin /path/to/fa_attention_kernel.xclbin --seq-len 64 --verify
```

## 8. Recommendation

At this stage, the HLS kernel has reached a reasonable "board bring-up candidate" state:

- LUT is much lower than before
- accuracy remains stable
- interface is already XRT-native

So the next milestone should be:

- **board execution of the operator**

rather than more speculative HLS restructuring before first hardware bring-up.
