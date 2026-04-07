# 2026-04-07 `llama-server` + HLS/KV260 CI 接入日报

## 归档快照

本日报对应的归档快照目录：

- [20260407_031633_llama_server_kv260_bundle_snapshot](/3.2T/work/fpga-fa/docs/daily/archive/20260407_031633_llama_server_kv260_bundle_snapshot)

归档中已包含：

- `bundle/`：当前可打包出来的 `llama-server` 运行目录
- `inference_ci/`：本轮新增/修改的 CI 与板端脚本
- `logs/`：`llama-server --help` 与 `ldd` 输出
- `meta/bundle_manifest.json`：bundle 中文件清单、大小与 `sha256`
- `meta/status.txt`：common image `md5`、当前宿主机构建产物架构、Docker 构建阻塞说明

## 本轮完成内容

### 1. 澄清评测入口

确认：

- [sample.py](/3.2T/work/fpga-fa/inference/problem/sample.py) 只是抽样工具，不调用服务。
- [throughput_eval.py](/3.2T/work/fpga-fa/inference/problem/throughput_eval.py) 会调用 OpenAI 兼容接口 `http://127.0.0.1:8080/v1`，因此它依赖 `llama-server` 正常启动并返回 `timings` 字段。

因此板端评测链应为：

1. 启动 `llama-server`
2. 再执行 `throughput_eval.py`
3. 由脚本生成 `prefill_speed_tps / decode_speed_tps` 的 JSON

### 2. `ggml-fpga` 后端已继续贴近 HLS 接口

本轮对 [ggml-fpga.cpp](/3.2T/work/fpga-fa/inference/xcomp/llama.cpp/ggml/src/ggml-fpga/ggml-fpga.cpp) 做了一个重要修正：

- 对 **causal** 短序列场景，backend 允许把 `seq_len` 向上补齐到 64 的倍数后再提交给 HLS/XRT 接口。
- 这样一来，短 prefill 不再必须天然满足 `32/64` tile 对齐，命中 FPGA 的机会更高。

当前后端仍然只支持：

- `FLASH_ATTN_EXT`
- `D = 64`
- `seq_len <= 256`
- `q_len == kv_len`
- causal attention 优先

仍然不支持：

- 长 prefill (`seq_len > 256`)
- decode (`q_len != kv_len`)
- `sinks`
- `softcap`
- 任意稀疏/复杂 mask

因此现在的加速范围仍然是“短序列、单块 attention bring-up 路线”，还不是完整 VLM 端到端硬件加速。

### 3. `inference/ci` 已补成可复用的部署脚本集合

已完成以下脚本与中文说明：

- [README.md](/3.2T/work/fpga-fa/inference/ci/README.md)
- [build_llama_server_fpga_kv260.sh](/3.2T/work/fpga-fa/inference/ci/build_llama_server_fpga_kv260.sh)
- [build_llama_server_fpga_kv260_docker.sh](/3.2T/work/fpga-fa/inference/ci/build_llama_server_fpga_kv260_docker.sh)
- [prepare_kv260_bundle.sh](/3.2T/work/fpga-fa/inference/ci/prepare_kv260_bundle.sh)
- [run_llama_server_fpga_kv260.sh](/3.2T/work/fpga-fa/inference/ci/run_llama_server_fpga_kv260.sh)
- [run_problem_sample.sh](/3.2T/work/fpga-fa/inference/ci/run_problem_sample.sh)
- [run_problem_throughput_eval.sh](/3.2T/work/fpga-fa/inference/ci/run_problem_throughput_eval.sh)

其中：

- `build_llama_server_fpga_kv260.sh`：当前宿主机构建入口；若宿主不是 `aarch64`，会明确提示这只是软件烟雾验证产物。
- `build_llama_server_fpga_kv260_docker.sh`：后续真正的 `arm64/KV260` 构建入口。
- `prepare_kv260_bundle.sh`：把 server、FPGA backend、本地共享库、评测脚本与可选 `xclbin/model/mmproj` 统一打包。
- `run_llama_server_fpga_kv260.sh`：板端统一入口，会设置 `LD_LIBRARY_PATH`、`GGML_BACKEND_PATH`、`GGML_FPGA_MODE`、`GGML_FPGA_XCLBIN`。

### 4. 本机 smoke build 已通过

我在宿主机上执行了：

```bash
BUILD_DIR=/tmp/llama-fpga-kv260-smoke GGML_FPGA_XRT=OFF bash inference/ci/build_llama_server_fpga_kv260.sh
```

结果：

- `llama-server` 构建成功
- `llama-cli` 构建成功
- `libggml-fpga.so` 构建成功

关键产物大小（来自归档快照与本机构建目录）：

- `llama-server`: `11175680` bytes
- `llama-cli`: `5851040` bytes
- `libggml-fpga.so`: `64176` bytes

