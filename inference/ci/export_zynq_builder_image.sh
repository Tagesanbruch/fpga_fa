#!/usr/bin/env bash
set -euo pipefail

IMAGE_NAME="${IMAGE_NAME:-zynq-builder:latest}"
OUTPUT_TAR="${OUTPUT_TAR:-/tmp/zynq-builder_latest.tar}"

if ! docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
  echo "[ERR] 本地不存在镜像: $IMAGE_NAME" >&2
  exit 1
fi

ARCH="$(docker image inspect "$IMAGE_NAME" --format '{{.Architecture}}')"
OS_NAME="$(docker image inspect "$IMAGE_NAME" --format '{{.Os}}')"

echo "[INFO] 导出镜像: $IMAGE_NAME"
echo "[INFO] 镜像平台: ${OS_NAME}/${ARCH}"
echo "[INFO] 输出文件: $OUTPUT_TAR"

docker save -o "$OUTPUT_TAR" "$IMAGE_NAME"
ls -lh "$OUTPUT_TAR"
