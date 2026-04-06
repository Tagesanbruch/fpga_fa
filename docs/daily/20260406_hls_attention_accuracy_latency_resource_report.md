# 2026-04-06 HLS FlashAttention 数值、延迟与资源分析日报

## 1. 本日结论

今天这轮工作在上一个日报基础上继续推进了两件关键事情：

1. 不再停留在 `strict q8.8` 路径，而是把 HLS 数值链往 `docs/report_0310/大纲.md` 的主线 RTL 口径靠；
2. 继续探索 LUT 压力的根因，并验证单纯删 pragma 是否真的能降 LUT。

当前结论如下：

- 当前 HLS kernel 已经从旧的 `strict` 数值口径切换到更接近主线 RTL 的 `ctx + acc24 normalize-align` 口径：
  - `exp2_ctx_step_q1_15` 风格的 exp 近似
  - `row_acc` compute 期使用 `int32 / Q16.16-like` 状态
  - normalize 前对 `row_acc` 做 `<<< 8` 尺度对齐
- `csim` 全部通过；对于 `S=64/128/256` 这些完整 tile case，HLS 与 cmodel `RTL_CTX_STEP_ACC24` **逐点一致**。
- 最坏 case `gaussian_s256_causal` 的误差已经从旧版 HLS 的
  - `MAE=0.012225`
  - `MaxAE=0.300781`
  显著下降到：
  - `MAE=0.002854`
  - `MaxAE=0.045567`
- 这已经明显靠近 `docs/report_0310/大纲.md` 中主线 RTL 的 `MAE=0.002499 / MaxAE=0.006583`，说明之前的大误差主因确实是**数值口径没有跟到主线 RTL**，不是 testbench 算错。
- 当前主线 RTL-like HLS 综合结果为：
  - `BRAM 115 (39%)`
  - `DSP 96 (7%)`
  - `FF 73490 (31%)`
  - `LUT 102052 (87%)`
- 基于当前 `csynth.rpt` 的粗估，`S=256, D=64` 下总延迟约为：
  - `2,734,696 cycles`
  - 在 `5 ns` 目标时钟下约 `13.67 ms`
- 与 `report_0310` 中主线 RTL 的 `85,928 cycles` 相比，当前 HLS 仍然约慢 `31.8x`，所以现在还不能把它视为性能版。
- 我额外试了一轮“去掉 `row_acc` 的行维完整分裂”来降 LUT，但 HLS 在 `init_row_context` 的 pipeline 上**自动把它推回去了**。这说明当前 LUT 压力不是一条 pragma 能解决的，而是和“为了守住 `II=1`，工具被迫把行状态全部摊开”强相关。

## 2. 当前版本状态

涉及文件：

- HLS 算法主体：[fpga/common/fa_q8_8_attention.cpp](/3.2T/work/fpga-fa/fpga/common/fa_q8_8_attention.cpp)
- HLS 头文件：[fpga/common/fa_q8_8_attention.hpp](/3.2T/work/fpga-fa/fpga/common/fa_q8_8_attention.hpp)
- HLS kernel 顶层：[fpga/hls/fa_attention_kernel/fa_attention_kernel.cpp](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/fa_attention_kernel.cpp)
- HLS testbench：[fpga/hls/fa_attention_kernel/tb/test_fa_attention_kernel.cpp](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/tb/test_fa_attention_kernel.cpp)
- 综合报告：[fpga/hls/fa_attention_kernel/build/fa_attention_kernel_hls/solution1/syn/report/csynth.rpt](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/build/fa_attention_kernel_hls/solution1/syn/report/csynth.rpt)
- 主线 RTL softmax leaf：[rtl/core/fa_online_softmax_ctx.sv](/3.2T/work/fpga-fa/rtl/core/fa_online_softmax_ctx.sv)
- 主线 RTL normalize leaf：[rtl/core/fa_o_normalize_block.sv](/3.2T/work/fpga-fa/rtl/core/fa_o_normalize_block.sv)
- 主线 RTL core：[rtl/core/fa_attention_core.sv](/3.2T/work/fpga-fa/rtl/core/fa_attention_core.sv)
- cmodel 主路径：[cmodel/csrc/attention_kernels.cpp](/3.2T/work/fpga-fa/cmodel/csrc/attention_kernels.cpp)
- cocotb fixed-point 参考：[dv/cocotb/tests/fp_ref.py](/3.2T/work/fpga-fa/dv/cocotb/tests/fp_ref.py)

当前 HLS 实现已经覆盖 RTL 主计算链路的结构语义：

