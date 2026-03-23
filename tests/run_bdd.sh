#!/bin/bash
# BDD + Regression test runner
# Runs .wyn files, compares output against // EXPECT: comments
set -euo pipefail

WYN="${WYN:-./wyn}"
PASS=0
FAIL=0
ERRORS=""

run_test() {
    local file="$1"
    local name=$(basename "$file")

    # Extract expected output lines
    local expected=$(grep '// EXPECT:' "$file" | sed 's|// EXPECT: ||')
    if [ -z "$expected" ]; then
        return  # No expectations, skip
    fi

    # Try to build and run, strip compiler status lines
    local output
    output=$($WYN run "$file" 2>&1 | grep -v "^Compiled in\|^\x1b\[2mCompiled") || true

    # Compare line by line
    local exp_lines=$(echo "$expected" | wc -l | tr -d ' ')
    local i=1
    local failed=0
    while IFS= read -r exp_line; do
        local actual_line=$(echo "$output" | sed -n "${i}p")
        if [ "$actual_line" != "$exp_line" ]; then
            failed=1
            ERRORS="${ERRORS}\n  FAIL: $name line $i\n    expected: $exp_line\n    actual:   $actual_line"
        fi
        i=$((i + 1))
    done <<< "$expected"

    if [ "$failed" -eq 0 ]; then
        PASS=$((PASS + 1))
        echo "  ✓ $name"
    else
        FAIL=$((FAIL + 1))
        echo "  ✗ $name"
    fi
}

echo "=== Expect Tests ==="
for f in tests/expect/*.wyn; do
    [ -f "$f" ] && run_test "$f"
done

echo ""
echo "=== Regression Tests ==="
for f in tests/regression/*.wyn; do
    [ -f "$f" ] && run_test "$f"
done

echo ""
echo "Results: $PASS pass, $FAIL fail"
if [ -n "$ERRORS" ]; then
    echo -e "\nFailures:$ERRORS"
fi
exit $FAIL
