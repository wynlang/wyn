#!/bin/bash
# Parallel unit test runner - runs all 110 tests concurrently
set -uo pipefail

WYN="${WYN:-./wyn}"
TMPDIR=$(mktemp -d)
START=$(date +%s)

# Per-test watchdog (stock macOS has no `timeout`): wall-clock alarm + CPU
# rlimit so a looping/leaking test can't exhaust the host across N shards.
WYN_TEST_TIMEOUT="${WYN_TEST_TIMEOUT:-60}"
with_limits() {
    ( ulimit -t $((WYN_TEST_TIMEOUT * 2)) 2>/dev/null
      exec perl -e 'alarm shift; exec @ARGV or exit 127' "$WYN_TEST_TIMEOUT" "$@" )
}

run_test() {
    local file="$1"
    local name=$(basename "$file")
    local result_file="$TMPDIR/$name"
    local output=$(with_limits $WYN run "$file" 2>&1)
    local exit_code=$?
    if [ $exit_code -eq 0 ] && ! echo "$output" | grep -q "Segmentation fault"; then
        echo "PASS" > "$result_file"
    else
        echo "FAIL" > "$result_file"
    fi
}

# Read test list from run_tests.wyn
TESTS=$(cat tests/test_list.txt 2>/dev/null || grep '"tests/' tests/run_tests.wyn | sed 's/.*"\(tests\/[^"]*\)".*/\1/')

# A CHECK THAT CANNOT FAIL READS AS COVERAGE.
#
# tests/test_list.txt is not in the tree and tests/run_tests.wyn no longer carries the
# quoted paths this fell back to, so this script has been selecting ZERO tests and
# reporting "0 pass, 0 fail" with exit 0 - while CI runs it and CLAUDE.md described it
# as a gate. That is strictly worse than not running it: a green tick that proves
# nothing. Refuse instead, and say what would fix it.
#
# `make test` is the source of truth. This script is a manual accelerator; if you want
# it back as a gate, regenerate tests/test_list.txt and remove this guard.
# Count only paths that EXIST. The fallback grep matches this script's own mention of
# tests/test_list.txt, so a naive non-empty check passes on one bogus path that the run
# loop then skips - which is exactly how "0 pass, 0 fail, exit 0" was reached.
_test_count=0
for _t in $TESTS; do [ -f "$_t" ] && _test_count=$((_test_count + 1)); done
if [ "$_test_count" -eq 0 ]; then
    echo "run_tests_parallel.sh: selected ZERO existing tests - refusing to report success." >&2
    echo "  tests/test_list.txt is absent and the run_tests.wyn fallback matched no real files." >&2
    echo "  This script gates nothing in that state. Use 'make test' (the real gate)," >&2
    echo "  or regenerate tests/test_list.txt to use this runner." >&2
    exit 2
fi

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
