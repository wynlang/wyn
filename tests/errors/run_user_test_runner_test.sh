#!/bin/bash
# `wyn test` for USER projects (T4.7): discovery (test_*.wyn, *_test.wyn, one
# subdir level), failing asserts exit nonzero (the wrapper's
# wyn_test_exit_code hook), name filter, and the runner's own exit code.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

mkdir -p "$TMP/proj/tests/unit"
printf 'fn main() {\n    Test.init("m")\n    Test.assert_eq_int(2+3, 5, "add")\n    Test.summary()\n}\n' > "$TMP/proj/tests/test_math.wyn"
printf 'fn main() {\n    Test.init("f")\n    Test.assert_eq_int(1, 2, "nope")\n    Test.summary()\n}\n' > "$TMP/proj/tests/test_fail.wyn"
printf 'fn main() {\n    Test.init("s")\n    Test.assert_eq_str("hi".upper(), "HI", "up")\n    Test.summary()\n}\n' > "$TMP/proj/tests/unit/strings_test.wyn"

cd "$TMP/proj"

# 1. full run: discovers all 3 (incl. subdir + _test suffix), 1 fails, exit 1
out=$(perl -e 'alarm 60; exec @ARGV' "$WYN_ABS" test 2>&1); code=$?
if [ $code -eq 1 ] && echo "$out" | grep -q "2 passed, 1 failed"; then
  ok "discovery + failing assert fails the run"
else bad "full run: code=$code [$(echo "$out" | tail -3)]"; fi

# 2. failing test binary itself exits nonzero (wrapper hook)
"$WYN_ABS" build tests/test_fail.wyn -o "$TMP/tf" >/dev/null 2>&1
"$TMP/tf" >/dev/null 2>&1; rc=$?
[ $rc -ne 0 ] && ok "failed asserts -> nonzero process exit" || bad "test binary exited 0 despite failed assert"

# 3. filter runs only matching files and passes
out=$(perl -e 'alarm 60; exec @ARGV' "$WYN_ABS" test math 2>&1); code=$?
if [ $code -eq 0 ] && echo "$out" | grep -q "1 passed, 0 failed"; then
  ok "name filter"
else bad "filter: code=$code [$(echo "$out" | tail -2)]"; fi

# 4. empty project: helpful message, exit 1
mkdir -p "$TMP/empty/tests"; cd "$TMP/empty"
out=$(perl -e 'alarm 30; exec @ARGV' "$WYN_ABS" test 2>&1); code=$?
if [ $code -eq 1 ] && echo "$out" | grep -q "test_\*.wyn"; then
  ok "empty project: helpful guidance"
else bad "empty: code=$code [$out]"; fi

echo ""; echo "user-test-runner: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
