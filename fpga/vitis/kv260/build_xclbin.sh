#!/usr/bin/env bash
set -euo pipefail

THIS_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "$THIS_DIR/../../.." && pwd)

source "$ROOT_DIR/fpga/remote/env_detect.sh"

TARGET=${TARGET:-hw}
PART=${PART:-xck26-sfvc784-2LV-c}
CLOCK_NS=${CLOCK_NS:-5.0}
BUILD_DIR=${BUILD_DIR:-"$ROOT_DIR/fpga/build/kv260"}
XO=${XO:-"$BUILD_DIR/fa_attention_kernel.xo"}
XCLBIN=${XCLBIN:-"$BUILD_DIR/fa_attention_kernel.xclbin"}
CFG=${CFG:-"$THIS_DIR/kv260.cfg"}
KERNEL_NAME=${KERNEL_NAME:-fa_attention_kernel}

mkdir -p "$BUILD_DIR"

echo "[INFO] Building XO: $XO"
(
  cd "$ROOT_DIR/fpga/hls/fa_attention_kernel"
  make xo XO="$XO" PART="$PART" CLOCK_NS="$CLOCK_NS"
)

echo "[INFO] Linking XCLBIN with platform: $KV260_PLATFORM"
v++ \
  --link \
  --target "$TARGET" \
  --platform "$KV260_PLATFORM" \
  --config "$CFG" \
  --kernel "$KERNEL_NAME" \
  --save-temps \
  --temp_dir "$BUILD_DIR/_link" \
  -o "$XCLBIN" \
  "$XO"

echo "[INFO] XCLBIN generated at $XCLBIN"
