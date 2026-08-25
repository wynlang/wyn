#!/usr/bin/env bash
# A `%` in a `test "..."` NAME must print literally.
#
# WHY THIS IS A SHELL TEST AND NOT AN EXPECT FILE
#
# What is under test is the text `wyn test` PRINTS for a test block, and one case
# requires a FAILING assertion (the ✗ line is emitted from a different branch than
# the ✓ line, and only one of the two was ever exercised by accident). An EXPECT
# file in tests/regression/ runs under `wyn run` and must succeed, so it can
# express neither.
#
# THE DEFECT
#
# codegen spliced the test name into the emitted printf's FORMAT string, so every
# `%` in a name became a live conversion consuming an argument nobody passed:
#
#   test "a 30%-alpha stroke over itself"   ->   a 300x1p-1074lpha stroke over itself
#
# `%-a` read a nonexistent double off the stack. That is UNDEFINED BEHAVIOUR, not a
# cosmetic mangling - a name containing `%s` or `%n` would read or write through a
# wild pointer - and it is reachable from any test name that mentions a percentage,
# which is a natural thing to write about alpha, coverage or opacity. Found while
# mutation-testing WynCanvas's brush, whose suite has exactly such a name.
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
# Assert a substring is ABSENT - how the mangling is caught, since the mangled form
# is what a regression would print.
check_absent() {
    local name="$1" bad="$2" got="$3"
    if printf '%s' "$got" | grep -qF -- "$bad"; then
        printf '  FAIL  %s\n          must NOT contain: %s\n' "$name" "$bad"
        printf '%s\n' "$got" | sed 's/^/          | /'
        fail=$((fail + 1))
    else
        printf '  ok    %s\n' "$name"; pass=$((pass + 1))
    fi
}

root=$(mktemp -d) || exit 1
trap 'rm -rf "$root"' EXIT

n=0
fresh() {
    n=$((n + 1)); d="$root/case$n"; mkdir -p "$d/tests"
    printf '[project]\nname = "pct"\nversion = "0.1.0"\n' > "$d/wyn.toml"
}

# ---------------------------------------------------------------------------
# 1. A PASSING test whose name contains a percentage. This is the exact name from
#    WynCanvas's brush suite that exposed the bug.
# ---------------------------------------------------------------------------
fresh
cat > "$d/tests/test_pass.wyn" <<'WYN'
test "a 30%-alpha stroke over itself" {
    assert_eq(1, 1)
}
WYN
out=$( cd "$d" && "$WYN_ABS" test 2>&1 )
check        "a % in a passing test name prints literally" "a 30%-alpha stroke over itself" "$out"
check_absent "and is not consumed as a conversion"         "0x1p-"                          "$out"

# ---------------------------------------------------------------------------
# 2. A FAILING test whose name contains a percentage. Separate branch in codegen -
#    the ✗ line - and it was wrong in the same way.
# ---------------------------------------------------------------------------
fresh
cat > "$d/tests/test_fail.wyn" <<'WYN'
test "a 30%-alpha stroke over itself" {
    assert_eq(1, 2)
}
WYN
out=$( cd "$d" && "$WYN_ABS" test 2>&1 )
check        "a % in a FAILING test name prints literally" "a 30%-alpha stroke over itself" "$out"
check_absent "and the failing branch does not mangle it"   "0x1p-"                          "$out"
check        "the test still reports as failed"            "0 passed, 1 failed"             "$out"

# ---------------------------------------------------------------------------
# 3. The dangerous specifiers. `%s` would print a wild pointer as a string and
#    `%n` would WRITE through one, so these matter more than the arithmetic ones.
#    `%%` is included because a doubling bug that fired twice would eat it.
# ---------------------------------------------------------------------------
fresh
cat > "$d/tests/test_spec.wyn" <<'WYN'
test "100% coverage with %s and %d and %n and %% inside" {
    assert_eq(3, 3)
}
WYN
out=$( cd "$d" && "$WYN_ABS" test 2>&1 )
check "every specifier survives verbatim" \
      "100% coverage with %s and %d and %n and %% inside" "$out"

# ---------------------------------------------------------------------------
# 4. CONTROL: a name with no percent is unaffected. A fix that escaped too
#    eagerly, or that broke the ordinary path, has to show up somewhere.
# ---------------------------------------------------------------------------
fresh
cat > "$d/tests/test_plain.wyn" <<'WYN'
test "an ordinary name is unchanged" {
    assert_eq(4, 4)
}
test "and a second one still runs" {
    assert_eq(5, 5)
}
WYN
out=$( cd "$d" && "$WYN_ABS" test 2>&1 )
check "an ordinary test name is unchanged"    "an ordinary name is unchanged" "$out"
check "a sibling test still runs"             "and a second one still runs"   "$out"
check "both pass"                             "2 tests passed"                "$out"

echo ""
echo "test-name-percent: $pass pass, $fail fail"
[ "$fail" -eq 0 ]
