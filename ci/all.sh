#!/bin/bash
set -e

BASEDIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"

echo "Checking scripts"
"$BASEDIR/check_scripts.sh"

echo "Running format check"
"$BASEDIR/check_format.sh"

echo "Analizing code"
"$BASEDIR/static_analyzer.sh"

echo "Loading driver"
"$BASEDIR/load.sh"

echo "Running tests"
"$BASEDIR/test.sh"
