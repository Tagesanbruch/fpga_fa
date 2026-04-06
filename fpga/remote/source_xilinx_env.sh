#!/usr/bin/env bash
set -euo pipefail

MODE=${1:-all}
XILINX_ROOT_DEFAULT=${XILINX_ROOT_DEFAULT:-/home/luci/tools/Xilinx}
XILINX_VERSION=${XILINX_VERSION:-2023.1}

source_if_exists() {
  local candidate="$1"
  if [[ -f "$candidate" ]]; then
    # shellcheck disable=SC1090
    source "$candidate"
    return 0
  fi
  return 1
}

case "$MODE" in
  vitis)
    source_if_exists "$XILINX_ROOT_DEFAULT/Vitis/$XILINX_VERSION/settings64.sh"
    ;;
  hls)
    source_if_exists "$XILINX_ROOT_DEFAULT/Vitis_HLS/$XILINX_VERSION/settings64.sh"
    ;;
  vivado)
    source_if_exists "$XILINX_ROOT_DEFAULT/Vivado/$XILINX_VERSION/settings64.sh"
    ;;
  all)
    source_if_exists "$XILINX_ROOT_DEFAULT/Vitis/$XILINX_VERSION/settings64.sh" || true
    source_if_exists "$XILINX_ROOT_DEFAULT/Vitis_HLS/$XILINX_VERSION/settings64.sh" || true
    source_if_exists "$XILINX_ROOT_DEFAULT/Vivado/$XILINX_VERSION/settings64.sh" || true
    ;;
  *)
    echo "[ERR] unsupported mode '$MODE' in source_xilinx_env.sh" >&2
    exit 1
    ;;
esac
