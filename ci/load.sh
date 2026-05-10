#!/bin/bash
set -e

BASEDIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"
KERNELDIR="$BASEDIR/../kernel"

cd "$KERNELDIR"

make

if lsmod | grep -q "^vmmc"; then
    sudo rmmod vmmc
    sleep 1
fi

sudo insmod vmmc.ko

echo "Module loaded"
