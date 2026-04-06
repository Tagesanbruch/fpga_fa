#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/../.." && pwd)
XILINX_ROOT_DEFAULT=${XILINX_ROOT_DEFAULT:-/home/luci/tools/Xilinx}
XILINX_VERSION=${XILINX_VERSION:-2023.1}
source "$SCRIPT_DIR/source_xilinx_env.sh" all

find_kv260_platform() {
  local roots=(
    "${KV260_PLATFORM_DIR:-}"
    "$ROOT_DIR/fpga/platforms"
    "$HOME/xilinx/platforms"
    "$HOME/platforms"
    "$XILINX_ROOT_DEFAULT/Vitis/$XILINX_VERSION/base_platforms"
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
  echo "[ERR] Installed base platforms on this machine:" >&2
  find "$XILINX_ROOT_DEFAULT/Vitis/$XILINX_VERSION/base_platforms" -maxdepth 2 -type f -name '*.xpfm' 2>/dev/null | sed 's/^/[ERR]   /' >&2 || true
  return 1 2>/dev/null || exit 1
fi

echo "[INFO] Xilinx version : $XILINX_VERSION"
echo "[INFO] vitis_hls      : $(command -v vitis_hls)"
echo "[INFO] v++            : $(command -v v++)"
echo "[INFO] KV260 platform : $KV260_PLATFORM"
