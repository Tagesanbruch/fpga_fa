#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
COMPOSE_FILE="$ROOT_DIR/inference/xcomp/docker-compose.yml"
SERVICE_NAME="${SERVICE_NAME:-zynq-builder}"
IMAGE_NAME="${IMAGE_NAME:-zynq-builder:latest}"
BUILD_DIR_IN_CONTAINER="${BUILD_DIR_IN_CONTAINER:-/workspace/fpga-fa/inference/xcomp/llama.cpp/build-kv260-fpga-arm64}"
GGML_FPGA_XRT="${GGML_FPGA_XRT:-OFF}"
GGML_FPGA_XRT_ROOT="${GGML_FPGA_XRT_ROOT:-}"
JOBS="${JOBS:-96}"

if docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
  echo "[INFO] 发现本地镜像 $IMAGE_NAME，跳过 compose build，直接复用。"
  RUN_BUILD="0"
else
  echo "[INFO] 未发现本地镜像 $IMAGE_NAME，尝试通过 docker compose build 构建。"
  RUN_BUILD="1"
fi

if [[ "$RUN_BUILD" == "1" ]]; then
  docker compose -f "$COMPOSE_FILE" build
fi

docker compose -f "$COMPOSE_FILE" run --rm \
  -e BUILD_DIR="$BUILD_DIR_IN_CONTAINER" \
  -e GGML_FPGA_XRT="$GGML_FPGA_XRT" \
  -e GGML_FPGA_XRT_ROOT="$GGML_FPGA_XRT_ROOT" \
  -e JOBS="$JOBS" \
  "$SERVICE_NAME" \
  bash -lc "git config --global --add safe.directory /workspace/fpga-fa/inference/xcomp/llama.cpp && cd /workspace/fpga-fa && bash inference/ci/build_llama_server_fpga_kv260.sh"

cat <<EOF
[INFO] Docker arm64 构建完成
[INFO] 容器内构建目录: $BUILD_DIR_IN_CONTAINER
[INFO] 宿主机对应目录: ${BUILD_DIR_IN_CONTAINER/\/workspace\/fpga-fa/$ROOT_DIR}
EOF
