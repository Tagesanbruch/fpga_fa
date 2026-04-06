#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/../.." && pwd)
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

if ! command -v v++ >/dev/null 2>&1; then
  source_if_exists "$XILINX_ROOT_DEFAULT/Vitis/$XILINX_VERSION/settings64.sh" || true
fi

if ! command -v vitis_hls >/dev/null 2>&1; then
  source_if_exists "$XILINX_ROOT_DEFAULT/Vitis_HLS/$XILINX_VERSION/settings64.sh" || true
fi

find_kv260_platform() {
  local roots=(
    "${KV260_PLATFORM_DIR:-}"
    "$ROOT_DIR/fpga/platforms"
    "$HOME/xilinx/platforms"
    "$HOME/platforms"
    "$XILINX_ROOT_DEFAULT/Downloads"
    "$XILINX_ROOT_DEFAULT/SharedData"
    "/opt/xilinx/platforms"
  )

  for root in "${roots[@]}"; do
    [[ -n "$root" && -d "$root" ]] || continue
    local hit
    hit=$(find "$root" -type f -name '*kv260*.xpfm' 2>/dev/null | head -n 1 || true)
    if [[ -n "$hit" ]]; then
      echo "$hit"
      return 0
    fi
  done
  return 1
}

if [[ -z "${KV260_PLATFORM:-}" ]]; then
  if platform=$(find_kv260_platform); then
    export KV260_PLATFORM="$platform"
  fi
fi

if ! command -v v++ >/dev/null 2>&1; then
  echo "[ERR] v++ not found on PATH after sourcing Xilinx environment" >&2
  return 1 2>/dev/null || exit 1
fi

if ! command -v vitis_hls >/dev/null 2>&1; then
  echo "[ERR] vitis_hls not found on PATH after sourcing Xilinx environment" >&2
  return 1 2>/dev/null || exit 1
fi

if [[ -z "${KV260_PLATFORM:-}" ]]; then
  echo "[ERR] KV260_PLATFORM is not set and no kv260 .xpfm could be found automatically" >&2
  return 1 2>/dev/null || exit 1
fi

echo "[INFO] Xilinx version : $XILINX_VERSION"
echo "[INFO] vitis_hls      : $(command -v vitis_hls)"
echo "[INFO] v++            : $(command -v v++)"
echo "[INFO] KV260 platform : $KV260_PLATFORM"
