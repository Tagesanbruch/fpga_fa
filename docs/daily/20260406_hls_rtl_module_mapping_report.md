## 2026-04-06 HLS 对齐 RTL 模块化映射与评估日报

### 目标

本轮工作不是继续在单个 `fa_q8_8_attention.cpp` 里做局部 pragma 微调，而是先把 HLS 实现按 RTL 当前主线的模块边界重新组织，使后续优化能够直接对照 RTL 的 leaf / stage / core 结构定位问题。

参考 RTL 主线：

- [fa_attention_core.sv](/3.2T/work/fpga-fa/rtl/core/fa_attention_core.sv)
- [fa_qk_dotprod_slice.sv](/3.2T/work/fpga-fa/rtl/core/fa_qk_dotprod_slice.sv)
- [fa_online_softmax_ctx.sv](/3.2T/work/fpga-fa/rtl/core/fa_online_softmax_ctx.sv)
- [fa_o_normalize_block.sv](/3.2T/work/fpga-fa/rtl/core/fa_o_normalize_block.sv)
- [fa_recip_nr_q16_16.sv](/3.2T/work/fpga-fa/rtl/softmax/fa_recip_nr_q16_16.sv)
- [大纲.md](/3.2T/work/fpga-fa/docs/report_0310/大纲.md)

RTL 当前主线口径：

- `cycles=85928`
- `MAE=0.002499`
- `MaxAE=0.006583`

### RTL 结构调研结论

当前 RTL 不是单一 compute 函数，而是明确分成两层：

1. leaf/算子层

- `fa_qk_dotprod_slice`
- `fa_online_softmax_ctx`
- `fa_recip_nr_q16_16`
- `fa_o_normalize_block`

2. core/调度层

- `fa_attention_core`
- `ROW_PAR=2`
- `SOFTMAX_CTXS=4`
- `QPAIR_BATCH_ROWS = ROW_PAR * SOFTMAX_CTXS = 8`
- `QK tag pipeline + batch-streamed retire/issue + K/V ping-pong`

也就是说，RTL 的关键性能来源不是“单个 online softmax leaf 很快”，而是：

- row-pair 并行
- softmax 多上下文交错
- QK/softmax retire/issue 重叠
- tile 级系统调度闭环

这也是为什么仅仅把 HLS 写成 row-pair 版本，仍然很难自动接近 RTL 的 `85928 cycles`。

### 本轮 HLS 代码重组

#### 新增模块文件

为了和 RTL 的 leaf 结构一一对应，本轮新增了这些 HLS/C++ 模块：

- [fa_q8_8_fixed_point.hpp](/3.2T/work/fpga-fa/fpga/common/fa_q8_8_fixed_point.hpp)
- [fa_q8_8_fixed_point.cpp](/3.2T/work/fpga-fa/fpga/common/fa_q8_8_fixed_point.cpp)
- [fa_q8_8_recip_nr_q16_16.hpp](/3.2T/work/fpga-fa/fpga/common/fa_q8_8_recip_nr_q16_16.hpp)
- [fa_q8_8_recip_nr_q16_16.cpp](/3.2T/work/fpga-fa/fpga/common/fa_q8_8_recip_nr_q16_16.cpp)
- [fa_q8_8_qk_dotprod_slice.hpp](/3.2T/work/fpga-fa/fpga/common/fa_q8_8_qk_dotprod_slice.hpp)
- [fa_q8_8_qk_dotprod_slice.cpp](/3.2T/work/fpga-fa/fpga/common/fa_q8_8_qk_dotprod_slice.cpp)
- [fa_q8_8_online_softmax_ctx.hpp](/3.2T/work/fpga-fa/fpga/common/fa_q8_8_online_softmax_ctx.hpp)
- [fa_q8_8_online_softmax_ctx.cpp](/3.2T/work/fpga-fa/fpga/common/fa_q8_8_online_softmax_ctx.cpp)
- [fa_q8_8_o_normalize_block.hpp](/3.2T/work/fpga-fa/fpga/common/fa_q8_8_o_normalize_block.hpp)
- [fa_q8_8_o_normalize_block.cpp](/3.2T/work/fpga-fa/fpga/common/fa_q8_8_o_normalize_block.cpp)
- [fa_q8_8_row_context_rf.hpp](/3.2T/work/fpga-fa/fpga/common/fa_q8_8_row_context_rf.hpp)
- [fa_q8_8_row_context_rf.cpp](/3.2T/work/fpga-fa/fpga/common/fa_q8_8_row_context_rf.cpp)

