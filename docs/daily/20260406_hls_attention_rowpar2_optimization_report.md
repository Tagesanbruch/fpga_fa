## 2026-04-06 HLS Attention `ROW_PAR=2` 优化日报

### 背景

本轮目标是继续严格参考 RTL 主线实现，对 HLS 版 FlashAttention 做结构级优化，尽可能向 [大纲.md](/3.2T/work/fpga-fa/docs/report_0310/大纲.md) 中的主线口径靠拢，重点看三件事：

- 是否能更接近 RTL 的 `ROW_PAR=2` 组织
- 是否能在不破坏精度的前提下进一步降低 HLS cycle
- 是否能同时压低此前偏高的 LUT 占用

RTL 当前主线口径为：

- full-run `cycles=85928`
- 相对 FP32：`MAE=0.002499`、`MaxAE=0.006583`

参考来源：

- [大纲.md](/3.2T/work/fpga-fa/docs/report_0310/大纲.md)
- [fa_attention_core.sv](/3.2T/work/fpga-fa/rtl/core/fa_attention_core.sv)

### 本轮修改

核心修改文件：

- [fa_q8_8_attention.hpp](/3.2T/work/fpga-fa/fpga/common/fa_q8_8_attention.hpp)
- [fa_q8_8_attention.cpp](/3.2T/work/fpga-fa/fpga/common/fa_q8_8_attention.cpp)

主要改动：

1. 在 HLS 公共头文件中正式引入 `kRowPar = 2`
2. 将 HLS 主计算路径改为按 query row pair 处理，而不是单 row 串行
3. 继续保留并复用当前主线的数值口径：
   - `exp2_ctx_q1_15`
   - `row_acc << 8` 后进入 normalize
   - `row_l / row_m / row_acc` 的 streamed-ctx 主线组织
4. 对以下阶段都改成 row-pair 形式：
   - `init_row_context`
   - `load_q_tile`
   - `compute_kv_tile`
   - `normalize_tile`
   - `store_o_tile`
5. 对关键数组增加分区与关键乘法 `bind_op impl=dsp`

本轮 HLS 更接近 RTL 的地方：

- query 处理粒度从单行切换到 `ROW_PAR=2`
- `compute_kv_tile` 中同一 key row 上同时更新两个 query row 的 online softmax 上下文
- normalize 阶段也按 row pair 组织

### 数值验证结果

运行：

```bash
make fpga-kernel-csim
```

结果：通过。

关键结论：

- `HLS vs rtl_main_ref`：逐点一致
- `rtl_main_aligned=PASS`
- 最坏 case 仍是 `gaussian_s256_causal`

最坏 HLS 相对 FP32：

- `MAE=0.002854`
- `MaxAE=0.045567`

与上一版 HLS 相比，这一版保持了已经修回来的主线数值口径，没有因为 `ROW_PAR=2` 重构而回退到早期 `strict` 误差水平。

与 RTL 主线对比：

- RTL 主线：`MAE=0.002499`、`MaxAE=0.006583`
- 当前 HLS：`MAE=0.002854`、`MaxAE=0.045567`

判断：

- `MAE` 已经接近主线量级
- `MaxAE` 仍明显高于 RTL 主线
- 当前 HLS 数值已经不再是主问题，但还不能算“完全追平 RTL”

### 综合结果

运行：

```bash
make fpga-kernel-xo
```

结果：通过，成功导出：

- [fa_attention_kernel.xo](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/build/fa_attention_kernel.xo)

综合报告：

- [csynth.rpt](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/build/fa_attention_kernel_hls/solution1/syn/report/csynth.rpt)
- [compute_kv_tile_csynth.rpt](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/build/fa_attention_kernel_hls/solution1/syn/report/compute_kv_tile_csynth.rpt)
- [init_row_context_csynth.rpt](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/build/fa_attention_kernel_hls/solution1/syn/report/init_row_context_csynth.rpt)
- [normalize_tile_csynth.rpt](/3.2T/work/fpga-fa/fpga/hls/fa_attention_kernel/build/fa_attention_kernel_hls/solution1/syn/report/normalize_tile_csynth.rpt)

Top 资源结果：

| 指标 | 当前结果 |
| --- | --- |
| BRAM | `163 (56%)` |
| DSP | `159 (12%)` |
| FF | `45596 (19%)` |
| LUT | `83015 (70%)` |
| Estimated Fmax | `273.97 MHz` |

相对前一轮 attention HLS 的主要变化：

- LUT 明显下降
- DSP 明显上升
- BRAM 明显上升

这说明当前方向是有效的：

- 我们确实把一部分计算/并行开销从 LUT 转到了 DSP 和存储结构上
- 但这不是免费优化，BRAM 与端口冲突问题被放大了

