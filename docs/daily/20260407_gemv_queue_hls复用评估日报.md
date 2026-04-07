# 2026-04-07 GEMV 复用与任务队列 HLS 评估日报

## 1. 摘要

本轮工作在 HLS 层完成了两条新原型：

1. `fa_gemv_kernel`：将 attention 内部的 Q8.8 `dot-product` 微核抽取出来，复用到 GEMV。
2. `fa_gemv_queue_kernel`：在 GEMV 基础上加入任务队列/批量提交接口，验证控制面是否成立，以及它对功能、精度与潜在加速比的影响。

本轮归档数据以 `docs/daily/archive/20260407_025148_gemv_queue_reuse_snapshot/` 为准。

## 2. 变更概览

### 2.1 共享点积微核

新增：

- `fpga/common/fa_q8_8_dot.hpp`
- `fpga/common/fa_q8_8_dot.cpp`

作用：

- 提供 `dotprod_q8_8_single()`，供 GEMV 复用
- 提供 `dotprod_q8_8_pair()`，供 attention 的双行点积复用

对应改动：

- `fpga/common/fa_q8_8_qk_dotprod_slice.cpp` 改为调用共享微核
- `fpga/common/fa_q8_8_linear.cpp` 的 `compute_tile()` 改为调用共享微核

### 2.2 普通 GEMV 原型

新增/更新：

- `fpga/common/fa_q8_8_linear.hpp`
- `fpga/common/fa_q8_8_linear.cpp`
- `fpga/hls/fa_gemv_kernel/README.md`
- `fpga/hls/fa_gemv_kernel/run_hls.tcl`
- `fpga/hls/fa_gemv_kernel/Makefile`

### 2.3 队列 GEMV 原型

新增：

- `fpga/hls/fa_gemv_queue_kernel/fa_gemv_queue_kernel.cpp`
- `fpga/hls/fa_gemv_queue_kernel/tb/test_fa_gemv_queue_kernel.cpp`
- `fpga/hls/fa_gemv_queue_kernel/run_hls.tcl`
- `fpga/hls/fa_gemv_queue_kernel/Makefile`
- `fpga/hls/fa_gemv_queue_kernel/README.md`

根 `Makefile` 新增：

- `make fpga-gemv-csim`
- `make fpga-gemv-xo`
- `make fpga-gemv-queue-csim`
- `make fpga-gemv-queue-xo`

## 3. TB 设计

### 3.1 普通 GEMV TB

`fpga/hls/fa_gemv_kernel/tb/test_fa_gemv_kernel.cpp` 覆盖：

- 小矩形、宽矩形、最大尺寸
- 均匀分布与高斯分布输入
- 检查 HLS 输出是否逐点匹配 strict 参考
- 统计相对 FP32 的 `MAE/MaxAE`

### 3.2 队列 GEMV TB

`fpga/hls/fa_gemv_queue_kernel/tb/test_fa_gemv_queue_kernel.cpp` 同时比较三条路径：

1. `queue_strict`
2. `serial_hls`：逐任务串行调用 `run_gemv_tiled_hls`
3. `queue_hls`：一次调用队列 kernel

验证项：

- `queue_hls == strict`
- `queue_hls == serial_hls`
- 相对 FP32 的 `MAE/MaxAE`
- 主机侧执行时间：
  - `strict_ms`
  - `serial_hls_ms`
  - `queue_hls_ms`
  - `queue_speedup = serial_hls_ms / queue_hls_ms`

## 4. 功能与精度结果

### 4.1 普通 GEMV

归档日志：

- `docs/daily/archive/20260407_025148_gemv_queue_reuse_snapshot/logs/fpga-gemv-csim.log`

结果：

- `exact_vs_strict=PASS`
- 最坏 case：`wide_gaussian_128x128`
- 最坏误差：
  - `MAE = 0.002645`
  - `MaxAE = 0.005844`

这说明：

- 从 attention 中抽出的共享点积微核在 GEMV 上没有破坏数值口径
- 当前 GEMV HLS 版可以作为后续 GEMM/GEMV 复用探索的稳定基线