#### 调整的主文件

- [fa_q8_8_attention.cpp](/3.2T/work/fpga-fa/fpga/common/fa_q8_8_attention.cpp)
- [run_hls.tcl](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/run_hls.tcl)
- [Makefile](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/Makefile)

现在 HLS 的职责划分对应关系如下：

| RTL 模块 | HLS 对应 |
| --- | --- |
| `fa_qk_dotprod_slice` | `qk_dotprod_slice_pair()` |
| `fa_online_softmax_ctx` | `online_softmax_ctx_acc_row_step()` |
| `fa_recip_nr_q16_16` | `recip_nr_rtl_q16_16()` |
| `fa_o_normalize_block` | `o_normalize_block_row()` |
| `row context init` | `init_row_context_rf()` |
| `fa_attention_core` 外层 tile 调度 | `run_attention_tiled_hls()` |

### 功能/精度验证

运行：

```bash
make fpga-kernel-csim
```

结果：通过。

关键输出：

- `rtl_main_aligned=PASS`
- 所有测试用例 `exact=yes`

最坏 case：

- `gaussian_s256_causal`
- `MAE=0.002854`
- `MaxAE=0.045567`

与前一版相比：

- 数值没有回退
- 说明“模块拆分”本身没有破坏当前已经对齐到 `rtl_main_ref` 的主线数值口径

与 RTL 主线对比：

- RTL：`MAE=0.002499`、`MaxAE=0.006583`
- 当前 HLS：`MAE=0.002854`、`MaxAE=0.045567`

判断：

- `MAE` 已经接近 RTL 主线量级
- `MaxAE` 仍高于 RTL 主线
- 当前精度问题不是完全解决，但已经不是主瓶颈

### 综合与时序结果

运行：

```bash
make fpga-kernel-xo
```

结果：通过，成功导出：

- [fa_attention_kernel.xo](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/build/fa_attention_kernel.xo)

主要报告：

- [csynth.rpt](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/build/fa_attention_kernel_hls/solution1/syn/report/csynth.rpt)
- [run_attention_tiled_hls_csynth.rpt](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/build/fa_attention_kernel_hls/solution1/syn/report/run_attention_tiled_hls_csynth.rpt)
- [compute_kv_tile_csynth.rpt](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/build/fa_attention_kernel_hls/solution1/syn/report/compute_kv_tile_csynth.rpt)
- [qk_dotprod_slice_pair_csynth.rpt](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/build/fa_attention_kernel_hls/solution1/syn/report/qk_dotprod_slice_pair_csynth.rpt)
- [online_softmax_ctx_acc_row_step_csynth.rpt](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/build/fa_attention_kernel_hls/solution1/syn/report/online_softmax_ctx_acc_row_step_csynth.rpt)
- [init_row_context_rf_csynth.rpt](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/build/fa_attention_kernel_hls/solution1/syn/report/init_row_context_rf_csynth.rpt)
- [recip_nr_rtl_q16_16_csynth.rpt](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/build/fa_attention_kernel_hls/solution1/syn/report/recip_nr_rtl_q16_16_csynth.rpt)
- [o_normalize_block_row_csynth.rpt](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/build/fa_attention_kernel_hls/solution1/syn/report/o_normalize_block_row_csynth.rpt)

#### Top 结果

| 指标 | 当前结果 |
| --- | --- |
| BRAM | `163 (56%)` |
| DSP | `101 (8%)` |
| FF | `43756 (18%)` |
| LUT | `78384 (66%)` |
| Estimated Fmax | `273.97 MHz` |
| Loop Constraint Status | `NOT satisfied` |

相较前一轮 row-par 版 attention HLS：

- LUT 进一步下降
- FF 小幅下降
- DSP 略降
- BRAM 保持较高

这说明模块重组后，综合器对结构的识别更稳定了，但性能仍未闭环。

### 模块级 latency / PPA

从 `run_attention_tiled_hls_csynth.rpt` 抽取的关键模块结果：

| HLS 模块 | Latency |
| --- | --- |
| `load_q_tile` | `2124 cycles`, `II=2` |
| `init_row_context_rf` | `515 cycles` |
| `load_kv_tile` | `4172 cycles`, `II=1` |
| `compute_kv_tile` | `78881 cycles` |
| `o_normalize_block_row` | `32 cycles` |
| `store_o_tile` | `2121 cycles`, `II=2` |

