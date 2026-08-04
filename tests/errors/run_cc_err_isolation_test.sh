#!/usr/bin/env bash
# Two concurrent `wyn run` invocations in ONE directory must each report their OWN
# C-compiler diagnostic.
#
# THE DEFECT
#
# The C compiler's stderr was redirected to a bare relative `wyn_cc_err.txt`, i.e. a
# file in the CURRENT DIRECTORY. Two `wyn run` processes in the same directory - two
# parallel agents, a `make -j`, or the stdlib runner's 4+ jobs - therefore wrote to and
# read from the same file, and each could report the OTHER's error text: a diagnostic
# naming a source file the user was not building. That is worse than no diagnostic,
# because it sends you after the wrong file. The `wyn build` path was fixed first (a
# per-process file in the temp dir); the `run` path was not, and neither was the
# shared-library path.
#
# WHY A SHELL TEST AND NOT AN EXPECT FILE
#
# What is under test is a RACE between two processes and the text each one prints on
# FAILURE. An EXPECT file in tests/regression/ runs one `wyn run` that must succeed, so
# it can express neither half.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"

pass=0
fail=0
check() {
    if [ "$2" = "$3" ]; then
        echo "  ok    $1"
        pass=$((pass+1))
    else
        echo "  FAIL  $1"
        echo "          expected: $3"
        echo "          actual:   $2"
        fail=$((fail+1))
    fi
}

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cd "$work" || exit 1

# Two programs that each fail in the C compiler, not the checker: a call to an
# undeclared namespace method is deliberately let through by the checker (an allowlist
# there would reject ~100 real runtime functions), so it surfaces as a C error naming
# the generated function. Each file names a DISTINCT one, which is what lets us tell
# whose diagnostic we got.
cat > alpha.wyn <<'EOF'
fn main() -> int {
    Time.alpha_marker_aaa()
    return 0
}
EOF
cat > beta.wyn <<'EOF'
fn main() -> int {
    Time.beta_marker_bbb()
    return 0
}
EOF

# Run both concurrently, in this one shared directory, with WYN_DEBUG so the captured
# compiler text is echoed back to us.
WYN_DEBUG=1 "$WYN_ABS" run alpha.wyn > alpha.log 2>&1 &
pid_a=$!
WYN_DEBUG=1 "$WYN_ABS" run beta.wyn > beta.log 2>&1 &
pid_b=$!
wait $pid_a
wait $pid_b

# Neither log may mention the OTHER program's marker. Before the fix, whichever process
# lost the race read its sibling's file and reported the sibling's symbol.
a_has_foreign=$(grep -c 'beta_marker_bbb' alpha.log 2>/dev/null || true)
b_has_foreign=$(grep -c 'alpha_marker_aaa' beta.log 2>/dev/null || true)
check "alpha's diagnostic does not name beta's symbol" "$a_has_foreign" "0"
check "beta's diagnostic does not name alpha's symbol" "$b_has_foreign" "0"

# And each must still produce a real diagnostic - a fix that simply lost the error text
# would pass the two checks above while making the tool worse.
a_has_own=$(grep -c 'alpha_marker_aaa' alpha.log 2>/dev/null || true)
b_has_own=$(grep -c 'beta_marker_bbb' beta.log 2>/dev/null || true)
check "alpha still reports its own failure" "$([ "$a_has_own" -gt 0 ] && echo yes || echo no)" "yes"
check "beta still reports its own failure" "$([ "$b_has_own" -gt 0 ] && echo yes || echo no)" "yes"

# The scratch file must not be left in the user's working directory. It used to be
# created there and unlinked, so a crash or a concurrent read left it behind - stray
# copies were found in seven directories of this workspace.
stray=$(ls wyn_cc_err.txt 2>/dev/null | wc -l | tr -d ' ')
check "no wyn_cc_err.txt left in the working directory" "$stray" "0"

echo ""
echo "cc-err-isolation: $pass pass, $fail fail"
[ "$fail" -eq 0 ]
