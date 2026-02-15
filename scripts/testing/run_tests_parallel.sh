#!/bin/bash
# Helper for Wyn native runner
# Usage: ./run_tests_parallel.sh <num_workers>

WORKERS=${1:-50}
TMPFILE=$(mktemp)

find tests/unit -name '*.wyn' | xargs -P $WORKERS -n 1 sh -c '
    test="$1"
    out="${test%.wyn}.out"
    if ./wyn "$test" >/dev/null 2>&1 && [ -f "$out" ] && timeout 2 "$out" >/dev/null 2>&1; then
        echo "PASS"
    else
        echo "FAIL"
    fi
' _ > "$TMPFILE"

PASSED=$(grep -c "PASS" "$TMPFILE" 2>/dev/null || echo 0)
FAILED=$(grep -c "FAIL" "$TMPFILE" 2>/dev/null || echo 0)
TOTAL=$((PASSED + FAILED))

echo "Total:  $TOTAL"
echo "Passed: $PASSED"
echo "Failed: $FAILED"

rm -f "$TMPFILE"

[ $FAILED -eq 0 ]
