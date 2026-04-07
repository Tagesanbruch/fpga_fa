# 2026-04-07 KV260 Bundle 与 Platform 结论日报

## 一、结论

今天这一步已经把两条线收实：

1. **KV260 可上传 bundle 已经准备好**
   - `aarch64` 版 `llama-server`
   - `aarch64` 版 `llama-cli`
   - `libggml-fpga.so`
   - 运行所需 `libllama/libggml/libmtmd`
   - `sample.py`
   - `throughput_eval.py`
   - 板端启动/评测脚本

2. **`kria-vitis-platforms` 可以作为 KV260 平台生成入口**
   - 但当前 checkout 还是 `main`
   - 真正用于 `2023.1` 工具链时，应切到 `xlnx_rel_v2023.1`
   - submodule 也还没初始化
   - 这意味着：**平台路线是通的，但还没正式进入 build 阶段**

## 二、当前可直接使用的目录

### 1. HLS 交付物

路径：

- [inference/ci/artifacts/hls_attention_kernel](/3.2T/work/fpga-fa/inference/ci/artifacts/hls_attention_kernel)

目前包含：

- `fa_attention_kernel.xo`
- `csynth.rpt`
- `fa_attention_kernel.v`
- `fa_attention_kernel_run_attention_tiled_hls.v`
- `fa_attention_kernel_control_s_axi.v`

用途：

- 用于后续 `xpfm + v++ --link -> xclbin`

### 2. KV260 上传目录

路径：

- [inference/ci/artifacts/kv260_upload_bundle](/3.2T/work/fpga-fa/inference/ci/artifacts/kv260_upload_bundle)

目前包含：

- `bin/llama-server`
- `bin/llama-cli`
- `bin/run_llama_server_fpga_kv260.sh`
- `bin/run_problem_sample.sh`
- `bin/run_problem_throughput_eval.sh`
- `lib/libggml-fpga.so`
- `lib/libggml*.so*`
- `lib/libllama*.so*`
- `lib/libmtmd*.so*`
- `scripts/sample.py`
- `scripts/throughput_eval.py`
- `README.txt`

## 三、产物确认

当前 `aarch64` 产物已确认：

- `llama-server`
- `llama-cli`
- `libggml-fpga.so`

文件类型结果见归档：

- [aarch64_file_types.txt](/3.2T/work/fpga-fa/docs/daily/archive/20260407_050500_kv260_bundle_finalize_snapshot/meta/aarch64_file_types.txt)

动态依赖结果见：

- [llama_server_readelf_dynamic.txt](/3.2T/work/fpga-fa/docs/daily/archive/20260407_050500_kv260_bundle_finalize_snapshot/meta/llama_server_readelf_dynamic.txt)

结论：

- 当前 bundle 中的 server 不是宿主机测试版，而是 **ARM aarch64** 版，适合直接上传到 KV260。

## 四、板端现成资源

板上已有：

- 模型：`/root/sjtu-bin/AICAS/gguf/SmolVLM2-500M-Video-Instruct-Q8_0.gguf`
- mmproj：`/root/sjtu-bin/AICAS/gguf/mmproj-SmolVLM2-500M-Video-Instruct-Q8_0.gguf`
- 图片：`/root/sjtu-bin/AICAS/image.png`
- 样本集：`/root/sjtu-bin/AICAS/FullTest.json`

因此当前上传 bundle 时：

- **不需要重复上传 GGUF/mmproj**
- **不需要上传 `data_extracted/`**

原因：

- `sample.py` 只处理 `FullTest.json`
- `throughput_eval.py` 只调用本地 `llama-server` 并读取一张图片
- 两者都不直接依赖 `data_extracted/`

## 五、`kria-vitis-platforms` 结论

当前仓库：

- [third_party/kria-vitis-platforms](/3.2T/work/fpga-fa/third_party/kria-vitis-platforms)

已确认它具备 KV260 平台生成入口：

- [kv260/kv260.mk](/3.2T/work/fpga-fa/third_party/kria-vitis-platforms/kv260/kv260.mk)
- [kv260/platforms/kv260_bist/Makefile](/3.2T/work/fpga-fa/third_party/kria-vitis-platforms/kv260/platforms/kv260_bist/Makefile)

能力边界：

1. `kv260_bist` 可以生成 **extensible XSA**
2. 根 `Makefile` 可以在 `XSA + XSCT` 基础上生成 **`.xpfm`**
3. 生成我们自己的 `fa_attention_kernel.xclbin` 仍然需要：
   - 先有 `.xpfm`
   - 再用 `fa_attention_kernel.xo` 跑 `v++ --link`

也就是说：

- **它能生成平台**
- **不能直接替我们生成最终 attention `xclbin`**

### 当前还差什么

1. 切分支到 `xlnx_rel_v2023.1`
2. `git submodule update --init --recursive`
3. 准备 `xilinx-zynqmp-common-v2023.1`
4. 跑平台 build，得到 `.xpfm`
5. 再回到我们仓库的 HLS `.xo` 去 link `xclbin`

## 六、当前阻塞

当前真正剩下的板上 blocker 只有一个：

- **`fa_attention_kernel.xclbin` 还没有生成**

不是因为：

- HLS 没完成
- `llama-server` 没完成
- `aarch64` 构建没完成

而是因为：

- 平台侧 `xpfm` 还没落地

## 七、归档

本次 bundle/HLS 快照已归档：

- [20260407_050500_kv260_bundle_finalize_snapshot](/3.2T/work/fpga-fa/docs/daily/archive/20260407_050500_kv260_bundle_finalize_snapshot)

其中包括：

- bundle 目录
- HLS 交付物目录
- bundle/hls manifest
- `aarch64` 产物文件类型记录
- `llama-server` 动态依赖记录

## 八、下一步

1. 把 `kria-vitis-platforms` 切到 `xlnx_rel_v2023.1`
2. 初始化 submodule
3. 基于 `kv260_bist` 生成 `xpfm`
4. 用当前 `fa_attention_kernel.xo` 生成 `fa_attention_kernel.xclbin`
5. 把 `xclbin` 放进 `inference/ci/artifacts/kv260_upload_bundle/bitstreams/`
6. 在 KV260 上用已有 GGUF/mmproj/image 直接启动 `llama-server` 并跑：
   - `throughput_eval.py`
   - `sample.py`
