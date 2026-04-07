KV260 最小通路验证包（add_seq）
=================================

用途：
- 先验证 KV260 板端的 XRT + `xclbin` 基础通路
- 避免一上来就把复杂 attention 核带到板端，便于快速判断平台、DDR、kernel 调用是否正常

目录说明：
- `bitstreams/`
  - 放 `add_seq_kernel.xclbin`
- `src/`
  - 板端编译用源文件
- `bin/`
  - 板端启动脚本

推荐板端步骤：
1. 上传整个目录到 KV260
2. 进入 bundle 目录
3. 执行：

   ./bin/run_add_seq_test.sh

期望结果：
- 输出 `[PASS] add_seq kernel verified`
- 并打印若干输入/输出样例
