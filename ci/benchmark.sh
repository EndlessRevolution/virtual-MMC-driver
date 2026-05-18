#!/bin/bash
set -e

BASEDIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"
ROOTDIR=$(realpath "$BASEDIR/..")

cd "$ROOTDIR/user"

make >/dev/null

dd if=/dev/zero of=input.bin bs=512 count=2048 status=none

printf "\n"

printf "%-18s %-8s %-10s %-18s %-18s\n" \
    "Operation" "Blocks" "Offset" "Avg time (ns)" "Blocks/ns"

printf "%-18s %-8s %-10s %-18s %-18s\n" \
    "------------------" \
    "--------" \
    "----------" \
    "------------------" \
    "------------------"

run_test() {
    local name=$1
    local op=$2
    local offset=$3
    local count=$4
    local mode=$5

    if [ "$mode" = "read" ]; then
        output=$(./user_program \
            --op "$op" \
            --offset "$offset" \
            --count "$count" \
            --output output.bin)
    else
        output=$(./user_program \
            --op "$op" \
            --offset "$offset" \
            --count "$count" \
            --input input.bin)
    fi

    avg_time=$(echo "$output" | awk '/Average time:/ {print $3}')
    blocks_ns=$(echo "$output" | awk '/Average blocks per ns:/ {print $5}')

    printf "%-18s %-8s %-10s %-18s %-18s\n" \
        "$name" \
        "$count" \
        "$offset" \
        "$avg_time" \
        "$blocks_ns"
}

run_test "single read"    17 0 1  read
run_test "multiple read"  18 0 25 read
run_test "single write"   24 0 1  write
run_test "multiple write" 25 0 25 write

printf "\n"