并且已验证 bundle 中的二进制在脱离原 build 目录后仍可独立拉起帮助页：

```bash
LD_LIBRARY_PATH=/tmp/kv260-llama-fpga-bundle/lib /tmp/kv260-llama-fpga-bundle/bin/llama-server --help
```

归档日志：

- [llama_server_help.txt](/3.2T/work/fpga-fa/docs/daily/archive/20260407_031633_llama_server_kv260_bundle_snapshot/logs/llama_server_help.txt)
- [ldd_llama_server.txt](/3.2T/work/fpga-fa/docs/daily/archive/20260407_031633_llama_server_kv260_bundle_snapshot/logs/ldd_llama_server.txt)

### 5. bundle 已能生成，但当前是宿主机版本

已成功打包：

- `bin/llama-server`
- `bin/llama-cli`
- `bin/run_llama_server_fpga_kv260.sh`
- `bin/run_problem_sample.sh`
- `bin/run_problem_throughput_eval.sh`
- `lib/libggml-fpga.so`
- `lib/libllama*.so`
- `lib/libggml*.so`
- `lib/libmtmd*.so`
- `scripts/sample.py`
- `scripts/throughput_eval.py`

bundle 清单见：

- [bundle_manifest.json](/3.2T/work/fpga-fa/docs/daily/archive/20260407_031633_llama_server_kv260_bundle_snapshot/meta/bundle_manifest.json)

但当前 bundle 仍然是 **宿主机 x86_64 产物**，不是可以直接上传到 KV260 的 `aarch64` 二进制。`file` 输出已归档：

- [status.txt](/3.2T/work/fpga-fa/docs/daily/archive/20260407_031633_llama_server_kv260_bundle_snapshot/meta/status.txt)

## 环境核验

### 1. ZYNQMP common image

已确认当前 common image 包的 `md5` 与你刚刚修正后的 2023.1 值一致：

- `fffc62554d65a8cf617cb9087e97eb14`

对应文件：

- `third_party/xilinx-zynqmp-common-v2023.1_05080224.tar.gz`

### 2. Docker arm64 构建

我尝试了：

```bash
docker compose -f inference/xcomp/docker-compose.yml build
```

当前仍然失败，失败原因不是脚本，而是 **Docker daemon 的代理配置**：

- daemon 拉 `ubuntu:22.04` 时走到了 `127.0.0.1:8888`
- 该代理拒绝连接

这意味着：

- `build_llama_server_fpga_kv260_docker.sh` 的流程已经有了
- 但当前宿主机 Docker 环境还不能完成 `arm64` 容器拉起

## 指标与结论

### 功能性

- `throughput_eval.py` 对 `llama-server` 的调用路径已打通到脚本层
- `llama-server` + `ggml-fpga` + bundle 组织已可复现
- 运行时依赖（`libllama/libggml/libmtmd/libggml-fpga`）已补齐，不再是一个“只拷可执行文件”的半成品包

### 精度/算子映射

- 本轮没有改变 HLS attention 核心数值口径，只扩展了 `ggml-fpga` 对 causal 短序列的 tile 对齐兼容
- 因此 attention 的数值指标仍以先前 HLS 日报为准，不在本日报重复统计

### 延迟/加速效果

- 本轮工作聚焦在部署链，不是重新优化 HLS kernel 延迟
- 因此目前还不能给出 `llama-server` 端到端的 KV260 实测 `prefill/decode` 指标
- 真正的性能 JSON 需要在以下条件齐备后才能生成：
  1. `aarch64` 版 `llama-server` bundle
  2. `fa_attention_kernel.xclbin`
  3. 可用的 `GGUF + mmproj`
  4. KV260 板端启动成功的 `llama-server`

## 当前阻塞

1. **当前 smoke build 是 x86_64，不可直接上传到 KV260 运行**
2. **Docker arm64 构建被 daemon 代理卡住**
3. **仓库里的 `SmolVLM2-500M-Video-Instruct` 目前是 HF/ONNX 资产，不是 `GGUF + mmproj` 成品**
4. **HLS/XRT 路线仍缺最终 `fa_attention_kernel.xclbin`**
5. **即使 server 跑起来，当前 FPGA 后端也只覆盖短 prefill attention，不能完整覆盖 decode/VLM 全流程**

## 下一步建议

1. 先修 Docker daemon 代理，让 `build_llama_server_fpga_kv260_docker.sh` 真正产出 `aarch64` bundle
2. 同步推进 `kv260_custom.xpfm -> xclbin`，让 `GGML_FPGA_XCLBIN` 有真实输入
3. 准备 `SmolVLM2-500M-Video-Instruct` 对应的 `GGUF + mmproj`
4. 在 KV260 Jupyter Notebook 中按 bundle 目录启动 `llama-server`
5. 运行 `run_problem_throughput_eval.sh`，拿到第一版 `prefill/decode` JSON
