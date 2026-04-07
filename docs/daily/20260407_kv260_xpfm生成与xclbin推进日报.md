# 2026-04-07 KV260 xpfm 生成与 xclbin 推进日报

## 结论

今天把 KV260 平台链路里最关键的一步打通了：

- 已成功从 `kv260_bist.xsa` 生成 `kv260_bist.xpfm`
- 同时生成了 `kv260_bist.spfm`
- 这说明 `XSA -> XPFM` 这条路已经可用，后续可以直接用于 `v++ --link`

当前已经进入 `fa_attention_kernel.xo + kv260_bist.xpfm -> fa_attention_kernel.xclbin` 阶段。

## 关键产物

当前 CI 产物目录：

- `inference/ci/artifacts/kv260_platform/kv260_bist.xsa`
- `inference/ci/artifacts/kv260_platform/kv260_bist.xpfm`
- `inference/ci/artifacts/kv260_platform/kv260_bist.spfm`
- `inference/ci/artifacts/kv260_platform/kv260_bist_wrapper.bit`
- `inference/ci/artifacts/kv260_platform/kv260_bist_wrapper_timing_summary_routed.rpt`
- `inference/ci/artifacts/kv260_platform/kv260_bist_wrapper_utilization_placed.rpt`

归档快照：

- `docs/daily/archive/20260407_125200_kv260_xpfm_generation_snapshot/platform/kv260_bist.xpfm`
- `docs/daily/archive/20260407_125200_kv260_xpfm_generation_snapshot/platform/kv260_bist.spfm`
- `docs/daily/archive/20260407_125200_kv260_xpfm_generation_snapshot/platform/platform.log`
- `docs/daily/archive/20260407_125200_kv260_xpfm_generation_snapshot/platform/IDE.log`

## 过程与问题定位

最初平台导出卡在 `platform generate`，报错为：

- `ERROR: default domain is empty`

进一步检查 `platform.spr` 后确认：

- `sysDefaultDom` 为空
- 平台里只有两个 `bootDomain`
  - `zynqmp_fsbl`
  - `zynqmp_pmufw`
- 没有可作为运行时默认域的 Linux domain

这意味着：

- Vivado 硬件平台本身是好的
- boot BSP/FSBL/PMUFW 生成也是好的
- 真正缺的是一个给 `XRT + v++` 使用的 Linux 运行域

后续在现有 `xsct/kv260_bist` workspace 上补了：

- `domain create -name smp_linux -os linux -proc psu_cortexa53`

这里还踩到了一个小坑：

- `psu_cortexa53_0` 不能用于创建 Linux domain
- XSCT 需要的是处理器簇名 `psu_cortexa53`

修正后，`platform generate` 成功结束，并打印了：

- `PLATFORM_GENERATE_DONE`

## 当前 xclbin 推进状态

已经开始执行：

- `bash fpga/vitis/kv260/build_xclbin.sh`

并成功识别新平台：

- `KV260 platform : .../kv260_bist.xpfm`

当前构建链已经越过：

- 平台缺失
- `.xpfm` 缺失
- `fa_q8_8_dot.cpp` 未加入 HLS 工程

现在重新进入的告警，已经全部回到 HLS 本体已知热点：

### 1. `dotprod_q8_8_pair`

HLS 当前调度结果：

- `Final II = 4`

主要约束：

- `rhs_*` 数组端口不足
- 典型告警：
  - `Unable to schedule 'load' operation ... on array 'rhs_6' due to limited memory ports`

### 2. `online_softmax_ctx_acc_row_step`

当前调度结果中，主要告警为：

- `row_acc_0` 读写相关的 carried dependence
- `v_row_0` / `row_acc_0` memory ports 不足

这和之前 `csynth` 分析是一致的，说明：

- 平台链路已经打通
- 当前剩余压力重新回到了 HLS 核本体的数据通路结构

## 对后续的意义

这一步打通之后，项目状态发生了实质变化：

1. 不再缺 KV260 acceleration platform
2. 后续可以直接围绕这份 `kv260_bist.xpfm` 做 `v++ --link`
3. `llama.cpp + ggml-fpga + KV260` 这条链已经只差最终 `xclbin`

## 下一步

1. 等当前 `build_xclbin.sh` 跑完，确认 `fa_attention_kernel.xclbin` 是否生成
2. 若 link 成功：
   - 复制到 `inference/ci/artifacts/kv260_upload_bundle/bitstreams/`
   - 补齐 KV260 上传包说明
3. 若 link 失败：
   - 优先检查是 `v++` 层平台兼容问题，还是 HLS kernel 结构问题
4. 无论 link 成败，后续都要回到两个主要 HLS 热点继续优化：
   - `dotprod_q8_8_pair`
   - `online_softmax_ctx_acc_row_step`

