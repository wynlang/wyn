#!/bin/bash
# V-17: `wyn test` must print ONE truthful summary.
#
# It printed two, and they disagreed:
#
#     🧪 Wyn Test Runner            <- src/cmd_test.c
#     Scanning: tests/
#       ✓ one plus one              <- the COMPILED TEST BINARY
#       ✓ strings concat
#       ✓ truthy
#       ✓ len
#     4 tests passed                <- the binary's summary: TEST BLOCKS
#     🧪 Wyn Test Runner            <- the banner AGAIN
#     Scanning: tests/
#       ✓ tests/a_test.wyn (0.0s)   <- cmd_test's per-FILE line
#     Results: 1 passed, 0 failed   <- cmd_test's summary: FILES
#     🎉 All tests passed!
#
# TWO SEPARATE DEFECTS, and the second is the interesting one.
#
# 1. The counts measure different things and neither says which. "4 tests passed"
#    counts `test` blocks - what a user means by a test. "1 passed" counts FILES.
#    A reader has no way to tell, and on a 40-file suite the two numbers diverge
#    wildly.
#
# 2. The DUPLICATE BANNER is an unflushed stdout crossing fork(). cmd_test's
#    run_process() forks and the child calls
#        freopen("/dev/null", "w", stdout);
#    to silence the build. freopen flushes the stream first - and the child holds a
#    COPY of the parent's not-yet-flushed buffer, so the banner is written a second
#    time to the shared fd before the redirect takes effect.
#
#    IT ONLY HAPPENS WHEN STDOUT IS NOT A TTY. On a terminal, stdout is
#    line-buffered and the banner is already gone before the fork. Redirected - a
#    file, a pipe, every CI log, every agent capturing output - it is fully buffered
#    and duplicates. So this test MUST capture to a file, which it does; run it on a
#    terminal by hand and the defect is invisible.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

run_to() { perl -e 'alarm(shift); exec @ARGV' "$@"; }

# ---------------------------------------------------------------- one file, 4 blocks
P1="$TMP/p1"; mkdir -p "$P1/tests"
cat > "$P1/tests/a_test.wyn" <<'WYN'
test "one plus one" {
    assert_eq(1 + 1, 2)
}

test "strings concat" {
    assert_eq("a" + "b", "ab")
}

test "truthy" {
    assert(true)
}

test "len" {
    assert_eq("abc".len(), 3)
}
WYN
# REDIRECTED, not a pipe to another command and not a TTY - see the header.
(cd "$P1" && run_to 300 "$WYN" test > "$TMP/o1.txt" 2>&1); rc1=$?
strip() { sed 's/\x1b\[[0-9;]*m//g' "$1"; }

banners=$(strip "$TMP/o1.txt" | grep -c 'Wyn Test Runner' || true)
if [ "$banners" = "1" ]; then
    ok "the banner is printed exactly once when stdout is redirected"
else
    bad "the banner is printed $banners times (unflushed stdout across fork())"
    strip "$TMP/o1.txt" | sed 's/^/          /'
fi

# Exactly one SUMMARY. Both spellings count: the compiled binary's "N tests passed"
# and cmd_test's "Results: N passed", so the arm fails whichever one is duplicated.
sums=$(strip "$TMP/o1.txt" | grep -cE '^[[:space:]]*(Results:|[0-9]+ tests? (passed|failed))' || true)
if [ "$sums" = "1" ]; then
    ok "exactly one summary line"
else
    bad "$sums summary lines - a reader cannot tell which number to believe"
    strip "$TMP/o1.txt" | grep -nE '^[[:space:]]*(Results:|[0-9]+ tests? (passed|failed))' | sed 's/^/          /'
fi

# The count must be the number of TEST BLOCKS (4), not of FILES (1). This is the arm
# that says WHICH of the two old numbers was right.
if strip "$TMP/o1.txt" | grep -qE '(^|[^0-9])4 (tests? )?passed|passed:? 4\b|4 of 4'; then
    ok "the summary counts the 4 test blocks, not the 1 file"