### 4.2 队列 GEMV

归档日志：

- `docs/daily/archive/20260407_025148_gemv_queue_reuse_snapshot/logs/fpga-gemv-queue-csim.log`

结果：

- `exact_vs_strict=PASS`
- 所有 case 均 `serial_match=yes`
- 最坏 case：`uniform_q4_64x64`
- 最坏误差：
  - `MAE = 0.002488`
  - `MaxAE = 0.005844`

这说明：

- 队列控制面没有引入新的数值误差
- 当前队列实现本质上是“控制层聚合”，而不是“改变算术结果”

## 5. 加速比观察

### 5.1 C-sim 直接观察

队列版 `queue_speedup`：

- `64x64, q4, task_count=4`：`1.008602x`
- `64x128, gaussian, task_count=4`：`0.992600x`
- `128x128, q8, task_count=8`：`0.934803x`
- `256x256, gaussian, task_count=8`：`1.007922x`

均值：

- 算术平均：`0.985982x`
- 几何平均：`0.985508x`

### 5.2 结论

在当前 HLS/C++ 层，任务队列**几乎没有带来计算本体的加速**，原因很直接：

- `run_gemv_queue_tiled_hls()` 内部仍然是顺序调用 `run_gemv_tiled_hls()`
- 它减少的是“调用边界数量”，不是“每个任务内部的计算量”
- 在软件 C-sim 中，函数调用开销本来就很小，因此几乎看不到正向收益

因此，队列化的真正意义是：

- 为板级 XRT/host runtime 预留 batched submit 接口
- 未来在真实硬件上减少多次 kernel launch 的控制开销
- 而不是指望在当前 HLS C-sim 层直接出现显著算力提升

## 6. 综合结果

### 6.1 普通 GEMV (`fa_gemv_kernel`)

归档报告：

- `docs/daily/archive/20260407_025148_gemv_queue_reuse_snapshot/artifacts/fa_gemv_kernel_build/fa_gemv_kernel_hls/solution1/syn/report/csynth.rpt`

主要指标：

- `BRAM = 64 (22%)`
- `DSP = 15 (1%)`
- `FF = 9489 (4%)`
- `LUT = 10555 (9%)`
- `Estimated Fmax = 273.97 MHz`
- `Loop Constraint Status = All loop constraints were satisfied`

关键循环：

- `load_x_tile` 对应 loop：`II = 1`
- `dotprod_q8_8_single` 内层主循环：`Final II = 8`

### 6.2 队列 GEMV (`fa_gemv_queue_kernel`)

归档报告：

- `docs/daily/archive/20260407_025148_gemv_queue_reuse_snapshot/artifacts/fa_gemv_queue_kernel_build/fa_gemv_queue_kernel_hls/solution1/syn/report/csynth.rpt`

主要指标：

- `BRAM = 80 (27%)`
- `DSP = 10 (~0%)`
- `FF = 23581 (10%)`
- `LUT = 55427 (47%)`
- `Estimated Fmax = 273.97 MHz`

关键循环：

- 任务描述符 preload：`II = 1`
- profile clear loop：`II = 1`
- `dotprod_q8_8_single` 内层主循环：`Final II = 8`

### 6.3 资源变化解释

相对普通 GEMV，队列版：

- LUT 从 `9%` 上升到 `47%`
- FF 从 `4%` 上升到 `10%`
- BRAM 从 `22%` 上升到 `27%`

原因不是新增了大量乘法器，而是：

- 任务描述符缓存
- 额外 profile 与控制逻辑
- 更大的顶层状态机与地址计算
- `run_gemv_tiled_hls()` 被整体内联到 queue top 后，控制面显著膨胀

这说明：

- 队列化在 HLS 层首先增加的是**控制成本**
- 如果没有板级 launch 开销这一收益去抵消，单看综合结果并不“便宜”

## 7. dot-product 做大的必要性分析

这是本轮最重要的理论问题之一。

### 7.1 为什么需要“做大”点积

不做大点积时：

- 每个周期处理的 Q8.8 乘加数很少
- DSP 利用率低
- 算子吞吐上限很低

做大点积时：

