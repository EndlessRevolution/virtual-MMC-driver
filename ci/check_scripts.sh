#!/bin/bash
set -e

BASEDIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"

find "$BASEDIR" -name "*.sh" -exec bash -n {} +
find "$BASEDIR" -name "*.sh" -exec shellcheck {} +

echo "=== Done ==="
