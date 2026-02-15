#!/bin/bash
# Parallel Unit Test Runner for Wyn

echo "╔══════════════════════════════════════════════════════════════════════════╗"
echo "║                  WYN PARALLEL UNIT TEST RUNNER                           ║"
echo "╚══════════════════════════════════════════════════════════════════════════╝"
echo ""

NUM_JOBS=${NUM_JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 8)}
echo "Using $NUM_JOBS parallel workers"
echo ""

START=$(date +%s)
TMPDIR=$(mktemp -d)

# Create test list
find tests/unit -name "*.wyn" > $TMPDIR/tests.txt
TOTAL=$(wc -l < $TMPDIR/tests.txt | tr -d ' ')

echo "Found $TOTAL tests, running in parallel..."
echo ""

# Run tests in parallel
cat $TMPDIR/tests.txt | xargs -P $NUM_JOBS -n 1 sh -c '
    test_file="$1"
    out_file="${test_file%.wyn}.out"
    if ./wyn "$test_file" >/dev/null 2>&1 && [ -f "$out_file" ]; then
        echo "PASS"
    else
        echo "FAIL"
    fi
' _ > $TMPDIR/results.txt

END=$(date +%s)
DURATION=$((END - START))

# Count results
PASSED=$(grep -c "PASS" $TMPDIR/results.txt 2>/dev/null || echo 0)
FAILED=$(grep -c "FAIL" $TMPDIR/results.txt 2>/dev/null || echo 0)

rm -rf "$TMPDIR"

echo "╔══════════════════════════════════════════════════════════════════════════╗"
echo "║                           TEST RESULTS                                   ║"
echo "╚══════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "Total:    $TOTAL"
echo "Passed:   $PASSED ✅"
echo "Failed:   $FAILED ❌"
echo "Duration: ${DURATION}s"
[ $DURATION -gt 0 ] && echo "Speed:    $((TOTAL / DURATION)) tests/sec"
echo ""
echo "Speedup vs sequential (~570s): ~$((570 / DURATION))x faster"
echo ""

[ $PASSED -eq $TOTAL ] && echo "✅ ALL TESTS PASSED" || echo "❌ $FAILED TESTS FAILED"
[ $PASSED -eq $TOTAL ] && exit 0 || exit 1
