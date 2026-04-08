#!/usr/bin/env bash
set -euo pipefail

THIS_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "$THIS_DIR/../../.." && pwd)

BUNDLE_DIR=${BUNDLE_DIR:-"$ROOT_DIR/inference/ci/artifacts/kv260_rtl_add_seq_bundle"}
BUILD_DIR=${BUILD_DIR:-"$ROOT_DIR/fpga/build/kv260_rtl_add_seq"}

mkdir -p "$BUNDLE_DIR/bitstreams" "$BUNDLE_DIR/src" "$BUNDLE_DIR/bin" "$BUNDLE_DIR/reports"

cp "$BUILD_DIR/rtl_add_seq_kernel.xclbin" "$BUNDLE_DIR/bitstreams/"
cp "$ROOT_DIR/inference/ci/artifacts/kv260_add_seq_bundle/src/add_seq_runner.cpp" "$BUNDLE_DIR/src/rtl_add_seq_runner.cpp"
cp "$ROOT_DIR/inference/ci/artifacts/kv260_add_seq_bundle/src/Makefile" "$BUNDLE_DIR/src/Makefile"
if [ -f "$BUILD_DIR/_link/link/int/xo/rtl_add_seq_kernel/rtl_add_seq_kernel/kernel.xml" ]; then
  cp "$BUILD_DIR/_link/link/int/xo/rtl_add_seq_kernel/rtl_add_seq_kernel/kernel.xml" "$BUNDLE_DIR/reports/kernel.xml"
fi

cat > "$BUNDLE_DIR/bin/run_rtl_add_seq_test.sh" <<'EOT'
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
cd "$ROOT_DIR/src"
cp rtl_add_seq_runner.cpp add_seq_runner.cpp
sed -i 's/add_seq_kernel:{add_seq_kernel_1}/rtl_add_seq_kernel:{rtl_add_seq_kernel_1}/' add_seq_runner.cpp
sed -i 's/add_seq_kernel/rtl_add_seq_kernel/g' add_seq_runner.cpp
touch Makefile add_seq_runner.cpp
make clean
make
./add_seq_runner --xclbin "$ROOT_DIR/bitstreams/rtl_add_seq_kernel.xclbin" --kernel 'rtl_add_seq_kernel:{rtl_add_seq_kernel_1}' --length 1024 --base-add 1 --dump --verbose
EOT
chmod +x "$BUNDLE_DIR/bin/run_rtl_add_seq_test.sh"

cat > "$BUNDLE_DIR/add_seq_rtl_pynq_demo.ipynb" <<'EOT'
{
  "cells": [
    {
      "cell_type": "markdown",
      "metadata": {},
      "source": [
        "# KV260 RTL add_seq PYNQ/XRT 验证\n",
        "\n",
        "这个 notebook 用来加载 `rtl_add_seq_kernel.xclbin`，查看 overlay 信息，并调用 native XRT runner 做板端验证。\n"
      ]
    },
    {
      "cell_type": "code",
      "execution_count": null,
      "metadata": {},
      "outputs": [],
      "source": [
        "from pathlib import Path\n",
        "import subprocess\n",
        "import pynq\n",
        "from pynq import Overlay\n",
        "bundle_dir = Path.cwd()\n",
        "xclbin = bundle_dir / 'bitstreams' / 'rtl_add_seq_kernel.xclbin'\n",
        "print('PYNQ version:', pynq.__version__)\n",
        "print('xclbin:', xclbin)\n",
        "ol = Overlay(str(xclbin))\n",
        "print('Overlay loaded')\n",
        "print('IP dict keys:', list(ol.ip_dict.keys()))\n",
        "print('Memories:', list(getattr(ol, 'mem_dict', {}).keys()))\n"
      ]
    },
    {
      "cell_type": "code",
      "execution_count": null,
      "metadata": {},
      "outputs": [],
      "source": [
        "subprocess.run(['bash', 'bin/run_rtl_add_seq_test.sh'], check=True)\n"
      ]
    }
  ],
  "metadata": {
    "kernelspec": {
      "display_name": "Python 3",
      "language": "python",
      "name": "python3"
    },
    "language_info": {
      "name": "python",
      "version": "3.x"
    }
  },
  "nbformat": 4,
  "nbformat_minor": 5
}
EOT

cat > "$BUNDLE_DIR/README.txt" <<'EOT'
KV260 `rtl_add_seq_kernel` 最小 RTL 验证包
=========================================

用途：
- 验证纯 RTL kernel 是否能通过当前 KV260 platform 生成并运行 `xclbin`
- 验证 DDR 读 / 加法 / DDR 写 回路

目录：
- `bitstreams/rtl_add_seq_kernel.xclbin`
- `src/rtl_add_seq_runner.cpp`
- `src/Makefile`
- `bin/run_rtl_add_seq_test.sh`
- `add_seq_rtl_pynq_demo.ipynb`
- `reports/kernel.xml`（若构建产出）

板端使用：
1. 上传整个目录到 KV260
2. 进入 bundle 目录
3. 运行：

   ./bin/run_rtl_add_seq_test.sh

或在 Jupyter 中打开 `add_seq_rtl_pynq_demo.ipynb`
EOT

echo "[INFO] RTL bundle prepared at $BUNDLE_DIR"
