#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_DIR="$SCRIPT_DIR/../kernel"

cd "$KERNEL_DIR"

make

if lsmod | grep -q "^vmmc"; then
    sudo rmmod vmmc
    sleep 1
fi

sudo insmod vmmc.ko
