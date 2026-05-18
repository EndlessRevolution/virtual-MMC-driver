#!/bin/bash
set -e

BASEDIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"

echo "Checking scripts"
"$BASEDIR/check_scripts.sh"
echo

echo "Running format check"
"$BASEDIR/check_format.sh"
echo

echo "Analizing code"
"$BASEDIR/static_analyzer.sh"
echo

echo "Loading driver"
"$BASEDIR/load.sh"
echo

echo "Running tests"
"$BASEDIR/test.sh"
