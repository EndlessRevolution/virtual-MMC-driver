#!/bin/bash

BASEDIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"
ROOTDIR=$(realpath "$BASEDIR/..")

cd "$ROOTDIR/user" || exit

make >/dev/null

printf "\n"

passed=0
failed=0
failures=""

print_result() {
    local name=$1
    local message=$2
    local expected=$3
    local actual=$4
    local i=${#name}

    printf "TEST: %s " "$name"

    while [ "$i" -lt 25 ]; do
        printf "."
        i=$((i + 1))
    done

    printf " %s\n" "$message"

    if [ "$message" = "OK" ]; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))

        if [ "$expected" != "" ]; then
            failures="$failures
FAILED: $name
  expected \"$expected\"
  actual   \"$actual\"
"
        else
            failures="$failures
FAILED: $name
  error    \"$actual\"
"
        fi
    fi
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
        print_result "$name" "FAILED" "" "$output"
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
        print_result "$name" "FAILED" "" "$output"
        return
    fi

    if cmp -s input.bin output.bin; then
        print_result "$name" "OK"
    else
        print_result "$name" "FAILED" \
        "data read is equal to data written" \
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
        print_result "$name" "OK"
    elif [ "$output" == "open: Permission denied" ]; then
        print_result "$name" "FAILED" "" "$output"
    else print_result "$name" "FAILED" \
        "vmmc: $name" "$output"
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

if [ "$failed" -ne 0 ]; then
    printf "\n"
    printf "%s\n" "$failures"
fi

printf "summary: %s OK, %s FAILED\n" "$passed" "$failed"
