# 20260407 SmolVLM2 本机 CPU 评测与 xclbin 状态日报

## 归档来源
本日报以以下归档快照为主：
- [20260407_233500_smolvlm2_local_cpu_eval](/3.2T/work/fpga-fa/docs/daily/archive/20260407_233500_smolvlm2_local_cpu_eval)

关键原始文件：
- [local_smolvlm2_completion_response.json](/3.2T/work/fpga-fa/docs/daily/archive/20260407_233500_smolvlm2_local_cpu_eval/local_smolvlm2_completion_response.json)
- [local_smolvlm2_mtmd_cli.txt](/3.2T/work/fpga-fa/docs/daily/archive/20260407_233500_smolvlm2_local_cpu_eval/local_smolvlm2_mtmd_cli.txt)
- [local_smolvlm2_server_trace.log](/3.2T/work/fpga-fa/docs/daily/archive/20260407_233500_smolvlm2_local_cpu_eval/local_smolvlm2_server_trace.log)
- [local_smolvlm2_trace_summary.json](/3.2T/work/fpga-fa/docs/daily/archive/20260407_233500_smolvlm2_local_cpu_eval/local_smolvlm2_trace_summary.json)
- [throughput_metrics_local_smolvlm2.json](/3.2T/work/fpga-fa/docs/daily/archive/20260407_233500_smolvlm2_local_cpu_eval/throughput_metrics_local_smolvlm2.json)
- [local_smolvlm2_throughput_cold_warm.json](/3.2T/work/fpga-fa/docs/daily/archive/20260407_233500_smolvlm2_local_cpu_eval/local_smolvlm2_throughput_cold_warm.json)
- [convert_text.log](/3.2T/work/fpga-fa/docs/daily/archive/20260407_233500_smolvlm2_local_cpu_eval/convert_text.log)
- [convert_mmproj.log](/3.2T/work/fpga-fa/docs/daily/archive/20260407_233500_smolvlm2_local_cpu_eval/convert_mmproj.log)
- [v++_fa_attention_kernel.log](/3.2T/work/fpga-fa/docs/daily/archive/20260407_233500_smolvlm2_local_cpu_eval/v++_fa_attention_kernel.log)

## 结论先行
1. `SmolVLM2-500M-Video-Instruct` 已在本机 `x86_64` 上完成 `llama.cpp` CPU 版编译、HF -> GGUF 转换、`mmproj` 转换，并成功完成图像推理。
2. `llama-mtmd-cli` 路径可以稳定工作，说明模型、`mmproj`、视觉编码和文本生成主链都是通的。
3. `llama-server` 的 OpenAI `chat/completions` 多模态入口当前对 SmolVLM2 有模板拼接问题：服务端会把 `image_url` 转成 `media_marker`，但 SmolVLM2 的 chat template 只识别 `image`，导致 marker 在模板阶段丢失，最终报 `number of bitmaps (1) does not match number of markers (0)`。
4. 绕开方式已经验证：使用 `POST /completion`，以 `prompt_string + multimodal_data + <__media__>` 的方式可以稳定完成本地推理并返回 timings。
5. FPGA 侧 `xclbin` 仍未生成，当前阻塞已经明确为 `v++` 实现阶段 BRAM 过量，不再是平台问题。

## 本机环境与构建结果
### Python / 依赖环境
本轮复用了 conda 环境 `ecg_gpu_3`：
- `transformers 5.5.0`
- `sentencepiece 0.2.1`
- `openai 2.30.0`

### x86 CPU 版 llama.cpp
已成功编译的二进制：
- [llama-server](/3.2T/work/fpga-fa/inference/xcomp/llama.cpp/build-x86-cpu-smolvlm/bin/llama-server)
- [llama-cli](/3.2T/work/fpga-fa/inference/xcomp/llama.cpp/build-x86-cpu-smolvlm/bin/llama-cli)
- [llama-mtmd-cli](/3.2T/work/fpga-fa/inference/xcomp/llama.cpp/build-x86-cpu-smolvlm/bin/llama-mtmd-cli)

说明：
- 纯 CPU 编译
- `-j96` 并行构建
- 当前本地长期服务通过 `tmux` 保持，session 名为 `smolvlm2_server_cpu`
- 观察方式：`tmux attach -t smolvlm2_server_cpu`

## 模型准备情况
本地目录：
- [SmolVLM2-500M-Video-Instruct](/3.2T/work/fpga-fa/inference/vlm/SmolVLM2-500M-Video-Instruct)

本轮补齐了原先缺失的 tokenizer / processor 小文件：
- `preprocessor_config.json`
- `processor_config.json`
- `special_tokens_map.json`
- `tokenizer.json`
- `tokenizer_config.json`
- `vocab.json`

