#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
LLAMA_DIR="$ROOT_DIR/inference/xcomp/llama.cpp"
BUILD_DIR="${BUILD_DIR:-$LLAMA_DIR/build-kv260-fpga}"
GENERATOR="${GENERATOR:-Ninja}"

GGML_FPGA_MODE="${GGML_FPGA_MODE:-software}"
GGML_FPGA_XRT="${GGML_FPGA_XRT:-OFF}"
GGML_FPGA_XRT_ROOT="${GGML_FPGA_XRT_ROOT:-}"

cmake -S "$LLAMA_DIR" -B "$BUILD_DIR" -G "$GENERATOR" \
  -DGGML_BACKEND_DL=ON \
  -DGGML_FPGA=ON \
  -DGGML_FPGA_XRT="$GGML_FPGA_XRT" \
  -DGGML_FPGA_XRT_ROOT="$GGML_FPGA_XRT_ROOT" \
  -DGGML_NATIVE=OFF \
  -DGGML_CCACHE=OFF \
  -DGGML_OPENMP=OFF \
  -DLLAMA_BUILD_TESTS=OFF \
  -DLLAMA_BUILD_EXAMPLES=ON \
  -DLLAMA_BUILD_SERVER=ON

cmake --build "$BUILD_DIR" --target llama-cli llama-server ggml-fpga -j"$(nproc)"

cat <<EOF
[INFO] KV260-targeted llama.cpp build completed
[INFO] Build dir: $BUILD_DIR
[INFO] Backend mode hint: GGML_FPGA_MODE=$GGML_FPGA_MODE
[INFO] To use XRT on-board, rebuild with GGML_FPGA_XRT=ON and GGML_FPGA_XRT_ROOT set.
EOF
