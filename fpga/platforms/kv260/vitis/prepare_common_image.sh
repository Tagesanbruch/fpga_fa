#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PLATFORM_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
REPO_ROOT=$(cd "$PLATFORM_DIR/../../.." && pwd)

COMMON_TAR=${1:-"$REPO_ROOT/third_party/xilinx-zynqmp-common-v2023.1_05080224.tar.gz"}
WORK_DIR=${2:-"$PLATFORM_DIR/common_image"}
SDK_DIR=${3:-"$PLATFORM_DIR/common_sdk/xilinx-zynqmp-common-v2023.1-sdk"}

if [[ ! -f "$COMMON_TAR" ]]; then
  echo "[ERR] common image tar not found: $COMMON_TAR" >&2
  exit 1
fi

mkdir -p "$WORK_DIR"
mkdir -p "$(dirname "$SDK_DIR")"

echo "[INFO] Extracting common image tar: $COMMON_TAR"
tar -xzf "$COMMON_TAR" -C "$WORK_DIR"

COMMON_DIR=$(find "$WORK_DIR" -maxdepth 1 -type d -name 'xilinx-zynqmp-common*' | head -n 1 || true)
if [[ -z "$COMMON_DIR" ]]; then
  echo "[ERR] extracted common image directory not found under $WORK_DIR" >&2
  exit 1
fi

if [[ ! -x "$COMMON_DIR/sdk.sh" ]]; then
  echo "[ERR] sdk.sh not found or not executable under $COMMON_DIR" >&2
  exit 1
fi

echo "[INFO] Installing sysroot SDK into: $SDK_DIR"
"$COMMON_DIR/sdk.sh" -y -d "$SDK_DIR"

echo "[INFO] Common image prepared"
echo "[INFO] Common dir : $COMMON_DIR"
echo "[INFO] SDK dir    : $SDK_DIR"
