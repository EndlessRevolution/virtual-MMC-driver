#!/bin/bash
set -e

BASEDIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"
ROOTDIR=$(realpath "$BASEDIR/..")

echo "[1/2] User program (clang-tidy)"
clang-tidy "$ROOTDIR/user/user_program.c" -- 2>&1 | grep -E "warning:|error:" || true

echo "[2/2] Kernel module (sparse)"
make -C "$ROOTDIR/kernel" sparse 2>&1 | grep -E "warning:|error:" || true

echo "=== Done ==="
