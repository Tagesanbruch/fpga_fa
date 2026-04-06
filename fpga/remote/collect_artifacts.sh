#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/../.." && pwd)
REMOTE_HOST=${REMOTE_HOST:-luci@100.111.132.110}
REMOTE_DIR=${REMOTE_DIR:-/3.2T/work/vivado/fpga-fa}
LOCAL_DIR=${LOCAL_DIR:-$ROOT_DIR/fpga/artifacts/kv260}

mkdir -p "$LOCAL_DIR"

rsync -az \
  "$REMOTE_HOST:$REMOTE_DIR/fpga/build/kv260/" \
  "$LOCAL_DIR/"

echo "[INFO] Collected artifacts into $LOCAL_DIR"
