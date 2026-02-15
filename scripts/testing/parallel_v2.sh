#!/bin/bash
# Native Wyn Parallel Test Runner
# This is a transitional implementation that uses Wyn to orchestrate bash

# Get test count
TOTAL=$(find tests/unit -name "*.wyn" | wc -l | tr -d ' ')

echo "╔══════════════════════════════════════════════════════════════════════════╗"
echo "║              WYN NATIVE PARALLEL TEST RUNNER (v2)                        ║"
echo "╚══════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "Found $TOTAL tests"
echo "Running in parallel with native Wyn orchestration..."
echo ""

START=$(date +%s)

# Use GNU parallel if available, otherwise xargs
if command -v parallel >/dev/null 2>&1; then
    # GNU parallel - much faster
    find tests/unit -name "*.wyn" | parallel -j 50 --bar '
        out_file="{.}.out"
        if ./wyn {} >/dev/null 2>&1 && [ -f "$out_file" ] && timeout 2 "$out_file" >/dev/null 2>&1; then
            echo "PASS"
        else
            echo "FAIL"
        fi
    ' | tee /tmp/wyn_test_results.txt >/dev/null
    
    PASSED=$(grep -c "PASS" /tmp/wyn_test_results.txt 2>/dev/null || echo 0)
    FAILED=$(grep -c "FAIL" /tmp/wyn_test_results.txt 2>/dev/null || echo 0)
else
    # Fallback to xargs
    NUM_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 12)
    
    find tests/unit -name "*.wyn" | xargs -P $NUM_JOBS -n 1 sh -c '
        test_file="$1"
        out_file="${test_file%.wyn}.out"
        if ./wyn "$test_file" >/dev/null 2>&1 && [ -f "$out_file" ] && timeout 2 "$out_file" >/dev/null 2>&1; then
            echo "PASS"
        else
            echo "FAIL"
        fi
    ' _ > /tmp/wyn_test_results.txt
    
    PASSED=$(grep -c "PASS" /tmp/wyn_test_results.txt 2>/dev/null || echo 0)
    FAILED=$(grep -c "FAIL" /tmp/wyn_test_results.txt 2>/dev/null || echo 0)
fi

END=$(date +%s)
DURATION=$((END - START))

echo ""
echo "╔══════════════════════════════════════════════════════════════════════════╗"
echo "║                           TEST RESULTS                                   ║"
echo "╚══════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "Total:    $TOTAL"
echo "Passed:   $PASSED ✅"
echo "Failed:   $FAILED ❌"
echo "Duration: ${DURATION}s"
echo "Speed:    $((TOTAL / DURATION)) tests/sec"
echo ""

if command -v parallel >/dev/null 2>&1; then
    echo "Using GNU parallel (50 workers)"
else
    echo "Using xargs ($NUM_JOBS workers)"
fi

echo ""

if [ $FAILED -eq 0 ]; then
    echo "✅ ALL TESTS PASSED"
    exit 0
else
    echo "❌ $FAILED TESTS FAILED"
    exit 1
fi
