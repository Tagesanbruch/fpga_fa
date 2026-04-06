#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/../.." && pwd)
REMOTE_HOST=${REMOTE_HOST:-luci@100.111.132.110}
REMOTE_DIR=${REMOTE_DIR:-/3.2T/work/vivado/fpga-fa}
TARGET=${TARGET:-hw}

ssh "$REMOTE_HOST" "bash -lc '
  set -euo pipefail
  cd \"$REMOTE_DIR\"
  source fpga/remote/env_detect.sh
  make -C fpga/hls/fa_attention_kernel csim
  TARGET=\"$TARGET\" fpga/vitis/kv260/build_xclbin.sh
'"

echo "[INFO] Remote build completed on $REMOTE_HOST:$REMOTE_DIR"