转换产物：
- 文本模型 GGUF：[ggml-model-f16.gguf](/3.2T/work/fpga-fa/inference/vlm/SmolVLM2-500M-Video-Instruct/ggml-model-f16.gguf)
- 多模态投影 GGUF：[mmproj-model-f16.gguf](/3.2T/work/fpga-fa/inference/vlm/SmolVLM2-500M-Video-Instruct/mmproj-model-f16.gguf)

## 推理链路验证
### 1. `llama-mtmd-cli` 直连验证
原始输出见：
- [local_smolvlm2_mtmd_cli.txt](/3.2T/work/fpga-fa/docs/daily/archive/20260407_233500_smolvlm2_local_cpu_eval/local_smolvlm2_mtmd_cli.txt)

结论：
- 图像编码、图像 token 解码、文本生成都已完成
- 对手写单词图片给出了合理描述与 OCR 结果，能识别出 `that`

关键性能：
- `prompt eval time = 8388.94 ms / 879 tokens`
- `prompt throughput = 104.78 tokens/s`
- `decode eval time = 3234.23 ms / 95 tokens`
- `decode throughput = 29.37 tokens/s`

### 2. `llama-server` 路径验证
服务长期运行在：
- `http://127.0.0.1:8080`

服务端原始日志：
- [local_smolvlm2_server_trace.log](/3.2T/work/fpga-fa/docs/daily/archive/20260407_233500_smolvlm2_local_cpu_eval/local_smolvlm2_server_trace.log)

接口现状分两类：

#### `POST /v1/chat/completions`
当前对 SmolVLM2 不稳定，报错原因已经确认：
- server 会把 `image_url` 改写成 `media_marker`
- 但 SmolVLM2 的 chat template 使用的是 `line['type'] == 'image'`
- 模板渲染阶段没有把 `<__media__>` 插入 prompt
- 最终在 `mtmd_tokenize()` 里看到：`1` 张图，`0` 个 marker

服务端日志中对应报错：
- `tokenize: error: number of bitmaps (1) does not match number of markers (0)`

#### `POST /completion`
这条路已经验证成功。使用形式：
- `prompt.prompt_string` 中显式放 `<__media__>`
- `prompt.multimodal_data` 里放 base64 图片

这条路径的原始返回：
- [local_smolvlm2_completion_response.json](/3.2T/work/fpga-fa/docs/daily/archive/20260407_233500_smolvlm2_local_cpu_eval/local_smolvlm2_completion_response.json)

## 吞吐分析
### 冷启动请求
来自 [local_smolvlm2_throughput_cold_warm.json](/3.2T/work/fpga-fa/docs/daily/archive/20260407_233500_smolvlm2_local_cpu_eval/local_smolvlm2_throughput_cold_warm.json) 的第 2 条完整图文请求：
- `prompt_tokens = 879`
- `prompt_ms = 6935.14`
- `prompt_tps = 126.75`
- `decode_tokens = 96`
- `decode_ms = 3479.92`
- `decode_tps = 27.59`

这是当前最有代表性的本机 CPU 吞吐数据。

### 热缓存复用请求
同一文件的第 3 条请求：
- `prompt_tokens = 1`
- `prompt_ms = 38.88`
- `prompt_tps = 25.72`
- `decode_tokens = 96`
- `decode_ms = 3344.56`
- `decode_tps = 28.70`

解释：
- 这里的 `prompt_tokens = 1` 不是模型 prompt 变短了，而是 slot/LCP 复用后只需要重新评估 1 个 token
- 所以这组数据说明的是 **缓存复用后 prefill 开销显著下降**
- 不应该拿这组 `prefill_speed_tps` 直接和冷启动做同口径比较

### 视觉侧耗时
来自 [local_smolvlm2_server_trace.log](/3.2T/work/fpga-fa/docs/daily/archive/20260407_233500_smolvlm2_local_cpu_eval/local_smolvlm2_server_trace.log) 的统计：

冷请求（13 个 image slice）：
- 平均 `image slice encoded`：`355.85 ms`
- 平均 `image decoded`：`157.23 ms`
- 平均 `image processed`：`513.31 ms`

热请求（13 个 image slice）：
- 平均 `image slice encoded`：`324.77 ms`
- 平均 `image decoded`：`164.69 ms`
- 平均 `image processed`：`489.54 ms`

说明：
- 视觉侧本身就是明显的大头
- 这个模型在 CPU 上跑视觉编码成本并不低
- 后续如果要优化 VLM 端到端时延，仅盯文本解码不够，视觉前端和图像 slice 组织同样重要

## Trace 与算子尺寸分析
### 模型主参数
来自 [local_smolvlm2_trace_summary.json](/3.2T/work/fpga-fa/docs/daily/archive/20260407_233500_smolvlm2_local_cpu_eval/local_smolvlm2_trace_summary.json)：
- 文本 hidden size：`960`
- 文本层数：`32`
- 文本 heads：`15`
- 文本 kv heads：`5`
- `head_dim = 64`
- FFN hidden：`2560`
- 视觉 hidden：`768`
- 视觉 heads：`12`
- 图像输入尺寸：`512`
- patch size：`16`

