#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
cd "$ROOT_DIR/src"
make
./add_seq_runner --xclbin "$ROOT_DIR/bitstreams/add_seq_kernel.xclbin" --length 1024 --base-add 1 --dump