else
    bad "the summary does not report 4 tests [$(strip "$TMP/o1.txt" | grep -iE 'result|passed' | tr '\n' ' ')]"
fi
[ "$rc1" -eq 0 ] && ok "an all-passing run exits 0" || bad "an all-passing run exited $rc1"

# The per-file line must appear once too - the same fork() flush duplicated it.
perfile=$(strip "$TMP/o1.txt" | grep -c 'a_test.wyn' || true)
if [ "$perfile" -le 1 ]; then
    ok "the per-file line is not duplicated"
else
    bad "the per-file line appears $perfile times"
fi

# ------------------------------------------------- two files, counts must aggregate
P2="$TMP/p2"; mkdir -p "$P2/tests"
cp "$P1/tests/a_test.wyn" "$P2/tests/a_test.wyn"
cat > "$P2/tests/b_test.wyn" <<'WYN'
test "b one" {
    assert_eq(2 * 2, 4)
}

test "b two" {
    assert_eq(10 - 1, 9)
}
WYN
(cd "$P2" && run_to 300 "$WYN" test > "$TMP/o2.txt" 2>&1); rc2=$?
if strip "$TMP/o2.txt" | grep -qE '(^|[^0-9])6 (tests? )?passed|passed:? 6\b|6 of 6'; then
    ok "two files aggregate to 6 test blocks, not 2 files"
else
    bad "counts do not aggregate across files [$(strip "$TMP/o2.txt" | grep -iE 'result|passed' | tr '\n' ' ')]"
fi
sums2=$(strip "$TMP/o2.txt" | grep -cE '^[[:space:]]*(Results:|[0-9]+ tests? (passed|failed))' || true)
if [ "$sums2" = "1" ]; then ok "still one summary with two files"
else bad "$sums2 summary lines with two files"; fi
[ "$rc2" -eq 0 ] && ok "two all-passing files exit 0" || bad "two all-passing files exited $rc2"

# --------------------------------------------------------- a failure must be visible
P3="$TMP/p3"; mkdir -p "$P3/tests"
cat > "$P3/tests/c_test.wyn" <<'WYN'
test "this one passes" {
    assert_eq(1, 1)
}

test "this one fails" {
    assert_eq(1, 2)
}
WYN
(cd "$P3" && run_to 300 "$WYN" test > "$TMP/o3.txt" 2>&1); rc3=$?
if [ "$rc3" -ne 0 ]; then ok "a failing test block makes wyn test exit nonzero"
else bad "a failing test block still exited 0"; fi
if strip "$TMP/o3.txt" | grep -qiE '1 failed|failed: 1|✗'; then
    ok "the summary reports the failure"
else
    bad "the summary hides the failure [$(strip "$TMP/o3.txt" | tail -3 | tr '\n' ' ')]"
fi
if strip "$TMP/o3.txt" | grep -qi "All tests passed"; then
    bad "it claimed 'All tests passed' with a failing test"
else
    ok "it does not claim 'All tests passed' with a failing test"
fi

# ------------- a program line that STARTS WITH A NUMBER is output, not a tally ------
# The first cut of the tally parser used
#     sscanf(plain, "%d tests passed", &p) == 1
# which is wrong in a way that looks right: sscanf's return counts ASSIGNED
# conversions and %d is assigned BEFORE the literal is compared, so ANY line starting
# with a number matched. Running `wyn test` over this repo's own tests/ reported
# "227705 tests passed". Two symptoms, and this arm checks both: the count is wrong,
# and the line is SWALLOWED (a summary line is deliberately not echoed), so a program's
# own output silently disappears from the report.
P6="$TMP/p6"; mkdir -p "$P6/tests"
cat > "$P6/tests/f_test.wyn" <<'WYN'
test "prints a number-led line" {
    print("42 apples on the wall")
    assert_eq(1, 1)
}
WYN
(cd "$P6" && run_to 300 "$WYN" test > "$TMP/o6.txt" 2>&1); rc6=$?
if ! strip "$TMP/o6.txt" | grep -q "42 apples on the wall"; then
    bad "a program line beginning with a number was swallowed as a summary"