### 一次图文请求的 token 组织
当前服务端展开出的 prompt 里包含：
- `12` 个 `<row_i_col_j>` marker
- `1` 个 `<global-img>` marker
- 总计 `13` 个 image slice
- 每个 slice 生成 `64` 个图像 token
- 总图像 token 数：`832`

结合服务端日志：
- 文本 token 约 `47`
- 总 prompt token 约 `879`
- 即：**视觉 token 占主导**

### 文本 Transformer 主干 trace
以单层为例，主干可抽象为：
1. RMSNorm：`[960] -> [960]`
2. `q_proj`：`[960] x W[960, 960] -> [960]`
3. `k_proj`：`[960] x W[960, 320] -> [320]`
4. `v_proj`：`[960] x W[960, 320] -> [320]`
5. Q heads reshape：`15 x 64`
6. K/V heads reshape：`5 x 64`
7. GQA attention：
   - query heads `15`
   - kv heads `5`
   - group size `3`
   - 每个 query head 与缓存中的 key 做 `head_dim = 64` 的点积
   - softmax 后再与 value 做加权求和
8. `o_proj`：`[960] x W[960, 960] -> [960]`
9. FFN gate / up：
   - `gate_proj`：`[960] x W[960, 2560] -> [2560]`
   - `up_proj`：`[960] x W[960, 2560] -> [2560]`
10. SwiGLU
11. `down_proj`：`[2560] x W[2560, 960] -> [960]`

### 对 HLS / FPGA 设计的意义
这份 trace 对后续 FPGA 设计有两个直接意义：
1. 纯文本侧最可复用的基础算子仍然是 `GEMV / small-batch MATMUL`
   - `q_proj / k_proj / v_proj / o_proj`
   - `gate_proj / up_proj / down_proj`
2. VLM 场景下 attention 之外的线性层开销非常大
   - 如果后面只做 `flash-attn`，对完整 VLM 端到端提升是有限的
   - 若想继续提升 VLM，`matmul/gemv` 后端复用会比只盯 attention 更有意义

## 精度分析
### 当前已验证的“功能正确性”
- `llama-mtmd-cli` 能输出合理图文描述与 OCR 结果
- `completion` 路径也能输出合理文本
- 当前示例图片的 OCR 结果能识别出 `that`

### 当前还不能直接给出完整 benchmark 精度分数
原因不是模型不通，而是本机缺：
- `FullTest.json`
- `OCRBench.json`
- 或 `sample_100.json`

也就是说：
- `inference/problem/data_extracted` 图片集在本机有
- 但 `acc_eval.py` 需要的题目 JSON 不在本机

所以当前精度结论是：
- **定性验证已通过**
- **定量 benchmark 分数还缺题目 JSON 输入，暂时不能给出最终分数**

## xclbin 当前状态
这部分仍然是 FPGA 板测前的主阻塞。

来自 [v++_fa_attention_kernel.log](/3.2T/work/fpga-fa/docs/daily/archive/20260407_233500_smolvlm2_local_cpu_eval/v++_fa_attention_kernel.log)：
- `xpfm` 已经打通
- `v++ link` 已经进入 `vpl -> impl`
- 最终失败在实现阶段资源超限

关键错误：
- 需要 `395` 个 RAMB18/36 兼容单元，而 KV260 只有 `288`
- 需要 `152` 个 RAMB36E2，而 KV260 只有 `144`

结论：
- 当前不能上传到 KV260 做 FPGA attention 验证
- 缺的不是平台，不是 `xpfm`，而是 **当前 HLS kernel 在 KV260 上 BRAM 过量，导致 `xclbin` 无法生成**

## 下一步建议
1. 继续保留当前 `tmux` 服务：
   - `tmux attach -t smolvlm2_server_cpu`
2. 如果要继续本机 benchmark：
   - 先补 `FullTest.json / OCRBench.json / sample_100.json`
   - 然后可直接跑 `acc_eval.py`
3. 如果要让 `problem/throughput_eval.py` 原样可用：
   - 修 `llama-server` 对 SmolVLM2 chat template 的 marker 注入问题
   - 或单独提供一个 completion 版适配脚本
4. 如果要继续 FPGA 主线：
   - 优先压 HLS kernel 的 BRAM 占用
   - 当前 `xclbin` 失败已经明确是 BRAM，而不是平台链路
5. 如果目标是 VLM 整体提速：
   - 后续不应只盯 `flash-attn`
   - 应同时考虑 `q/k/v/o` 和 FFN 线性层的 `gemv/matmul` 复用路线
