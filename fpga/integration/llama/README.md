# llama.cpp FPGA 运行时说明

这个目录保存的是 `llama.cpp` 与当前 HLS/XRT attention kernel 之间的桥接运行时。

## 组成

- `fa_task_queue_runtime.hpp`
- `fa_task_queue_runtime.cpp`
- `inference/xcomp/llama.cpp/ggml/src/ggml-fpga/ggml-fpga.cpp`

## 当前支持范围

当前后端只加速满足以下条件的 `FLASH_ATTN_EXT`：

- `D = 64`
- `seq_len <= 256`
- `q_len == kv_len`
- 稠密 Q8.8 布局
- `mask != nullptr` 时按 causal attention 处理

为了适配 HLS kernel 的 tile 约束，backend 在 **causal 场景** 下会自动将 `seq_len` 向上补齐到 64 的倍数；补齐行不会写回到最终输出。

## 当前限制

- 长 prefill (`seq_len > 256`) 仍然回退到 CPU
- decode 风格 (`q_len != kv_len`) 仍然回退到 CPU
- sinks / softcap / 任意 mask 目前都未映射到 HLS kernel

因此当前集成更接近：

- 为板级 bring-up 和短序列验证准备真实 `llama-server` 路径
- 而不是已经完成对完整 VLM 推理链的端到端硬件加速