elif strip "$TMP/o6.txt" | grep -qE 'Results:.*\b(42|43) tests? passed'; then
    bad "a program line beginning with a number was COUNTED as a tally [$(strip "$TMP/o6.txt" | grep Results:)]"
elif [ "$rc6" -eq 0 ] && strip "$TMP/o6.txt" | grep -qE 'Results:.*\b1 test passed'; then
    ok "a program line beginning with a number is echoed, not parsed as a tally"
else
    bad "number-led line arm: rc=$rc6 [$(strip "$TMP/o6.txt" | grep Results:)]"
fi

# ------------------------- the OTHER test style must keep working, and say its unit
# `wyn test` supports two styles and only one of them counts blocks:
#   (a) `test "name" { ... }`   -> the binary prints a tally (every arm above)
#   (b) `Test.init / Test.assert_* / Test.summary()` in a plain `fn main`
#                               -> NO tally line; pass/fail is only the exit code
# tests/stdlib/ and run_user_test_runner_test.sh's fixtures are all style (b), so a
# summary that assumed style (a) would break them. This arm pins the fallback: one
# summary, and it must say it is counting FILES rather than silently reporting 0 tests.
P5="$TMP/p5"; mkdir -p "$P5/tests"
cat > "$P5/tests/e_test.wyn" <<'WYN'
fn main() {
    Test.init("e")
    Test.assert_eq_int(2 + 3, 5, "add")
    Test.summary()
}
WYN
(cd "$P5" && run_to 300 "$WYN" test > "$TMP/o5.txt" 2>&1); rc5=$?
sums5=$(strip "$TMP/o5.txt" | grep -cE '^[[:space:]]*(Results:|[0-9]+ tests? (passed|failed))' || true)
if [ "$rc5" -eq 0 ] && [ "$sums5" = "1" ] && strip "$TMP/o5.txt" | grep -q 'test file'; then
    ok "a Test.summary()-style suite still passes, with one summary that says 'test file'"
else
    bad "the Test.summary() style broke (rc=$rc5, $sums5 summaries) [$(strip "$TMP/o5.txt" | tail -2 | tr '\n' ' ')]"
fi

# ------------------------------------------- a file with NO assertions still passes
# NOT FIXED, and asserted as a KNOWN GAP rather than left silent. A .wyn file named
# test_*.wyn with no assertions at all compiles, exits 0 and reads as green. It cannot
# be caught from the runner's side: on stdout and on the exit code it is
# INDISTINGUISHABLE from a passing style-(b) suite, and refusing it breaks all three
# fixtures in run_user_test_runner_test.sh (measured). The fix belongs in
# src/test_runtime.c, which knows how many assertions ran.
#
# This arm exists so the gap is VISIBLE and so the day it is fixed, this file fails
# and tells the next person to update it - the opposite of a silent waiver.
P4="$TMP/p4"; mkdir -p "$P4/tests"
cat > "$P4/tests/d_test.wyn" <<'WYN'
fn helper() -> int {
    return 1
}

fn main() -> int {
    return 0
}
WYN
(cd "$P4" && run_to 300 "$WYN" test > "$TMP/o4.txt" 2>&1); rc4=$?
if [ "$rc4" -eq 0 ]; then
    echo "  ~     KNOWN GAP: a test file with zero assertions still exits 0 (needs src/test_runtime.c)"
else
    bad "the zero-assertion gap now FAILS (rc=$rc4) - good news; delete this arm and the comment above it"
fi

echo ""
echo "test-summary: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