- `init_row_context`
- `load_q_tile`
- `load_kv_tile`
- `compute_kv_tile`
- `normalize_tile`
- `store_o_tile`

并完成：

- `Q/K/V/O` 的 AXI `m_axi` 接口；
- `seq_len/stride/scale/neg_large/causal/profile` 的 AXI-Lite 控制接口；
- `csim` 级功能、精度和 latency-model 回归；
- `vitis_hls` 下的 `csynth` 与 `.xo` 导出。

## 3. 为什么之前误差相对 0310 主线 RTL 偏高

### 3.1 旧版 HLS 的问题不在 testbench，而在数值口径

`docs/report_0310/大纲.md` 记录的当前主线 RTL 指标是：

- `MAE = 0.002499`
- `MaxAE = 0.006583`
- `85928 cycles`

而旧版 HLS（仍停留在 `strict q8.8` 路径）最坏 case 为：

- `gaussian_s256_causal`
- `MAE = 0.012225`
- `MaxAE = 0.300781`

这不是回归脚本有问题，而是**HLS 的数值链路还没有跟到主线 RTL**。旧版 HLS 用的是：

- `exp_pwl_q1_15`
- `64-bit strict row_acc`
- 没有主线 RTL 的 `acc -> normalize` 尺度对齐

而 `report_0310` 的主线 RTL 已经明确做了：

- `ctx/exp_d` 家族回到主线
- `row_acc` 在 compute 期以 `Q16.16 / 32-bit` 语义滚动
- normalize 入口 `row_acc <<< 8` 的尺度修正

### 3.2 这轮 HLS 已经切到更接近主线 RTL 的数值路径

本轮具体改动：

1. `compute_kv_tile()` 从 `exp_pwl_q1_15()` 切到 `exp2_ctx_q1_15()`；
2. `row_acc` 从 compute 期 `int64_t` 改为 `int32_t`；
3. `normalize_tile()` 引入 `row_acc <<< 8` 对齐；
4. testbench 对完整 tile case 改为对比 `attn::online_rtl_like(..., Mode::RTL_CTX_STEP_ACC24, ...)`。

### 3.3 当前精度结果

`make -C fpga/hls/fa_attention_kernel csim` 的当前结果：

| Case | 主线 RTL-like HLS vs FP32 MAE | MaxAE |
|---|---:|---:|
| `smoke_s32_causal_small` | 0.002758 | 0.006209 |
| `smoke_s64_noncausal_small` | 0.002408 | 0.005934 |
| `gaussian_s64_causal` | 0.003335 | 0.027344 |
| `gaussian_s128_noncausal` | 0.002614 | 0.012807 |
| `small_s128_causal_neg16` | 0.002722 | 0.005994 |
| `gaussian_s256_causal` | 0.002854 | 0.045567 |
| `small_s256_noncausal` | 0.002857 | 0.005890 |

对照旧版 strict-path HLS：

- `gaussian_s64_causal`: `0.017171 / 0.146656`
- `gaussian_s128_noncausal`: `0.009171 / 0.071735`
- `gaussian_s256_causal`: `0.012225 / 0.300781`

结论：

- 这轮数值链对齐是有效的；
- 当前 HLS 已经不再是“strict 参考正确但离主线 RTL 很远”的状态；
- 现在它已经进入“数值上基本接近主线 RTL，但系统性能和 LUT 还没跟上”的阶段。

补充说明：

- 对 `S=64/128/256`，HLS 与 cmodel `RTL_CTX_STEP_ACC24` 逐点一致；
- 对 `S=32`，同一个 cmodel 路径不能直接作为黄金参考，因为 `online_rtl_like()` 的 `kt < S / TK` 在 `S=32, TK=64` 时不会进入循环。因此 testbench 对这类 case 只做 HLS 功能回归与 FP32 对照，不做“主线 RTL-like cmodel exact compare”。

## 4. 为什么 DSP 低、LUT 高

### 4.1 当前资源结果

主线 RTL-like HLS 当前综合结果：

- Top: `BRAM 115 (39%) / DSP 96 (7%) / FF 73490 (31%) / LUT 102052 (87%)`
- Kernel body `run_attention_tiled_hls`: `BRAM 49 / DSP 96 / FF 67464 / LUT 96057`

### 4.2 LUT 高的主因

LUT 高并不只是“乘法没有进 DSP”，主因仍然是：

