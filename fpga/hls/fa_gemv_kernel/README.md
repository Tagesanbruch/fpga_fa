# GEMV HLS 原型

这个目录实现的是基于 attention 内部 Q8.8 `dot-product` 微核复用的 GEMV 原型。

目标有两层：

1. 验证 attention 内部点积微核是否能在不改数值口径的前提下复用到 GEMV。
2. 作为后续 GEMV/GEMM 复用探索的最小可综合、可验证基线。

## 设计口径

- 输入/权重/输出均采用 Q8.8 定点格式。
- `dotprod_q8_8_single()` 作为共享微核，来自 `fpga/common/fa_q8_8_dot.cpp`。
- 当前实现优先保证：
  - 与 strict 参考逐点一致
  - HLS 能稳定综合导出 `.xo`
  - 资源规模足够小，便于后续和 attention/queue 方案做对照

## 测试内容

`tb/test_fa_gemv_kernel.cpp` 会验证：

1. HLS 版本是否逐点匹配 strict 参考
2. 与 FP32 参考相比的 `MAE/MaxAE`
3. 不同矩阵形状下的 MAC 数量变化

## 当前结论

- 功能、精度已经稳定
- 资源占用较轻，适合作为复用基线
- 但共享 `dot-product` 微核目前仍直接从权重总线读数据，因此 `II` 会被 `gmem1` 访存限制住
- 这说明后续若要继续做大 dot-product 或推进到 GEMM，重点不只是“加乘法器”，还要同步引入权重本地缓存/分块搬运
