#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/inference/xcomp/llama.cpp/build-kv260-fpga}"
GGML_FPGA_XRT="${GGML_FPGA_XRT:-OFF}"
GGML_FPGA_XRT_ROOT="${GGML_FPGA_XRT_ROOT:-}"
HOST_ARCH="$(uname -m)"
JOBS="${JOBS:-96}"

export BUILD_DIR
export GGML_FPGA_XRT
export GGML_FPGA_XRT_ROOT
export JOBS

if [[ "$HOST_ARCH" != "aarch64" ]]; then
  echo "[WARN] 当前宿主架构为 $HOST_ARCH，本脚本会生成宿主机 native 二进制，仅用于软件烟雾验证。"
  echo "[WARN] 若要得到可直接上传到 KV260 的 aarch64 产物，请改用 inference/ci/build_llama_server_fpga_kv260_docker.sh"
fi

bash "$ROOT_DIR/inference/xcomp/build_kv260_llama_fpga.sh"
