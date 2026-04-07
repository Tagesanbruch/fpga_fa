#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<USAGE
Usage: $0 --model <model.gguf> [--mmproj <mmproj.gguf>] [--xclbin <kernel.xclbin>] [--port <port>] [--host <host>] [--device <name>] [--queue-depth <n>] [extra llama-server args...]
USAGE
}

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LIB_DIR="${SCRIPT_DIR}/../lib"
MODEL=""
MMPROJ=""
XCLBIN=""
PORT="8080"
HOST="0.0.0.0"
DEVICE_NAME="FPGA0"
QUEUE_DEPTH="8"
EXTRA_ARGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --model)
      MODEL="$2"; shift 2 ;;
    --mmproj)
      MMPROJ="$2"; shift 2 ;;
    --xclbin)
      XCLBIN="$2"; shift 2 ;;
    --port)
      PORT="$2"; shift 2 ;;
    --host)
      HOST="$2"; shift 2 ;;
    --device)
      DEVICE_NAME="$2"; shift 2 ;;
    --queue-depth)
      QUEUE_DEPTH="$2"; shift 2 ;;
    -h|--help)
      usage; exit 0 ;;
    --)
      shift
      EXTRA_ARGS+=("$@")
      break ;;
    *)
      EXTRA_ARGS+=("$1")
      shift ;;
  esac
done

if [[ -z "$MODEL" ]]; then
  echo "[ERR] --model is required" >&2
  usage
  exit 1
fi

export LD_LIBRARY_PATH="${LIB_DIR}:${SCRIPT_DIR}:${LD_LIBRARY_PATH:-}"
export GGML_BACKEND_PATH="${GGML_BACKEND_PATH:-$LIB_DIR/libggml-fpga.so}"
export GGML_FPGA_MODE="${GGML_FPGA_MODE:-xrt}"
export GGML_FPGA_QUEUE_DEPTH="${GGML_FPGA_QUEUE_DEPTH:-$QUEUE_DEPTH}"
if [[ -n "$XCLBIN" ]]; then
  export GGML_FPGA_XCLBIN="$XCLBIN"
fi

CMD=("$SCRIPT_DIR/llama-server" --model "$MODEL" --host "$HOST" --port "$PORT" --device "$DEVICE_NAME" --flash-attn on)
if [[ -n "$MMPROJ" ]]; then
  CMD+=(--mmproj "$MMPROJ")
fi
CMD+=("${EXTRA_ARGS[@]}")

echo "[INFO] GGML_BACKEND_PATH=$GGML_BACKEND_PATH"
echo "[INFO] GGML_FPGA_MODE=$GGML_FPGA_MODE"
[[ -n "${GGML_FPGA_XCLBIN:-}" ]] && echo "[INFO] GGML_FPGA_XCLBIN=$GGML_FPGA_XCLBIN"
echo "[INFO] starting: ${CMD[*]}"
exec "${CMD[@]}"
