# KV260 `llama-server` + FPGA 后端 CI/部署脚本

这个目录用于把当前仓库里的 HLS/XRT 接口落实到可以上传到 KV260 Jupyter Notebook 并直接跑评测脚本的形态。

## 目录目标

1. 在主机构建带 `ggml-fpga` 后端的 `llama-server`
2. 将 `llama-server`、`libggml-fpga.so`、HLS `xclbin`、评测脚本打包成可上传目录
3. 在 KV260 板端使用统一脚本启动 OpenAI 兼容的 `/v1` 服务
4. 直接对接：
   - `inference/problem/sample.py`
   - `inference/problem/throughput_eval.py`

## 当前已就绪的交付物

当前仓库内已经准备好的内容分两部分：

1. `inference/ci/artifacts/hls_attention_kernel/`
   - HLS `fa_attention_kernel.xo`
   - `csynth.rpt`
   - 关键 `syn/impl` Verilog
   - 这部分用于后续 `xpfm + v++ --link -> xclbin`

2. `inference/ci/artifacts/kv260_upload_bundle/`
   - `aarch64` 版 `llama-server`
   - `aarch64` 版 `llama-cli`
   - `libggml-fpga.so`
   - `libllama/libggml/libmtmd` 依赖库
   - `sample.py`
   - `throughput_eval.py`
   - 板端启动/评测脚本
   - 这是当前可以直接上传到 KV260 Jupyter Notebook 的目录

## 当前后端能力

当前 `ggml-fpga` 后端只会接管满足以下条件的 `FLASH_ATTN_EXT`：

- `D = 64`
- `seq_len <= 256`
- `q_len == kv_len`
- `mask != nullptr` 时按 **causal** 口径处理
- 对 causal 场景，backend 会自动将 `seq_len` 向上补齐到 64 的倍数，以适配 HLS kernel 的 tile 约束

因此：

- 短 prefill 的 causal attention 可以命中 FPGA
- 长 prefill（超过 256）以及 decode (`q_len != kv_len`) 仍会回退到 CPU
- 这也是当前 VLM 真正端到端加速效果仍有限的原因

## 脚本说明

### 1. 构建

```bash
bash inference/ci/build_llama_server_fpga_kv260.sh
```

默认调用：

- `inference/xcomp/build_kv260_llama_fpga.sh`

可通过环境变量控制：

- `BUILD_DIR`
- `GGML_FPGA_XRT`
- `GGML_FPGA_XRT_ROOT`
- `JOBS`（默认 `96`）

如果当前宿主机不是 `aarch64`，上面的脚本只会生成宿主机 native 产物，用于软件烟雾验证。要生成可直接上传到 KV260 的 `aarch64` 产物，请使用：

```bash
bash inference/ci/build_llama_server_fpga_kv260_docker.sh
```

同样支持：

- `BUILD_DIR_IN_CONTAINER`
- `GGML_FPGA_XRT`
- `GGML_FPGA_XRT_ROOT`
- `JOBS`（默认 `96`）

### 2. 打包上传目录

```bash
bash inference/ci/prepare_kv260_bundle.sh \
  --build-dir /path/to/build-kv260-fpga-arm64 \
  --bundle-dir /tmp/kv260-llama-bundle \
  --xclbin /path/to/fa_attention_kernel.xclbin \
  --model /path/to/model.gguf \
  --mmproj /path/to/mmproj.gguf
```

会整理出：

- `bin/llama-server`
- `bin/llama-cli`（如果构建产物存在）
- `bin/run_llama_server_fpga_kv260.sh`
- `bin/run_problem_throughput_eval.sh`
- `bin/run_problem_sample.sh`
- `lib/libggml-fpga.so`
- `lib/libllama*.so`
- `lib/libggml*.so`
- `lib/libmtmd*.so`
- `scripts/sample.py`
- `scripts/throughput_eval.py`
- `models/`（如果提供了 `--model/--mmproj`）
- `bitstreams/`（如果提供了 `--xclbin`）

当前默认不把 `data_extracted/` 一起打包，原因是：

- `sample.py` 只需要 `FullTest.json`
- `throughput_eval.py` 只需要输入图片和已启动的 `llama-server`
- 它们都不直接依赖 `data_extracted/`

### 3. 板端启动 server

上传 bundle 到 KV260 后：

```bash
cd /path/to/bundle/bin
bash ./run_llama_server_fpga_kv260.sh \
  --model ../models/model.gguf \
  --mmproj ../models/mmproj.gguf \
  --xclbin ../bitstreams/fa_attention_kernel.xclbin
```

这个脚本会：

- 设置 `LD_LIBRARY_PATH=../lib:$LD_LIBRARY_PATH`
- 设置 `GGML_BACKEND_PATH=../lib/libggml-fpga.so`
- 设置 `GGML_FPGA_MODE=xrt`
- 设置 `GGML_FPGA_XCLBIN=...`
- 默认使用 `--device FPGA0`
- 默认使用 `--flash-attn on`
- 以 `0.0.0.0:8080` 启动 `llama-server`

如果板上已经有现成模型文件，可以直接复用。例如你当前 KV260 上已有：

- `/root/sjtu-bin/AICAS/gguf/SmolVLM2-500M-Video-Instruct-Q8_0.gguf`
- `/root/sjtu-bin/AICAS/gguf/mmproj-SmolVLM2-500M-Video-Instruct-Q8_0.gguf`

对应启动方式：

```bash
cd /path/to/bundle/bin
bash ./run_llama_server_fpga_kv260.sh \
  --model /root/sjtu-bin/AICAS/gguf/SmolVLM2-500M-Video-Instruct-Q8_0.gguf \
  --mmproj /root/sjtu-bin/AICAS/gguf/mmproj-SmolVLM2-500M-Video-Instruct-Q8_0.gguf \
  --xclbin /path/to/fa_attention_kernel.xclbin
```

### 4. 运行评测脚本

吞吐量评测：

```bash
cd /path/to/bundle/bin
bash ./run_problem_throughput_eval.sh \
  --image /root/sjtu-bin/AICAS/image.png \
  --output /tmp/throughput_metrics.json
```

采样测试集：

```bash
cd /path/to/bundle/bin
bash ./run_problem_sample.sh \
  --input /root/sjtu-bin/AICAS/FullTest.json \
  --output /tmp/sample_100.json
```

## 注意事项

1. `throughput_eval.py` 只负责调用 `llama-server` 并写出 JSON，不负责启动服务。
2. `sample.py` 只是抽样工具，不会调用 `llama-server`。
3. 板端若 `xbutil examine` 异常，不代表 `llama-server + XRT` 一定不可用；以实际 `xclbin` 装载和推理结果为准。
4. 当前 VLM 目录下尚未提供现成 `GGUF + mmproj` 成品，若要使用 `llama-server` 多模态推理，需要额外准备这两类文件。
5. 当前 `inference/ci/artifacts/kv260_upload_bundle/` 已经是 `aarch64` 产物，可直接上传；它不是宿主机烟雾测试版本。
6. 当前还缺 `fa_attention_kernel.xclbin`，原因不是 HLS/`llama-server` 未完成，而是平台侧还未完成 `xpfm + v++ --link`。
