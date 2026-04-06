# 2026-04-06 Verilator / cocotb Validation Report

## Summary

This update completed the local Verilator toolchain bring-up using `third_party/verilator`, switched the repository's default Verilator entrypoints to prefer the locally built binary, and re-enabled cocotb regression on the server with high parallelism.

Key outcomes:
- Built a local Verilator successfully from `third_party/verilator`.
- Verified local version: `Verilator 5.047 devel rev v5.046-233-g0df0064d6`.
- Updated the root `Makefile` to prefer the local Verilator binary and propagate `96` build jobs.
- Disabled `ccache` in Verilator sub-builds to avoid the server's `/run/user/1000` write restriction.
- Re-ran cocotb successfully on:
  - `fa_online_softmax_ctx`
  - `fa_attention_core_full`
- Re-ran direct Verilator C++ testbench successfully, but it exposed an existing RTL-vs-FP32 threshold failure rather than a toolchain problem.

## Changes Made

### 1. Local Verilator build
Built from:
- `third_party/verilator`

Configuration:
- `./configure --prefix=$PWD/install`
- `make OBJCACHE= -j96 opt`

The build succeeded after bypassing top-level documentation/manpage generation and explicitly disabling `ccache`.

### 2. Makefile integration
Updated [Makefile](/3.2T/work/fpga-fa/Makefile) to:
- Prefer `third_party/verilator/bin/verilator` when present.
- Add `VERILATOR_MAKE_JOBS ?= 96`.
- Export a shared simulation environment with:
  - local Verilator at the front of `PATH`
  - `MAKEFLAGS=-j96`
  - `OBJCACHE=`
- Use `$(VERILATOR_BIN)` consistently for lint and direct Verilator build targets.
- Pass `-MAKEFLAGS "-j96 OBJCACHE="` to direct Verilator `--build` flow.
- Fix the `lint` target's missing RTL source list for `fa_attention_core` / `fa_attention_ip_top`.

## Validation Results

### A. Verilator version check
Command:
```bash
PATH=/3.2T/work/fpga-fa/third_party/verilator/bin:$PATH \
  /3.2T/work/fpga-fa/third_party/verilator/bin/verilator --version
```

Result:
```text
Verilator 5.047 devel rev v5.046-233-g0df0064d6
```

### B. cocotb unit test: `fa_online_softmax_ctx`
Command:
```bash
make test MODULE=fa_online_softmax_ctx WAVES=0
```

Result:
- Passed.
- cocotb confirmed it was running on local Verilator 5.047.
- Key lines:
```text
Running on Verilator version 5.047 devel
TESTS=1 PASS=1 FAIL=0
SIM TIME = 102 ns
```

This validates the basic cocotb + VPI + local Verilator path.

### C. cocotb datapath test: `fa_attention_core_full`
Command:
```bash
make test MODULE=fa_attention_core_full WAVES=0
```

Result:
- Passed.
- End-to-end DMA-backed attention core regression completed successfully.
- Key lines:
```text
Core done after 85834 cycles
RTL vs fixed-like: MAX_AE=5, MAE=1.6207, count=16384
RTL vs FP32: MAX_AE=0.018949, MAE=0.005012, count=16384
test_attention_core_small PASS
TESTS=1 PASS=1 FAIL=0
```

Observations:
- The measured core latency `85834 cycles` is aligned with the existing ~`85.9k` cycle expectation discussed in the RTL reports.
- The RTL-vs-FP32 metrics are within the cocotb test's acceptance.

### D. Direct Verilator C++ testbench
Command:
```bash
make check-sdpa-verilator-cpp
```

Result:
- Toolchain path succeeded: Verilator elaboration, C++ compilation, and binary execution all completed.
- The run failed on an existing numeric threshold inside the testbench, not on build infrastructure.

Key output:
```text
[Verilator C++ TB] DONE cycles=85808 o_cycles=85808
[Verilator C++ TB] PERF summary: busy=85807 load_q=2054 init=8 load_k=4104 load_v=4104 compute=67584 norm=5888 write_o=2056 next_q=8
[Verilator C++ TB] PERF compute split: dp=66560 score=32768 softmax=32768 comp_launch=64 recip_req=256 recip_rsp=256
[Verilator C++ TB] RTL vs FixedLike: MAE=2.887756 MAX_AE=56.000000 @(0,36)
[Verilator C++ TB] FixedLike vs FP32: MAE=0.002766 MAX_AE=0.006155 @(2,7)
[Verilator C++ TB] RTL vs FP32: MAE=0.010767 MAX_AE=0.214844 @(0,36)
[Verilator C++ TB] FP32 thresholds: MAE<=0.03 PASS, MAX_AE<=0.10 FAIL
```

Interpretation:
- This is now a valid RTL investigation signal.
- The build and runtime infrastructure are no longer the blocker.
- The next step for this path is to inspect why the direct C++ TB still sees `RTL vs FP32 MAX_AE=0.214844` while the cocotb regression accepts the same design.

## Notes on Lint

`make lint` now runs with the local Verilator and reaches the intended source set, but still exits non-zero due to pre-existing RTL warnings, including:
- `PINCONNECTEMPTY`
- `EOFNEWLINE`
- `UNUSEDPARAM`

This is expected under the current `-Wall` setup and is not a new regression introduced by the Verilator upgrade.

## Impact

We now have two working RTL validation paths on this server:
1. `cocotb + local Verilator 5.047`
2. `direct Verilator C++ testbench + local Verilator 5.047`

This unblocks the earlier HLS-vs-RTL comparison plan. We can now use:
- cocotb for module/top behavioral checks
- direct Verilator C++ TB for cycle/perf breakdown and stricter numeric diffs

## Recommended Next Steps

1. Investigate the discrepancy between:
   - cocotb `fa_attention_core_full` pass
   - direct C++ TB `RTL vs FP32 MAX_AE` threshold failure
2. Reuse the direct C++ TB perf breakdown when refining HLS stage-level latency targets.
3. If needed, add one more top-level cocotb run for `fa_attention_ip_top_regs` once we want to validate task-queue/control-plane behavior against the HLS/XRT integration plan.
