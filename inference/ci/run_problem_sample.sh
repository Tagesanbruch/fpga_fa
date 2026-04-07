#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<USAGE
Usage: $0 --input <FullTest.json> --output <sample_100.json>
USAGE
}

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PY_SCRIPT="$SCRIPT_DIR/../scripts/sample.py"
INPUT=""
OUTPUT=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --input)
      INPUT="$2"; shift 2 ;;
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

if [[ -z "$INPUT" || -z "$OUTPUT" ]]; then
  usage
  exit 1
fi

python3 "$PY_SCRIPT" -i "$INPUT" -o "$OUTPUT"
