#!/bin/bash
# Helper script to run a single test
# Usage: ./run_single_test.sh <test_file>

test_file="$1"
out_file="${test_file%.wyn}.out"

# Compile
if ! ./wyn "$test_file" >/dev/null 2>&1; then
    exit 1
fi

# Run
if [ -f "$out_file" ]; then
    timeout 2 "$out_file" >/dev/null 2>&1
    exit $?
fi

exit 1
