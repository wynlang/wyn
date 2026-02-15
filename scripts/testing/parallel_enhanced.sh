#!/bin/bash
# Enhanced Parallel Test Runner
# Uses more workers and better reporting

echo "╔══════════════════════════════════════════════════════════════════════════╗"
echo "║              WYN ENHANCED PARALLEL TEST RUNNER                           ║"
echo "╚══════════════════════════════════════════════════════════════════════════╝"
echo ""

# Use more workers - up to 50
NUM_JOBS=${NUM_JOBS:-50}
echo "Using $NUM_JOBS parallel workers (enhanced from 12)"
echo ""

START=$(date +%s)
TMPDIR=$(mktemp -d)

# Create test list
find tests/unit -name "*.wyn" > $TMPDIR/tests.txt
TOTAL=$(wc -l < $TMPDIR/tests.txt | tr -d ' ')

echo "Found $TOTAL tests, running in parallel..."
echo ""

# Run tests in parallel with more workers
cat $TMPDIR/tests.txt | xargs -P $NUM_JOBS -n 1 sh -c '
    test_file="$1"
    out_file="${test_file%.wyn}.out"
    if ./wyn "$test_file" >/dev/null 2>&1 && [ -f "$out_file" ] && timeout 2 "$out_file" >/dev/null 2>&1; then
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
echo "Speed:    $((TOTAL / (DURATION > 0 ? DURATION : 1))) tests/sec"
echo ""
echo "Speedup vs sequential (~570s): ~$((570 / (DURATION > 0 ? DURATION : 1)))x faster"
echo "Workers: $NUM_JOBS (enhanced from 12)"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "✅ ALL TESTS PASSED"
    exit 0
else
    echo "❌ $FAILED TESTS FAILED"
    exit 1
fi
