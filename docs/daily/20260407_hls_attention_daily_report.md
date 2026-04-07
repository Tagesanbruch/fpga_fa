# 2026-04-07 HLS FlashAttention 日报（基于 archive 快照）

## 1. 今日结论

本日工作围绕两件事完成闭环：

1. 基于最新可复现综合结果完成归档快照，并用归档内数据生成日报；
2. 继续尝试一轮 consume 侧结构优化，验证后确认退化，已回滚到当前稳定基线。

当前稳定基线（以归档快照为准）为：

- Top latency: `786727 cycles`
- Interval: `786728`
- 资源：`BRAM 165`、`DSP 353`、`FF 43372`、`LUT 43379`
- Slack: `-3.11 ns`
- C-sim: `rtl_main_aligned=PASS`，最坏 `gaussian_s256_causal`：`MAE=0.002854`、`MaxAE=0.045567`

结论：

- 功能与精度保持稳定；
- 当前距离用户目标 `<200k cycles` 仍有显著差距；
- 今日新增结构尝试未带来收益，主线已保持在当前 best-known 稳定分支。

## 2. 归档快照与证据

本日报所有关键指标均来自以下归档目录：

- [archive/20260407_014017_hls_attention_stable_snapshot](archive/20260407_014017_hls_attention_stable_snapshot)

关键文件：

- 总体综合：[archive/20260407_014017_hls_attention_stable_snapshot/reports/csynth.rpt](archive/20260407_014017_hls_attention_stable_snapshot/reports/csynth.rpt)
- 顶层综合摘要：[archive/20260407_014017_hls_attention_stable_snapshot/reports/fa_attention_kernel_csynth.rpt](archive/20260407_014017_hls_attention_stable_snapshot/reports/fa_attention_kernel_csynth.rpt)
- compute 子模块：[archive/20260407_014017_hls_attention_stable_snapshot/reports/compute_kv_tile_csynth.rpt](archive/20260407_014017_hls_attention_stable_snapshot/reports/compute_kv_tile_csynth.rpt)
- consume 子模块：[archive/20260407_014017_hls_attention_stable_snapshot/reports/p_anonymous_namespace_consume_score_batch_Pipeline_VITIS_LOOP_141_1_csynth.rpt](archive/20260407_014017_hls_attention_stable_snapshot/reports/p_anonymous_namespace_consume_score_batch_Pipeline_VITIS_LOOP_141_1_csynth.rpt)
- online softmax leaf：[archive/20260407_014017_hls_attention_stable_snapshot/reports/online_softmax_ctx_acc_row_step_csynth.rpt](archive/20260407_014017_hls_attention_stable_snapshot/reports/online_softmax_ctx_acc_row_step_csynth.rpt)
- qk leaf：[archive/20260407_014017_hls_attention_stable_snapshot/reports/qk_dotprod_slice_pair_csynth.rpt](archive/20260407_014017_hls_attention_stable_snapshot/reports/qk_dotprod_slice_pair_csynth.rpt)
- 调度警告诊断：[archive/20260407_014017_hls_attention_stable_snapshot/diagnostics/.message_syn.xml](archive/20260407_014017_hls_attention_stable_snapshot/diagnostics/.message_syn.xml)
- C-sim 日志：[archive/20260407_014017_hls_attention_stable_snapshot/logs/csim.log](archive/20260407_014017_hls_attention_stable_snapshot/logs/csim.log)
- 快照指标提要：[archive/20260407_014017_hls_attention_stable_snapshot/logs/metrics_snapshot.txt](archive/20260407_014017_hls_attention_stable_snapshot/logs/metrics_snapshot.txt)
- 回滚尝试记录：[archive/20260407_014017_hls_attention_stable_snapshot/logs/regression_attempt_note.txt](archive/20260407_014017_hls_attention_stable_snapshot/logs/regression_attempt_note.txt)

## 3. 指标总览

### 3.1 稳定基线（本日报主数据）

来自 [archive/20260407_014017_hls_attention_stable_snapshot/reports/csynth.rpt](archive/20260407_014017_hls_attention_stable_snapshot/reports/csynth.rpt)：

- `fa_attention_kernel`: `786727 cycles`
- `run_attention_tiled_hls`: `786725 cycles`
- 外层 Q-tile 循环 `VITIS_LOOP_307_1`: trip=`8`, iteration latency=`98323`
- `compute_kv_tile`: `18549 cycles`
- `process_score_batch`: interval=`4615`
- `generate_score_batch` 主 pipeline `VITIS_LOOP_83_1_VITIS_LOOP_85_2`: II=`4`
- `consume_score_batch` 主 pipeline `VITIS_LOOP_141_1`: II=`9`
- `online_softmax_ctx_acc_row_step`: interval=`8`
- `qk_dotprod_slice_pair`: interval=`4`

### 3.2 功能精度

来自 [archive/20260407_014017_hls_attention_stable_snapshot/logs/csim.log](archive/20260407_014017_hls_attention_stable_snapshot/logs/csim.log)：

- `cases=7`
- `rtl_main_aligned=PASS`
- 最坏 case：`gaussian_s256_causal`
- `hls_vs_fp32`: `MAE=0.002854`, `MaxAE=0.045567`

