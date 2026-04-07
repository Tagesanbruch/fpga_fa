# 2026-04-07 周报（HLS FlashAttention）

## 1. 本周目标与结果

本周核心目标：在保证数值一致性的前提下，推动 HLS 延迟向 `<200k cycles` 收敛，并形成可复现实验证据链。

本周结果（截至 2026-04-07）：

- 已形成稳定可复现基线并完成归档；
- 本周新增一次结构优化尝试已量化为回归并回滚；
- 当前稳定主线延迟为 `786727 cycles`，与 `<200k` 仍有 `3.93x` 差距。

## 2. 本周完成项

1. 基线稳定性闭环
- `csim` 多 case 连续通过，`rtl_main_aligned=PASS`。
- 指标稳定，未出现“看似优化但精度失真”的隐藏回归。

2. 性能瓶颈定位持续收敛
- 明确 top 延迟由 Q-tile 主循环主导；
- 明确 compute 区域占单 Q-tile 超九成；
- 明确 consume 侧 II 与 online recurrence 是主矛盾。

3. 本周新增优化尝试并完成回滚
- 尝试方向：score stream 载荷简化 + consume 循环重排；
- 结果：top 从 `786727` 恶化到 `1017255`，consume II 升到 `25`；
- 决策：立即回滚，恢复稳定实现。

4. 证据归档体系落地
- 新建快照：
  - [docs/daily/archive/20260407_014017_hls_attention_stable_snapshot](archive/20260407_014017_hls_attention_stable_snapshot)
- 快照内包含：综合报告、诊断 XML、build 产物、csim 日志、关键源码快照与指标摘要。

## 3. 本周关键数据（以归档为准）

主数据来源：

- [archive/20260407_014017_hls_attention_stable_snapshot/reports/csynth.rpt](archive/20260407_014017_hls_attention_stable_snapshot/reports/csynth.rpt)
- [archive/20260407_014017_hls_attention_stable_snapshot/logs/csim.log](archive/20260407_014017_hls_attention_stable_snapshot/logs/csim.log)
- [archive/20260407_014017_hls_attention_stable_snapshot/diagnostics/.message_syn.xml](archive/20260407_014017_hls_attention_stable_snapshot/diagnostics/.message_syn.xml)

稳定基线：

- Top latency: `786727`
- `compute_kv_tile`: `18549`
- `process_score_batch` interval: `4615`
- `generate` II: `4`
- `consume` II: `9`
- `online_softmax_ctx_acc_row_step` interval: `8`
- 资源：`BRAM 165 / DSP 353 / FF 43372 / LUT 43379`
- Slack：`-3.11ns`

精度/一致性：

- `cases=7`, `rtl_main_aligned=PASS`
- worst case `gaussian_s256_causal`: `MAE=0.002854`, `MaxAE=0.045567`

## 4. 周内结论与风险

### 4.1 已确认结论

1. 主要问题不是功能正确性，而是结构吞吐上限。
2. 单纯改 stream 载荷或局部循环形态，无法突破 online recurrence 主瓶颈。
3. `HLS 200-1449` + `200-880/885` 组合提示，说明 dataflow 划分与状态更新耦合仍偏重。

### 4.2 当前风险

1. 若继续以局部 pragma/小改为主，可能反复出现“可综合但整体更慢”的试错。
2. 当前负 slack 会进一步压制调度自由度。
3. 若不先降低 consume recurrence 压力，`<200k` 基本不可达。

## 5. 下周执行计划（可量化）

1. 架构优先：先做 recurrence 解耦方案
- 目标：把 `consume` 主 loop II 从 `9` 压到 `<=4`。

2. dataflow 优先：消除/缓解 `200-1449`
- 目标：跨 process 输入来源改为显式桥接，减少综合器保守调度。

3. 以 batch 周期作为中间 KPI
- 当前 `C_batch ~ 4615`
- 下周目标：先降到 `~2500`，再评估资源与时序代价。

4. 周内验收门槛
- 功能：`rtl_main_aligned=PASS`
- 延迟：至少进入 `<500k` 档位，否则视为未形成有效突破。

## 6. 本周状态总结

- 本周主线是“稳定性与证据链”优先，技术路线已从“局部改写”转向“结构重构”。
- 虽未达 `<200k`，但已避免把回归版本带入主线，并沉淀了可复盘归档，便于下周高效推进。