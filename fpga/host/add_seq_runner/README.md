# KV260 `add_seq` 验证 Runner

这个目录提供一个最小的 XRT 原生测试程序，用来验证：

- `xclbin` 可以被 KV260 板端加载
- AXI DDR 读写通路工作正常
- HLS kernel 调用与结果回传正常

## 构建

在 KV260 板端执行：

```bash
make
```

如果 XRT 头文件或库不在系统默认路径，可以先设置：

```bash
export XILINX_XRT=/opt/xilinx/xrt
```

## 运行

```bash
./add_seq_runner --xclbin ./add_seq_kernel.xclbin --length 1024 --base-add 1 --dump
```
