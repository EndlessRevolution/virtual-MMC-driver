#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_DIR="$SCRIPT_DIR/../kernel"

if lsmod | grep -q "^vmmc"; then
    sudo rmmod vmmc
fi

cd "$KERNEL_DIR"
make clean 2>/dev/null || true
