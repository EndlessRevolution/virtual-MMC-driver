#!/bin/bash
set -e

BASEDIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"
ROOTDIR=$(realpath "$BASEDIR/..")

echo "[1/2] User program (clang-format)"
clang-format --dry-run --Werror "$ROOTDIR/user/user_program.c"

echo "[2/2] Kernel module (clang-format)"
clang-format --dry-run --Werror "$ROOTDIR/kernel/vmmc.c"

echo "=== Done ==="
