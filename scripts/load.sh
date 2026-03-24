#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_DIR="$SCRIPT_DIR/../kernel"
DEVICE_NAME="virtual_mmc_driver"
DEVICE_PATH="/dev/$DEVICE_NAME"

cd "$KERNEL_DIR"

make

if lsmod | grep -q vmmc; then
    sudo rmmod vmmc
    sleep 1
fi

sudo insmod vmmc.ko

MAJOR=$(awk "\$2==\"$DEVICE_NAME\" {print \$1}" /proc/devices)

if [ ! -e "$DEVICE_PATH" ]; then
    sudo mknod "$DEVICE_PATH" c "$MAJOR" 0
fi

sudo chmod 666 "$DEVICE_PATH"
