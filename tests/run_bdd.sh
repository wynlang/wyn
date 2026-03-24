#!/bin/bash
# Parallel BDD + Regression test runner
# Runs all .wyn tests concurrently using background jobs
set -uo pipefail

WYN="${WYN:-./wyn}"
TMPDIR=$(mktemp -d)
PASS=0
FAIL=0
TOTAL=0
ERRORS=""

run_test() {
    local file="$1"
    local name=$(basename "$file")
    local result_file="$TMPDIR/$name.result"

    local expected=$(grep '// EXPECT:' "$file" | sed 's|// EXPECT: ||')
    if [ -z "$expected" ]; then
        echo "SKIP" > "$result_file"
        return
    fi

    local output=$($WYN build "$file" 2>/dev/null && "${file%.wyn}" 2>&1; rm -f "${file%.wyn}" "${file}.c" 2>/dev/null) || true
    output=$(echo "$output" | grep -v "Building\|Built\|Compiled in\|Warning:")

    local failed=0
    local errs=""
    local i=1
    while IFS= read -r exp_line; do
        local actual_line=$(echo "$output" | sed -n "${i}p")
        if [ "$actual_line" != "$exp_line" ]; then
            failed=1
            errs="${errs}    expected: $exp_line\n    actual:   $actual_line\n"
        fi
        i=$((i + 1))
    done <<< "$expected"

    if [ "$failed" -eq 0 ]; then
        echo "PASS" > "$result_file"
    else
        printf "FAIL\n%b" "$errs" > "$result_file"
    fi
}

# Collect all test files
FILES=()
for f in tests/expect/*.wyn tests/regression/*.wyn; do
    [ -f "$f" ] && FILES+=("$f")
done

# Launch all tests in parallel
for f in "${FILES[@]}"; do
    run_test "$f" &
done
wait

# Collect results
echo "=== Expect Tests ==="
for f in tests/expect/*.wyn; do
    [ -f "$f" ] || continue
    name=$(basename "$f")
    rf="$TMPDIR/$name.result"
    [ -f "$rf" ] || continue
    status=$(head -1 "$rf")
    if [ "$status" = "SKIP" ]; then continue; fi
    TOTAL=$((TOTAL + 1))
    if [ "$status" = "PASS" ]; then
        PASS=$((PASS + 1))
        echo "  ✓ $name"
    else
        FAIL=$((FAIL + 1))
        echo "  ✗ $name"
        ERRORS="${ERRORS}\n  FAIL: $name\n$(tail -n +2 "$rf")"
    fi
done

echo ""
echo "=== Regression Tests ==="
for f in tests/regression/*.wyn; do
    [ -f "$f" ] || continue
    name=$(basename "$f")
    rf="$TMPDIR/$name.result"
    [ -f "$rf" ] || continue
    status=$(head -1 "$rf")
    if [ "$status" = "SKIP" ]; then continue; fi
    TOTAL=$((TOTAL + 1))
    if [ "$status" = "PASS" ]; then
        PASS=$((PASS + 1))
        echo "  ✓ $name"
    else
        FAIL=$((FAIL + 1))
        echo "  ✗ $name"
        ERRORS="${ERRORS}\n  FAIL: $name\n$(tail -n +2 "$rf")"
    fi
done

echo ""
echo "Results: $PASS pass, $FAIL fail"
if [ -n "$ERRORS" ]; then
    echo -e "\nFailures:$ERRORS"
fi
rm -rf "$TMPDIR"
exit $FAIL
