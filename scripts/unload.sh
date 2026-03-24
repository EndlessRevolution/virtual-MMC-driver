#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_DIR="$SCRIPT_DIR/../kernel"
DEVICE_NAME="virtual_mmc_driver"
DEVICE_PATH="/dev/$DEVICE_NAME"

if [ -e "$DEVICE_PATH" ]; then
    sudo rm -f "$DEVICE_PATH"
fi

if lsmod | grep -q vmmc; then
    sudo rmmod vmmc
fi

cd "$KERNEL_DIR"
make clean 2>/dev/null || true
