#!/bin/bash
# Quick demo of parallel test runner speedup

cd "$(dirname "$0")"

echo "╔══════════════════════════════════════════════════════════════════════════╗"
echo "║                                                                          ║"
echo "║                  PARALLEL TEST RUNNER DEMO                               ║"
echo "║                                                                          ║"
echo "╚══════════════════════════════════════════════════════════════════════════╝"
echo ""

# Find a subset of tests
TEST_FILES=$(find . -maxdepth 1 -name "*.wyn" -type f | grep -v "parallel_test_runner" | head -20)
NUM_TESTS=$(echo "$TEST_FILES" | wc -l | tr -d ' ')

echo "Demo with $NUM_TESTS tests"
echo ""

# Sequential timing
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SEQUENTIAL (one at a time)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
START=$(date +%s)

PASSED=0
for test in $TEST_FILES; do
    if timeout 5 ../wyn run "$test" >/dev/null 2>&1; then
        PASSED=$((PASSED + 1))
        echo "✅ $(basename $test)"
    else
        echo "❌ $(basename $test)"
    fi
done

END=$(date +%s)
SEQ_TIME=$((END - START))

echo ""
echo "Sequential: ${SEQ_TIME}s ($PASSED/$NUM_TESTS passed)"
echo ""

# Parallel timing
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "PARALLEL (8 at once)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
START=$(date +%s)

run_test() {
    if timeout 5 ../wyn run "$1" >/dev/null 2>&1; then
        echo "✅ $(basename $1)"
    else
        echo "❌ $(basename $1)"
    fi
}
export -f run_test

echo "$TEST_FILES" | xargs -P 8 -I {} bash -c 'run_test "$@"' _ {}

END=$(date +%s)
PAR_TIME=$((END - START))

echo ""
echo "Parallel: ${PAR_TIME}s"
echo ""

# Calculate speedup
if [ $PAR_TIME -gt 0 ]; then
    SPEEDUP=$((SEQ_TIME * 100 / PAR_TIME))
    SPEEDUP_DECIMAL=$((SPEEDUP / 100))
    SPEEDUP_FRAC=$((SPEEDUP % 100))
    echo "╔══════════════════════════════════════════════════════════════════════════╗"
    echo "║                          RESULTS                                         ║"
    echo "╚══════════════════════════════════════════════════════════════════════════╝"
    echo ""
    echo "Sequential: ${SEQ_TIME}s"
    echo "Parallel:   ${PAR_TIME}s"
    echo "Speedup:    ${SPEEDUP_DECIMAL}.${SPEEDUP_FRAC}x faster"
    echo ""
    echo "With 579 tests @ 500s sequential:"
    echo "  → Parallel would take ~$((500 * PAR_TIME / SEQ_TIME))s"
    echo "  → Savings: ~$((500 - 500 * PAR_TIME / SEQ_TIME))s (~$((500 - 500 * PAR_TIME / SEQ_TIME) / 60) minutes)"
fi