- 可以通过更高 lane 数并行计算更多 MAC
- 更适合作为 attention / GEMV / GEMM 的共享算术微核
- 也更容易为未来的 systolic / tiled GEMM 打基础

### 7.2 为什么不能只把点积做大

当前共享微核 `dotprod_q8_8_single()` 已经设置：

- `kDotLanes = 8`
- 内层 `UNROLL factor = 8`
- 乘法绑定 DSP

但综合结果表明，主循环仍然是：

- `Final II = 8`

HLS 日志明确指出根因：

- `gmem1` 上的权重读存在 carried dependence / memory port limit

也就是说，现在的瓶颈不是“乘法器太少”，而是：

- 权重还是直接从外部总线流入
- 没有本地块缓存 / 权重 staging
- 点积宽度一旦增加，总线压力也同步上升
- 最终 II 仍然被 DDR/AXI 访存限制住

### 7.3 参数对计算能力的影响

从当前原型可以总结出几个一阶参数：

1. `kDotLanes`
- 决定每次点积并行展开的乘法器宽度
- 理想情况下越大越快
- 但前提是权重和输入能以相同速率喂入

2. `in_dim`
- 直接决定每行点积长度
- 在当前结构下，`in_dim` 越大，受访存带宽影响越明显

3. `out_dim`
- 决定输出行数
- 在普通 GEMV 中近似线性影响总 MAC 数
- 在队列版中还会放大控制面开销

4. `task_count`
- 影响队列控制层长度
- 当前实现里主要影响顶层串行循环长度
- 不改变单任务点积吞吐

5. `本地缓存容量`
- 是后续最值得投入的参数
- 如果把权重先搬入片上 buffer，再驱动 dot-product，才有机会真正把更宽的点积 lane 转换为吞吐提升

## 8. 对 attention / GEMV / GEMM 复用的判断

### 8.1 当前可以确认的

- attention 内部点积微核可以稳定复用到 GEMV
- 数值口径没有出问题
- 复用后的 GEMV 资源仍然很轻

### 8.2 当前不该过度乐观的

- 不能因为点积微核可复用，就认为 GEMM 会自然高效
- GEMM 相比 GEMV，更依赖：
  - 权重块缓存
  - 输入块复用
  - 更强的 tile 调度/数据复用

### 8.3 当前最合理的路线

1. 保留 attention 的主核独立演进
2. 将 `dot-product` 作为共享算术微核继续保留
3. 先让 GEMV 在片上缓存/权重复用层面更合理
4. 再基于同一微核设计 GEMM 的外层调度，而不是把 attention top 直接“改造成 GEMM”

## 9. 归档内容

本轮已经完成以下归档：

- `docs/daily/archive/20260407_025148_gemv_queue_reuse_snapshot/logs/`
- `docs/daily/archive/20260407_025148_gemv_queue_reuse_snapshot/artifacts/fa_gemv_kernel_build/`
- `docs/daily/archive/20260407_025148_gemv_queue_reuse_snapshot/artifacts/fa_gemv_queue_kernel_build/`
- `docs/daily/archive/20260407_025148_gemv_queue_reuse_snapshot/MANIFEST.md`

其中包含：

- csim 日志
- `.xo`
- TB 可执行文件
- `syn/report/csynth.rpt`
- `impl/verilog/` 下的生成 RTL

## 10. 结论与下一步

### 10.1 本轮结论

- GEMV 复用是成立的，功能和精度都稳
- 任务队列接口在 HLS 层是成立的，但不会自动带来计算加速
- 共享 dot-product 微核是正确方向，但当前仍然明显受权重总线限制
- 如果后续继续追求 GEMV/GEMM 加速，重点应转向：
  - 权重本地缓存
  - tile 级数据复用
  - 板级 launch amortization

### 10.2 下一步建议

1. 在 GEMV 中加入权重 tile buffer，重新评估 `dotprod_q8_8_single` 的 `II`
2. 再看 queue 版本是否值得保留为板级 runtime 接口
3. 若要继续 GEMM，优先做独立 HLS kernel，而不是把 queue GEMV 直接放大