1. **顶层控制逻辑偏重**
- `run_attention_tiled_hls` 仍是顺序式 tile kernel；
- 带 profile 更新、ping-pong 选择、变量 trip loop；
- HLS 自动生成的使能/地址/条件逻辑本身就重。

2. **定点数值链偏 LUT-heavy**
- `exp2_ctx_q1_15` 虽然比旧 `exp_pwl` 更接近主线 RTL，但仍是查表 + 逻辑；
- `recip_nr_rtl_q16_16` 也是局部乘法 + 拼接 + 修正逻辑；
- `normalize` 路径里有符号扩展、舍入、饱和、mux。

3. **状态组织导致大扇出**
- `row_m[TQ]`
- `row_l[TQ]`
- `row_acc[TQ][D]`
- 为了守住 `II=1`，HLS 会把相当一部分局部状态做成高度可见的并行结构，带来大 fanout 与 mux 代价。

4. **当前 HLS 还没把 RTL 主线的 batch/context 调度真正带进来**
- RTL 主线有 `ROW_PAR=2`、`SOFTMAX_CTXS=4`、`QK tag pipeline + batch-streamed retire/issue`；
- 当前 HLS 只是数值路径更像 RTL 了，但调度结构仍然是 row-serial loop nest；
- 所以系统级控制开销和状态可见性仍然偏大。

### 4.3 DSP 为什么不高

DSP 并不高，原因也比较明确：

- 当前设计的 LUT 热点并不主要集中在“裸乘法器数量不够多”；
- 很多热点来自控制、表、拼接、移位、状态 mux；
- 把少量乘法推进 DSP 有帮助，但不会根本改变 `LUT=87%` 这一现实。

## 5. LUT -> DSP 映射，以及后续 LUT 探索

### 5.1 第一轮 DSP bind 的结果

前一轮已经对这些乘法做过 `bind_op ... impl=dsp`：

- `q8_8_mul_sat()` 中的 `prod`
- `mul_q1_31()` / `mul_q1_31_corr()` 的 16x16 部分积
- `compute_kv_tile()` 中的 `qk_mul` / `l_scaled_mul` / `acc_old_mul` / `pv_mul`

在旧 strict-path HLS 上，资源变化是：

- `DSP: 112 -> 120`
- `LUT: 103792 -> 103163`
- `FF: 87248 -> 84932`
- `BRAM: 115 -> 115`

结论：

- LUT -> DSP 的方向是对的；
- 但它解决不了主要矛盾，因为 LUT 高并不主要来自“裸乘法器”。

### 5.2 切到主线 RTL-like 路径之后的结果

切换到 `ctx + acc24 align` 路径后，综合结果变为：

- `BRAM 115`
- `DSP 96`
- `FF 73490`
- `LUT 102052`

对比前一版 strict-path HLS：

- LUT: `103163 -> 102052`
- FF: `84932 -> 73490`
- DSP: `120 -> 96`

解释：

1. `32-bit row_acc` 的确减轻了状态量，FF 降得比较明显；
2. LUT 也小幅下降，说明数值链切到主线 RTL-like 以后，状态/控制逻辑确实更合理了；
3. 但 DSP 没有继续上升，说明当前这版的优势不是“多用 DSP”，而是“数值链结构更像 RTL 主线”。

### 5.3 额外探索：删除 `row_acc` 行维完整分裂

我又做了一轮探索，尝试把：

- `#pragma HLS ARRAY_PARTITION variable=row_acc complete dim=1`

拿掉，只保留：

- `dim=2 factor=8`

目的很明确：

- 希望减少 `row_acc` 在行维度上的完全摊开，降低 LUT / fanout。

结果：

- HLS 自己把它推回来了；
- log 明确显示：
  - `Inferring pragma 'array_partition type=complete dim=1' for array 'row_acc_0' ... due to pipeline pragma`

这说明：

- 在当前 `init_row_context` / compute 组织下，工具为了守住 `II=1`，会自动要求更强的行状态可见性；
- 也就意味着：**单纯删 pragma 不足以降 LUT**。

这个观察很重要，因为它告诉我们接下来真正值得做的不是继续调一个 pragma，而是要改结构：

1. 把 context clear 从当前高度 pipeline 化的行状态路径拆开；
2. 让 HLS 不再被迫把行状态全部 materialize 成宽控制可见结构；
3. 更接近 RTL 主线的 `ROW_PAR / SOFTMAX_CTXS` 组织，而不是继续在 row-serial loop nest 上抠 pragma。

## 6. Cycle 级延迟分析

### 6.1 `csim` 主机时间