### Cycle 级延迟分析

当前 HLS 关键阶段 latency：

| 阶段 | Latency / II |
| --- | --- |
| `load_q_tile` | `2124 cycles`, `II=2` |
| `init_row_context` | `515 cycles`, 关键 pipeline `II=8` |
| `load_kv_tile` | `4172 cycles`, `II=1` |
| `compute_kv_tile` | `66593 cycles` |
| `normalize_tile` | `897 cycles` |
| `store_o_tile` | `2121 cycles`, `II=2` |

在默认参数下：

- `S=256`
- `TQ=32`，所以 `Q tiles = 8`
- `TK=64`，所以 `KV tiles = 4`

粗估每个 Q tile：

```text
load_q        =  2124
init_ctx      =   515
load_kv x4    = 16688
compute x4    = 66593
normalize     =   897
store_o       =  2121
--------------------------------
per_q_tile    ≈ 88938 cycles
```

总 rough cycles：

```text
88938 * 8 = 711504 cycles
```

若按 `273.97 MHz` 粗估：

```text
711504 / 273.97e6 ≈ 2.60 ms
```

注意：这里的 `711504 cycles` 是 HLS 静态 rough estimate，不等价于 RTL 顶层实测 perf counter；但足够说明当前数量级。

与 RTL 主线对比：

- RTL 主线：`85928 cycles`
- 当前 HLS rough estimate：`711504 cycles`

比例约为：

```text
711504 / 85928 ≈ 8.28x
```

也就是说，这轮优化虽然显著优于早期 HLS 结构，但距离 RTL 主线仍然有大约 `8x` 的周期差距，还远没有做到“同一水平”。

### 与 RTL 的结构对照结论

本轮已经靠近 RTL 的部分：

1. `ROW_PAR=2` query row pair 组织
2. `compute_kv_tile` 中的双 row 在线 softmax 更新
3. `normalize` 的 row-pair 处理方式

仍未追平 RTL 的关键部分：

1. RTL 主线已经实现 system-level overlap，不是简单 row pair 并行
2. RTL 已将 `QK issue / score retire / softmax issue` 做到真正重叠
3. RTL 的 `SOFTMAX_CTXS=4`、`QPAIR_BATCH_ROWS=8` 这组批流式上下文调度，当前 HLS 还没有
4. HLS 当前依然是 tile 级顺序 kernel，缺少主线 RTL 的多上下文交错与更激进的 system-level pipeline

### 当前主要瓶颈

当前最主要的 3 个问题已经很明确：

1. `init_row_context` 太慢

- 当前达到 `515 cycles`
- 关键 pipeline `II=8`
- 直接原因是 `row_acc` 初始化写端口冲突

2. `load_q_tile` / `store_o_tile` 受 AXI 端口限制

- 两者都只有 `II=2`
- 这会直接拉高每个 Q tile 的固定开销

3. `compute_kv_tile` 虽然明显改善，但仍缺 RTL 那种 system-level overlap

- 已从先前更高的量级下降到 `66593 cycles`
- 但仍不足以追平 RTL 主线的整体组织能力

### 结论

本轮 `ROW_PAR=2` HLS 重构是有效的，主要成果是：

1. HLS 结构更接近 RTL 主线
2. 精度保持在接近主线的量级，没有因重构而退化
3. LUT 从更高的水平明显下降到 `83015 (70%)`
4. `compute_kv_tile` latency 有明显改善

但必须明确：

- 当前 HLS 还没有达到 RTL 主线的延迟水平
- 当前 rough estimate 仍约为 RTL 的 `8.28x`
- 真正阻碍继续收敛的已经不是简单 pragma，而是：
  - `row_acc` 初始化结构
  - AXI load/store 固定开销
  - 缺失 RTL 主线里的多上下文 / system-level overlap

### 下一步建议

如果继续以“尽量让 HLS 延迟追平 RTL”为目标，后续优先级建议是：

1. 重构 `init_row_context`

- 避免每个 Q tile 显式清零整块 `row_acc`
- 优先探索 valid-tag / lazy reset / 局部 scratch accumulator 方案

2. 重构 `compute_kv_tile`

- 继续减少 scalar recurrence
- 尝试把 row-pair 的点积与 `acc update` 再拆开

3. 继续沿 RTL 主线逼近 system-level overlap

- 评估是否需要在 HLS 里引入更接近 `SOFTMAX_CTXS=4` 的多上下文组织
- 否则 HLS 很难在周期上接近当前 RTL 主线

当前判断：

- 本轮可以算作一次成功的结构优化
- 但距离“与 RTL 在一个延迟水平上”还有明显距离
- 下一轮应该优先继续压 `init_row_context` 和 system-level overlap，而不是再做零碎 pragma 微调
