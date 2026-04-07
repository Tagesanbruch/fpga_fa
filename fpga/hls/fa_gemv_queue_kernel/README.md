# GEMV 任务队列 HLS 原型

这个目录实现的是：

- 基于 attention 内部可复用 `dot-product` 微核的 GEMV 原型
- 在 HLS 层加入 **任务队列 / batched 提交** 的版本

目标不是直接证明“计算本体更快”，而是先回答两个实际问题：

1. attention 内部的点积微核能不能复用到 GEMV
2. 把多个 GEMV 任务塞进一次 HLS kernel 调用后，功能/精度是否稳定，控制开销是否合理

## 输入形式

队列 kernel 接受：

- 拼接后的 `x_all`
- 拼接后的 `w_all`
- 拼接后的 `y_all`
- `GemvTaskDesc[]` 描述符数组
- `task_count`

每个任务描述符包含：

- `x_offset_elems`
- `w_offset_elems`
- `y_offset_elems`
- `in_dim`
- `out_dim`

## 测试重点

TB 会同时检查：

1. `queue_hls` 是否逐点匹配 `strict`
2. `queue_hls` 是否逐点匹配“逐任务串行调用 HLS”
3. 与 FP32 参考相比的：
   - `MAE`
   - `MaxAE`
4. host 侧软件执行时间：
   - `strict_ms`
   - `serial_hls_ms`
   - `queue_hls_ms`

注意：

- 这里的 `queue_speedup` 是 **软件 C-sim 层的批处理观察值**
- 它不能直接等同于板上硬件速度提升
- 真正的硬件收益要结合 HLS `csynth` latency 与后续 XRT launch 开销一起看
