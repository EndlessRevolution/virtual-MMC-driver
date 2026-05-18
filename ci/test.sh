#!/bin/bash

BASEDIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"
ROOTDIR=$(realpath "$BASEDIR/..")

cd "$ROOTDIR/user" || exit

make >/dev/null

printf "\n"

printf "%-30s   %-30s\n" \
    "Test name" "Result"

printf "%-33s%-30s\n" \
    "---------------------------------"\
    "------------------------------"

print_result() {
    local name=$1
    local message=$2
    local reason=$3

    printf "%-30s   %-30s\n" \
    "$name" "$message"

    if [ "$reason" != "" ]; then
        printf "\n"
        printf "%s\n" "$reason"
        printf "\n"
    fi

    printf "%-33s%-30s\n" \
        "---------------------------------"\
        "------------------------------"
    printf "\n"
}

test_persistent() {
    local name=$1
    local write_op=$2
    local read_op=$3
    local offset=$4
    local count=$5

    dd if=/dev/urandom of=input.bin bs=512 count="$count" status=none

    output=$(./user_program \
        --op "$write_op" \
        --offset "$offset" \
        --count "$count" \
        --input input.bin \
        2>&1)
    code=$?

    if [ $code -ne 0 ]; then
        print_result "$name" "FAILED" "$output"
        return
    fi

    output=$(./user_program \
        --op "$read_op" \
        --offset "$offset" \
        --count "$count" \
        --output output.bin \
        2>&1)
    code=$?

    if [ $code -ne 0 ]; then
        print_result "$name" "FAILED" "$output"
        return
    fi

    if cmp -s input.bin output.bin; then
        print_result "$name" "PASSED"
    else
        print_result "$name" "MISMATCH"  \
        "data read is different from data written"
    fi
}

test_card_error() {
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
            --output output.bin \
            2>&1)
    else
        output=$(./user_program \
            --op "$op" \
            --offset "$offset" \
            --count "$count" \
            --input input.bin \
            2>&1)
    fi

    if [ "$output" == "vmmc: $name" ]; then
        print_result "$name" "PASSED"
    else print_result "$name" "MISMATCH" \
        "\"$output\" instead of \"vmmc: $name\""
    fi
}

test_persistent "Persistent single 1"   24 17   0       1
test_persistent "Persistent single 2"   24 17   1048064 1
test_persistent "Persistent multiple 1" 25 18   0       2048
test_persistent "Persistent multiple 2" 25 18   512000  10

test_card_error "Address error"         24 2047 1       write
test_card_error "Illegal command"       30 512  10      read
test_card_error "Block length error"    24 0    3       write
test_card_error "Out of range"          18 1024 2047    read