子模块结果：

| 子模块 | Latency / 关键点 |
| --- | --- |
| `qk_dotprod_slice_pair` | `29 cycles`，内部 loop `II=3` |
| `online_softmax_ctx_acc_row_step` | `20 cycles`，内部 loop `II=1` |
| `recip_nr_rtl_q16_16` | `18 cycles` |
| `o_normalize_block_row` | `32 cycles` |
| `init_row_context_rf` | `514 cycles` 主体，初始化 loop `II=8` |

compute 内资源：

- `qk_dotprod_slice_pair`: `DSP 16`, `LUT 1448`
- `online_softmax_ctx_acc_row_step`: `DSP 28`, `LUT 1509`
- `o_normalize_block_row`: `DSP 48`, `LUT 3662`
- `compute_kv_tile` 总体：`DSP 46`, `LUT 5048`

### 与 RTL 的差距分析

#### 当前 rough cycle 估算

按默认参数：

- `S=256`
- `TQ=32`
- `TK=64`
- `Q tiles = 8`
- `KV tiles = 4`

粗估每个 Q tile：

```text
load_q        =  2124
init_ctx      =   515
load_kv x4    = 16688
compute       = 78881
normalize     ≈ 1088
store_o       =  2121
--------------------------------
per_q_tile    ≈ 101417 cycles
```

总 rough cycles：

```text
101417 * 8 = 811336 cycles
```

与 RTL 主线：

- RTL：`85928 cycles`
- 当前 HLS rough estimate：`811336 cycles`

比例约为：

```text
811336 / 85928 ≈ 9.44x
```

#### 关键结论

这轮做对了两件事：

1. HLS 已经有了与 RTL 一一对应的 leaf 模块结构
2. `csynth` 可以直接告诉我们每个 leaf 的 latency / II / 资源

但这轮也暴露了一个更重要的事实：

**仅仅把 HLS 拆成和 RTL 同名模块，并不会自动获得 RTL 的 system-level latency。**

原因很清楚：

1. 当前 HLS 仍然没有 RTL 的 `SOFTMAX_CTXS=4`
2. 当前 HLS 没有 `QK tag pipeline + score retire / softmax issue overlap`
3. 当前 HLS `qk_dotprod_slice_pair` 仍有 `II=3`
4. `init_row_context_rf` 仍然很重，`II=8`
5. `load_q_tile / store_o_tile` 仍受 m_axi 端口限制，`II=2`

而 RTL 主线真正快的原因恰恰是：

- `ROW_PAR=2`
- `SOFTMAX_CTXS=4`
- `QPAIR_BATCH_ROWS=8`
- `QK` 与 `softmax` 的 system-level overlap

### 结论

本轮完成了一个很重要但性质偏“结构对齐”的步骤：

- HLS 不再只是一个 600 行的大函数
- 而是已经按 RTL 当前主线的 leaf 模块边界完成拆分和映射

这对后续继续优化非常关键，因为现在我们终于能回答：

- `QK slice` 慢在哪里
- `softmax ctx` 慢在哪里
- `normalize` 的 latency/资源占比是多少
- `init row context` 为什么卡死 II

但要明确：

- 当前 HLS 还没有在延迟上接近 RTL
- 当前 rough cycles 仍约是 RTL 的 `9.44x`
- 这次工作的价值主要是“把问题从黑箱变成可分阶段定位”，不是“已经追平 RTL”

### 下一步建议

如果目标仍然是“尽量把 HLS latency 往 RTL 靠”，下一轮优先级建议是：

1. 在 HLS core 层引入 `SOFTMAX_CTXS=4` 风格的 batch 化

- 这是和 RTL 主线最本质的差距之一

2. 继续压 `qk_dotprod_slice_pair`

- 当前 `II=3`
- 需要更接近 RTL 加法树的结构，而不是简单累加 recurrence

3. 重构 `init_row_context_rf`

- 当前 `II=8`
- 是明确的固定开销瓶颈

4. 评估 `profile` 相关访存是否需要从 hot path 中剥离

- 当前 `profile` 的读写模式在 HLS 报告里有多处 burst 分析失败

当前判断：

- 这轮模块映射是必要且正确的
- 但还只是“为下一轮 system-level HLS 优化打底”
- 真正决定能否逼近 RTL 的，下一步仍然是 `ctx batch + overlap`，不是再做小范围 pragma 微调
