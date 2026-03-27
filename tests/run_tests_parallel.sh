#!/bin/bash
# Parallel unit test runner — runs all 110 tests concurrently
set -uo pipefail

WYN="${WYN:-./wyn}"
TMPDIR=$(mktemp -d)
START=$(date +%s)

run_test() {
    local file="$1"
    local name=$(basename "$file")
    local result_file="$TMPDIR/$name"
    local output=$($WYN run "$file" 2>&1)
    local exit_code=$?
    if [ $exit_code -eq 0 ] && ! echo "$output" | grep -q "Segmentation fault"; then
        echo "PASS" > "$result_file"
    else
        echo "FAIL" > "$result_file"
    fi
}

# Read test list from run_tests.wyn
TESTS=$(cat tests/test_list.txt 2>/dev/null || grep '"tests/' tests/run_tests.wyn | sed 's/.*"\(tests\/[^"]*\)".*/\1/')

# Launch all in parallel (limit concurrency to avoid overwhelming the system)
JOBS=0
MAX_JOBS=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 8)
for f in $TESTS; do
    [ -f "$f" ] || continue
    run_test "$f" &
    JOBS=$((JOBS + 1))
    if [ $JOBS -ge $MAX_JOBS ]; then
        wait -n 2>/dev/null || wait
        JOBS=$((JOBS - 1))
    fi
done
wait

# Collect results
PASS=0
FAIL=0
FAIL_LIST=""
for f in $TESTS; do
    [ -f "$f" ] || continue
    name=$(basename "$f")
    rf="$TMPDIR/$name"
    if [ -f "$rf" ] && grep -q "PASS" "$rf"; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        FAIL_LIST="$FAIL_LIST  FAIL: $f\n"
    fi
done

END=$(date +%s)
ELAPSED=$((END - START))

echo "=== Wyn Test Runner (parallel) ==="
if [ -n "$FAIL_LIST" ]; then
    echo -e "$FAIL_LIST"
fi
echo ""
echo "Results: $PASS pass, $FAIL fail (${ELAPSED}s)"

rm -rf "$TMPDIR"
exit $FAIL
