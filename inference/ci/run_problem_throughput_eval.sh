#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<USAGE
Usage: $0 --image <image.png> [--output <metrics.json>]
USAGE
}

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PY_SCRIPT="$SCRIPT_DIR/../scripts/throughput_eval.py"
IMAGE=""
OUTPUT="throughput_metrics.json"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --image)
      IMAGE="$2"; shift 2 ;;
    --output)
      OUTPUT="$2"; shift 2 ;;
    -h|--help)
      usage; exit 0 ;;
    *)
      echo "[ERR] unknown arg: $1" >&2
      usage
      exit 1 ;;
  esac
done

if [[ -z "$IMAGE" ]]; then
  usage
  exit 1
fi

python3 "$PY_SCRIPT" -i "$IMAGE" -o "$OUTPUT"
