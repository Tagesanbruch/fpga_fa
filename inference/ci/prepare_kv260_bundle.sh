#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<USAGE
Usage: $0 --build-dir <dir> --bundle-dir <dir> [--xclbin <file>] [--model <file>] [--mmproj <file>]
USAGE
}

BUILD_DIR=""
BUNDLE_DIR=""
XCLBIN=""
MODEL_GGUF=""
MMPROJ_GGUF=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"; shift 2 ;;
    --bundle-dir)
      BUNDLE_DIR="$2"; shift 2 ;;
    --xclbin)
      XCLBIN="$2"; shift 2 ;;
    --model)
      MODEL_GGUF="$2"; shift 2 ;;
    --mmproj)
      MMPROJ_GGUF="$2"; shift 2 ;;
    -h|--help)
      usage; exit 0 ;;
    *)
      echo "[ERR] unknown arg: $1" >&2
      usage
      exit 1 ;;
  esac
done

if [[ -z "$BUILD_DIR" || -z "$BUNDLE_DIR" ]]; then
  usage
  exit 1
fi

SERVER_BIN="$BUILD_DIR/bin/llama-server"
CLI_BIN="$BUILD_DIR/bin/llama-cli"
FPGA_SO="$BUILD_DIR/bin/libggml-fpga.so"

if [[ ! -x "$SERVER_BIN" ]]; then
  echo "[ERR] missing llama-server: $SERVER_BIN" >&2
  exit 1
fi
if [[ ! -f "$FPGA_SO" ]]; then
  echo "[ERR] missing libggml-fpga.so: $FPGA_SO" >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$BUNDLE_DIR/bin" "$BUNDLE_DIR/lib" "$BUNDLE_DIR/scripts" "$BUNDLE_DIR/models" "$BUNDLE_DIR/bitstreams"

cp "$SERVER_BIN" "$BUNDLE_DIR/bin/"
if [[ -x "$CLI_BIN" ]]; then
  cp "$CLI_BIN" "$BUNDLE_DIR/bin/"
fi
cp -a "$BUILD_DIR"/bin/lib*.so* "$BUNDLE_DIR/lib/"
cp "$ROOT_DIR/inference/problem/sample.py" "$BUNDLE_DIR/scripts/"
cp "$ROOT_DIR/inference/problem/throughput_eval.py" "$BUNDLE_DIR/scripts/"
cp "$ROOT_DIR/inference/ci/run_llama_server_fpga_kv260.sh" "$BUNDLE_DIR/bin/"
cp "$ROOT_DIR/inference/ci/run_problem_throughput_eval.sh" "$BUNDLE_DIR/bin/"
cp "$ROOT_DIR/inference/ci/run_problem_sample.sh" "$BUNDLE_DIR/bin/"
chmod +x "$BUNDLE_DIR/bin/run_llama_server_fpga_kv260.sh" "$BUNDLE_DIR/bin/run_problem_throughput_eval.sh" "$BUNDLE_DIR/bin/run_problem_sample.sh"

if [[ -n "$XCLBIN" ]]; then
  cp "$XCLBIN" "$BUNDLE_DIR/bitstreams/"
fi
if [[ -n "$MODEL_GGUF" ]]; then
  cp "$MODEL_GGUF" "$BUNDLE_DIR/models/"
fi
if [[ -n "$MMPROJ_GGUF" ]]; then
  cp "$MMPROJ_GGUF" "$BUNDLE_DIR/models/"
fi

cat > "$BUNDLE_DIR/README.txt" <<TXT
KV260 upload bundle

bin/
  llama-server
  llama-cli
  run_llama_server_fpga_kv260.sh
  run_problem_throughput_eval.sh
  run_problem_sample.sh
lib/
  libggml-fpga.so
  libllama/libggml/libmtmd and related shared libraries
scripts/
  sample.py
  throughput_eval.py
models/
  model/mmproj gguf files if provided
bitstreams/
  xclbin if provided
TXT

echo "[INFO] Bundle prepared at: $BUNDLE_DIR"
