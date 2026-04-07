# KV260 平台工作区

这个目录用于放置仓库内、可重建的 KV260 平台脚本与产物。

它和 HLS kernel 源码分离：

- HLS kernel：`fpga/hls/`
- 板级 / 平台壳：`fpga/platforms/kv260/`
- Vitis link：`fpga/vitis/`

## 目标

我们希望最终形成两条稳定路径：

1. `最小 KV260 平台壳`
   - 纯 Tcl 生成
   - 导出 `.xsa`
   - 再生成 `.xpfm`

2. `算子验证通路`
   - 先用最简单的 `add_seq` kernel 验证
   - 再逐步上复杂 kernel

## 重要匹配约束

平台必须和当前 kernel 约束匹配：

1. 器件
- `xck26-sfvc784-2LV-c`

2. 运行时
- Linux + XRT/zocl
- 板端通过 XRT 原生接口加载 `xclbin`

3. kernel 侧接口
- 一个 AXI-Lite 控制口
- 若干 AXI memory 口，典型映射到：
  - `HPC0`
  - `HPC1`
  - `HP3`

4. 时钟
- 当前默认目标仍以 `5.0ns / 200MHz` 为参考

## 目录说明

- `hw/`
  - 参考旧工程导出 Tcl / XSA 的脚本
  - `minimal/` 下是仓库内最小 KV260 shell 的 Tcl 骨架

- `vitis/`
  - common image 准备脚本
  - 最小平台 `.xpfm` 生成脚本

## 当前入口

### 参考旧工程

1. 导出旧工程 BD Tcl：

```bash
vivado -mode batch -source fpga/platforms/kv260/hw/write_reference_bd_from_legacy_xpr.tcl
```

2. 导出旧工程参考 XSA：

```bash
vivado -mode batch -source fpga/platforms/kv260/hw/export_reference_xsa_from_legacy_xpr.tcl
```

### 生成最小平台骨架

3. 生成最小 KV260 shell：

```bash
cd fpga/platforms/kv260/hw/minimal
vivado -mode batch -source main.tcl -tclargs -jobs 96
```

4. 基于 `.xsa` 生成最小 `.xpfm`：

```bash
fpga/platforms/kv260/vitis/create_minimal_xpfm.sh
```

### 准备 common image

5. 准备 common image：

```bash
fpga/platforms/kv260/vitis/prepare_common_image.sh
```

## 当前状态

- 旧工程参考抽取脚本已可用
- 最小 KV260 platform Tcl 骨架已放到：
  - `fpga/platforms/kv260/hw/minimal/`
- 这套最小平台脚本当前仍属于“第一版骨架”
  - 重点是把 PS / clock / PFM 属性收敛到最小可用集
  - 后续仍需要用实际 `xclbin` 继续验证是否足够轻、是否还需要补 SmartConnect / reset 结构
