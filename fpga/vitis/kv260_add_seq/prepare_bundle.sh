#!/usr/bin/env bash
set -euo pipefail

THIS_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "$THIS_DIR/../../.." && pwd)

BUNDLE_DIR=${BUNDLE_DIR:-"$ROOT_DIR/inference/ci/artifacts/kv260_add_seq_bundle"}
BUILD_DIR=${BUILD_DIR:-"$ROOT_DIR/fpga/build/kv260_add_seq"}

mkdir -p "$BUNDLE_DIR/bitstreams" "$BUNDLE_DIR/src" "$BUNDLE_DIR/bin"

cp "$BUILD_DIR/add_seq_kernel.xclbin" "$BUNDLE_DIR/bitstreams/"
cp "$ROOT_DIR/fpga/host/add_seq_runner/add_seq_runner.cpp" "$BUNDLE_DIR/src/"
cp "$ROOT_DIR/fpga/host/add_seq_runner/Makefile" "$BUNDLE_DIR/src/"

cat > "$BUNDLE_DIR/bin/run_add_seq_test.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
cd "$ROOT_DIR/src"
make
./add_seq_runner --xclbin "$ROOT_DIR/bitstreams/add_seq_kernel.xclbin" --length 1024 --base-add 1 --dump
EOF
chmod +x "$BUNDLE_DIR/bin/run_add_seq_test.sh"

cat > "$BUNDLE_DIR/README.txt" <<'EOF'
KV260 `add_seq` 最小验证包
===========================

用途：
- 验证 `xclbin` 能否在 KV260 上正常加载
- 验证最基本的 DDR 读 / 计算 / DDR 写 回路

目录：
- `bitstreams/add_seq_kernel.xclbin`
- `src/add_seq_runner.cpp`
- `src/Makefile`
- `bin/run_add_seq_test.sh`

板端使用：
1. 上传整个目录到 KV260
2. 进入 bundle 目录
3. 运行：

   ./bin/run_add_seq_test.sh

成功时会输出 `[PASS] add_seq kernel verified`
EOF

echo "[INFO] Bundle prepared at $BUNDLE_DIR"
