#!/bin/bash
set -e

BASEDIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"
KERNELDIR="$BASEDIR/../kernel"

if lsmod | grep -q "^vmmc"; then
    sudo rmmod vmmc
fi

cd "$KERNELDIR"
make clean 2>/dev/null || true

echo "Module unloaded"
