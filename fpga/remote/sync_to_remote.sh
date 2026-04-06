#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/../.." && pwd)
REMOTE_HOST=${REMOTE_HOST:-luci@100.111.132.110}
REMOTE_DIR=${REMOTE_DIR:-/3.2T/work/vivado/fpga-fa}

rsync -az \
  --delete \
  --exclude .git \
  --exclude .venv \
  --exclude build \
  --exclude syn \
  --exclude logs \
  --exclude fpga/build \
  --exclude fpga/artifacts \
  --exclude inference/problem/data_extracted \
  "$ROOT_DIR/" "$REMOTE_HOST:$REMOTE_DIR/"

echo "[INFO] Synced repository to $REMOTE_HOST:$REMOTE_DIR"