这是 CPU 上跑 regression 的 wall-clock，不代表板上硬件延迟，但能作为回归趋势参考：

- `S=256, gaussian, causal`: `9.080314 ms`
- `S=256, small-int, noncausal`: `9.410974 ms`

### 6.2 当前 `csynth` 子阶段 cycles

从当前 `csynth.rpt` 摘出的关键子阶段：

- `load_q_tile`: `2124 cycles`
- `init_row_context`: `67 cycles`
- `load_kv_tile`: `4172 cycles`
- `compute_kv_tile`: `79937 cycles`
- `normalize_tile`: `1089 cycles`
- `store_o_tile`: `2121 cycles`

对 `S=256, D=64, TQ=32, TK=64`：

- `Q tiles = 8`
- `KV tiles = 4`

每个 `Q tile` 粗估：

```text
2124 + 67 + 4*4172 + 4*79937 + 1089 + 2121
= 341837 cycles
```

全局粗估：

```text
8 * 341837 = 2734696 cycles
```

按当前 `5 ns` 目标时钟折算：

```text
2734696 * 5ns = 13.67348 ms
```

与 `report_0310` 的主线 RTL 比较：

- `mainline RTL = 85928 cycles`
- `current HLS ≈ 2734696 cycles`
- 差距约为：`31.8x`

结论：

- 现在 HLS 的主要问题已经不再是“精度离主线 RTL 很远”；
- 真正剩下的核心问题是：**系统级延迟依然远大于主线 RTL**。

## 7. 当前 `csim` 回归结果

当前 `make -C fpga/hls/fa_attention_kernel csim` 已全部通过。

摘要：

- `cases=7`
- `rtl_main_aligned=PASS`
- 最坏 HLS vs FP32 case：`gaussian_s256_causal`
- `MAE=0.002854`
- `MaxAE=0.045567`
- `worst_host_ms=9.410974`

同时输出的 cmodel compute-only latency model 也仍可作为后续结构优化参考：

- `fixed_flow_l32_norm8_row2 total_compute_only_cycles=100608`
- `fixed_flow_l32_norm8_row4 total_compute_only_cycles=51328`
- `simple_noc_l32_norm8_row4 total_compute_only_cycles=71808`

这组结果再次说明：

- 如果想把 HLS 系统级 latency 真正往 RTL 主线 `85928 cycles` 靠，关键不是继续在当前 row-serial kernel 上做 leaf 微调；
- 而是需要引入更像 RTL 主线的行并行 / context 调度。

## 8. 当前 `csynth` 结果

当前主线 RTL-like HLS 综合摘要：

- Top：
  - `BRAM 115 (39%)`
  - `DSP 96 (7%)`
  - `FF 73490 (31%)`
  - `LUT 102052 (87%)`
- `run_attention_tiled_hls`：
  - `BRAM 49 (17%)`
  - `DSP 96 (7%)`
  - `FF 67464 (28%)`
  - `LUT 96057 (82%)`

重要观察：

- `normalize_tile` 自身已经不算大：`DSP 48 / LUT 5055`
- 真正的大头仍然在：
  - `compute_kv_tile`: `LUT 12058`
  - 以及 `run_attention_tiled_hls` 顶层围绕 tile/state/control 生成的大量逻辑
- HLS log 里 `init_row_context` 的 estimated max fanout 已经接近 `9k+`，这和 LUT 高、控制重是相互印证的。

## 9. 结论

1. 当前 HLS 已经从“只对齐 strict 参考”推进到了“数值上接近 0310 主线 RTL”的阶段。
2. 最坏 case `gaussian_s256_causal` 已从 `0.012225 / 0.300781` 降到 `0.002854 / 0.045567`，这个改进是实质性的。
3. 当前真正的 blocker 已经从“精度偏离”转向：
   - `LUT 仍高达 87%`
   - `系统级 cycles 仍约 31.8x 落后于主线 RTL`
4. 额外的 LUT 探索已经表明：
   - 不是单纯删掉 `row_acc` 的 partition pragma 就能降 LUT；
   - HLS 会为了 `II=1` 自动把行状态重新摊开。
5. 因此下一阶段最值得做的不是立刻接 GEMV/GEMM，也不是直接上 `llama.cpp`，而是：
   - 更严格地把 HLS 调度结构往 RTL 主线靠；
   - 重点考虑 `ROW_PAR / SOFTMAX_CTXS / batch retire-issue` 这类系统级组织；
   - 让 HLS 不再依赖“把整行状态全摊开”来换取局部 `II=1`。
