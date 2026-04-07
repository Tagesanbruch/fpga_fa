#!/usr/bin/env bash
set -euo pipefail

THIS_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "$THIS_DIR/../../.." && pwd)

COMMON_SDK_DIR=${COMMON_SDK_DIR:-"$ROOT_DIR/third_party/xilinx-zynqmp-common-v2023.1_05080224/sdk"}
XSA_PATH=${XSA_PATH:-"$ROOT_DIR/fpga/platforms/kv260/hw/minimal/project/kv260_min_shell.xsa"}
WORK_DIR=${WORK_DIR:-"$ROOT_DIR/fpga/platforms/kv260/vitis/workspaces/kv260_min_shell"}

mkdir -p "$WORK_DIR"

cat > "$WORK_DIR/platform_min_shell.tcl" <<EOF
setws $WORK_DIR
platform create -name kv260_min_shell -hw $XSA_PATH
domain create -name smp_linux -os linux -proc psu_cortexa53
domain config -sysroot $COMMON_SDK_DIR/sysroots/cortexa72-cortexa53-xilinx-linux
domain config -image $ROOT_DIR/third_party/xilinx-zynqmp-common-v2023.1_05080224/Image
domain config -rootfs $ROOT_DIR/third_party/xilinx-zynqmp-common-v2023.1_05080224/rootfs.ext4
platform config -remove-boot-bsp
platform generate
EOF

source "$ROOT_DIR/fpga/remote/source_xilinx_env.sh" all
xsct -nodisp "$WORK_DIR/platform_min_shell.tcl"