### 3.3 相对目标差距

- 相对当前 RTL 主线 `85928 cycles`：
  - `786727 / 85928 = 9.16x`
- 相对用户目标 `<200000 cycles`：
  - 仍高出 `3.93x`

## 4. 3.6 风格延迟拆解

本节按 [docs/report_0310/大纲.md](../report_0310/大纲.md) 第 3.6 节的写法，给出可计算、可复核的阶段拆解。

### 4.1 参数与层级关系

当前固定参数：

- `S=256`, `D=64`
- `TQ=32`，`TK=64`
- `Nq = S / TQ = 8`
- `Nk = S / TK = 4`

综合层级关键关系（来自归档 `csynth.rpt`）：

- 外层 Q-tile 循环 `VITIS_LOOP_307_1`：trip=`8`, iteration latency=`98323`
- 单个 K-tile 核心计算 `compute_kv_tile`：`18549`
- `compute_kv_tile` 内部 batch-loop `VITIS_LOOP_215_1`：trip=`4`, iteration latency=`4637`
- 单 batch 的 dataflow 主体 `process_score_batch`：interval=`4615`

可写成：

$$
C_{total} \approx C_{fixed} + N_q \cdot C_{q\_tile}
$$

$$
C_{q\_tile} \approx C_{load\_q} + C_{init} + N_k \cdot C_{kv\_tile} + C_{norm/store} + C_{ctrl}
$$

$$
C_{kv\_tile} \approx N_{batch} \cdot C_{batch}, \quad N_{batch}=4
$$

$$
C_{batch} \approx \max(C_{gen}, C_{consume})
$$

### 4.2 数值代入

- `C_q_tile = 98323`
- `Nq * C_q_tile = 8 * 98323 = 786584`
- Top `C_total = 786727`

说明大部分时延由 Q-tile 主循环主导，循环外固定开销极小。

在每个 Q-tile 内：

- `compute` 区域 `VITIS_LOOP_322_2 = 90920`
- 占比：

$$
\frac{90920}{98323} = 92.47\%
$$

即单 Q-tile 超过九成周期花在 `compute_kv_tile` 相关路径。

在每个 batch 内：

- `generate` 关键 loop II=`4`，单 batch ~`1042`
- `consume` 关键 loop II=`9`，单 batch ~`4611`

consume / generate 比值：

$$
\frac{4611}{1042} = 4.43
$$

因此 dataflow 虽然建立，但吞吐被 consume 侧强烈钳制。

### 4.3 结构瓶颈定位

结合 [archive/20260407_014017_hls_attention_stable_snapshot/diagnostics/.message_syn.xml](archive/20260407_014017_hls_attention_stable_snapshot/diagnostics/.message_syn.xml) 与归档提要：

1. `HLS 200-1449`（dataflow 警告）持续存在
- `consume_score_batch` 既有前驱又直接读取调用者输入，HLS 明示可能降低吞吐。

2. `online_softmax_ctx_acc_row_step` recurrence 与端口冲突
- `HLS 200-880`: row_acc 读写携带依赖
- `HLS 200-885`: `v_row` / `row_acc` memory ports 受限
- 直接导致 leaf interval=`8`

3. consume loop 受 leaf + recurrence 双重约束
- `VITIS_LOOP_141_1` 最终 II=`9`
- 已接近 leaf interval 下界，但仍高于目标。

4. 时钟仍有负 slack
- `Slack = -3.11ns`
- `Estimated clock = 6.765ns`（目标 `5.0ns`）

## 5. 今日新增优化尝试与回滚说明

本日新增一次结构试验（score stream 载荷简化 + consume 确定性循环重排），结论是**明确退化**，已回滚。

记录见 [archive/20260407_014017_hls_attention_stable_snapshot/logs/regression_attempt_note.txt](archive/20260407_014017_hls_attention_stable_snapshot/logs/regression_attempt_note.txt)：

- 退化版本：`1017255 cycles`
- consume loop II 拉高到 `25`
- 同时 LUT/FF 增长明显

因此当前主线保持稳定 packet 化 consume 实现，不引入本次回归改动。

## 6. 下一步（面向 <200k）

按当前数据，若要达到 `<200k`，不是 pragma 微调问题，而是结构重构问题。建议优先级：

1. 拆解 consume recurrence
- 引入更强的多上下文交错（softmax ctx banking）
- 将 row 级状态更新拆成更可并行/更可调度的两级流水

2. 清理 dataflow coupling（针对 `200-1449`）
- 将 caller 输入改为前驱 process 显式复制/桥接
- 降低 HLS 对跨 process 依赖的保守约束

3. 提高 batch 粒度并行度（在资源预算内）
- 目标不是把单 leaf 拉满，而是把 `C_batch` 从当前 ~`4615` 降到 `~1200-1500` 量级

4. 同步时序收敛
- 当前负 slack 与大 fanout 会反向限制调度
- 需和架构重构一起推进，不可后置

## 7. 今日状态结论

- 已完成：稳定基线复现、归档、证据化日报。
- 已验证：新增尝试为负收益并已回滚。
- 未达成：`<200k cycles`。
- 当前最关键事实：瓶颈集中在 consume/online recurrence + dataflow coupling，后续必须做结构级改造。