#!/usr/bin/env bash
# assert_eq must COMPARE FLOATS, not truncate them.
#
# WHY THIS IS A SHELL TEST AND NOT A .wyn EXPECT FILE
#
# The thing under test is a FAILING assertion, so the test must assert that
# `wyn test` reports a failure and exits non-zero. An EXPECT file in
# tests/regression/ is run with `wyn run` and is required to SUCCEED, which cannot
# express "this assertion must fail".
#
# THE DEFECT
#
# `assert_eq` dispatched only two ways, string or int, so every float comparison
# was emitted as wyn_assert_eq_int(a, b) with both sides truncated to long long:
#
#   assert_eq(0.0, -0.1875)   -> wyn_assert_eq_int(0, 0)   -> PASSED
#
# Any two values inside one unit interval truncate alike, so the assertion was
# VACUOUS. A suite full of opacity and coverage assertions could be green while
# every value was wrong - which is worse than no test, because it is a green light.
# Found by a WynCanvas agent whose 17 float assertions were all silently passing.
#
# A near-miss also mis-REPORTED: assert_eq(1.5, 1.9) failed but printed
# "expected: 1, got: 1", which reads like a compiler bug rather than a value bug.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"

pass=0
fail=0
check() {
    local name="$1" want="$2" got="$3"
    if printf '%s' "$got" | grep -qF -- "$want"; then
        printf '  ok    %s\n' "$name"; pass=$((pass + 1))
    else
        printf '  FAIL  %s\n          wanted: %s\n' "$name" "$want"
        printf '%s\n' "$got" | sed 's/^/          | /'
        fail=$((fail + 1))
    fi
}

root=$(mktemp -d) || exit 1
trap 'rm -rf "$root"' EXIT

# Each case needs its OWN project dir. `wyn test` scans the whole tests/
# directory, so leaving earlier cases behind makes every later run report the
# accumulated totals and the "N passed, M failed" checks below stop meaning
# anything - which is exactly what happened on the first draft of this file.
n=0
fresh() {
    n=$((n + 1))
    d="$root/case$n"
    mkdir -p "$d/tests"
    printf '[project]\nname = "assertfloat"\nversion = "0.1.0"\n' > "$d/wyn.toml"
}

# ---------------------------------------------------------------------------
# 1. The vacuous case. Both sides truncate to 0, so this PASSED before the fix.
#    This single check is the regression: if it ever passes again, the float arm
#    is gone.
# ---------------------------------------------------------------------------
fresh
cat > "$d/tests/test_vacuous.wyn" <<'WYN'
test "sub-integer difference must FAIL" {
    assert_eq(0.0, 0.0 - 0.1875)
}
WYN
out=$( cd "$d" && "$WYN_ABS" test 2>&1 )
check "a sub-integer float mismatch fails"        "0 passed, 1 failed" "$out"
check "and reports the REAL values, not 0 and 0"  "-0.1875"            "$out"

# ---------------------------------------------------------------------------
# 2. Equal floats must still PASS - the fix must not make assert_eq useless in
#    the other direction. Exact equality is the contract (callers who want a
#    tolerance have one); 0.1+0.2 is deliberately NOT used here, since that is
#    genuinely != 0.3 in binary floating point and asserting otherwise would
#    encode a wrong expectation.
# ---------------------------------------------------------------------------
fresh
cat > "$d/tests/test_equal.wyn" <<'WYN'
test "equal floats pass" {
    assert_eq(0.1875, 0.1875)
    assert_eq(1.0, 1.0)
    assert_eq(0.0, 0.0)
    var x = 2.5
    var y = 5.0 / 2.0
    assert_eq(x, y)
}
WYN
out=$( cd "$d" && "$WYN_ABS" test 2>&1 )
check "equal floats still pass" "1 passed, 0 failed" "$out"

# ---------------------------------------------------------------------------
# 3. A mismatch whose INTEGER parts differ already failed before the fix, but it
#    reported the truncated values. Assert the real ones are printed, so a
#    near-miss is diagnosable rather than confusing.
# ---------------------------------------------------------------------------
fresh
cat > "$d/tests/test_report.wyn" <<'WYN'
test "a near miss reports precisely" {
    assert_eq(1.5, 1.9)
}
WYN
out=$( cd "$d" && "$WYN_ABS" test 2>&1 )
check "a near miss fails"                    "0 passed, 1 failed" "$out"
check "and prints the actual value 1.5"      "1.5"                "$out"
# 1.9 is not exactly representable in binary floating point, and %.17g prints
# what is actually stored. That is the RIGHT behaviour for an assertion message -
# rounding it to "1.9" would hide the very drift the caller is chasing - so the
# expectation here is the stored value, not the literal as typed.
check "and the expected value at full precision" "1.8999999999999999" "$out"

# ---------------------------------------------------------------------------
# 4. Mixed float/int arguments must take the FLOAT arm. If the int arm wins, the
#    float side is truncated and the comparison is wrong in the same silent way.
# ---------------------------------------------------------------------------
fresh
cat > "$d/tests/test_mixed.wyn" <<'WYN'
test "a float against an int literal must not truncate" {
    var x = 1.5
    assert_eq(x, 1)
}
WYN
out=$( cd "$d" && "$WYN_ABS" test 2>&1 )
check "1.5 is not equal to 1" "0 passed, 1 failed" "$out"

# ---------------------------------------------------------------------------
# 5. CONTROLS. The int and string arms must be untouched - a fix that routed
#    everything through the float comparison would break integer identity for
#    values above 2^53 and would compare string pointers as doubles.
# ---------------------------------------------------------------------------
fresh
cat > "$d/tests/test_controls.wyn" <<'WYN'
test "ints still compare as ints" {
    assert_eq(2 + 3, 5)
    assert_eq(9007199254740993, 9007199254740993)
}
test "strings still compare as strings" {
    assert_eq("ab" + "c", "abc")
}
WYN
out=$( cd "$d" && "$WYN_ABS" test 2>&1 )
# `wyn test` prints per-BLOCK totals as "N tests passed" and per-FILE totals as
# "N passed, M failed"; two blocks in one file is the former.
check "int and string arms unaffected" "2 tests passed" "$out"

fresh
cat > "$d/tests/test_int_fails.wyn" <<'WYN'
test "an int mismatch still fails" {
    assert_eq(1, 2)
}
WYN
out=$( cd "$d" && "$WYN_ABS" test 2>&1 )
check "an int mismatch still fails" "0 passed, 1 failed" "$out"

echo ""
echo "assert-eq-float: $pass pass, $fail fail"
[ "$fail" -eq 0 ]
