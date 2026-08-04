#!/usr/bin/env bash
# An import that cannot be resolved must FAIL the build, not emit broken C.
#
# THE DEFECT
#
# load_module already printed a precise error ("Module 'x' not found" / "Package 'x'
# not installed") and then returned NULL, and every caller treated NULL as "carry on
# without it". So one build made three contradictory statements:
#
#   $ wyn build form.wyn          # form.wyn does `import gui`, gui not installed
#   Error: Package 'gui' not installed
#   Run: wyn pkg install gui
#   ✓ form.wyn: no errors          <-- contradicts the line above
#   $ echo $?
#   0                              <-- contradicts both
#
# and codegen emitted C with a hole where the missing module's call should have been:
#
#   long long ui = ;
#
# which then failed in the C compiler, pointing at generated code the user never
# wrote. Found while building VisualWyn (repos/gui), whose designer generates code
# that imports the `gui` package: the first thing a new user hits is a C syntax error
# in a file they did not write.
#
# The fix mirrors has_circular_import, which already latches exactly this way.
#
# WHY A SHELL TEST AND NOT AN EXPECT FILE
#
# What is under test is the EXIT STATUS and the ABSENCE of an artifact, for a program
# that must NOT compile. An EXPECT file in tests/regression/ runs under `wyn run` and
# must succeed, so it can express neither.
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

# A module that does not exist at all.
cat > missing.wyn <<'EOF'
import definitely_not_a_real_module

fn main() -> int {
    return 0
}
EOF

# A known PACKAGE name that is not installed here - the shape VisualWyn hits, and the
# one that produced `long long ui = ;`.
cat > pkg.wyn <<'EOF'
import gui

fn main() -> int {
    ui = gui.Ui_new(400, 300)
    print("unreachable")
    return 0
}
EOF

for f in missing pkg; do
    # Every entry point must agree. `check` used to say "no errors" while `build`
    # emitted broken C, so testing one would not have caught the other.
    "$WYN_ABS" check "$f.wyn" >/dev/null 2>&1
    check "$f: wyn check exits nonzero" "$([ $? -ne 0 ] && echo yes || echo no)" "yes"

    "$WYN_ABS" build "$f.wyn" >/dev/null 2>&1
    check "$f: wyn build exits nonzero" "$([ $? -ne 0 ] && echo yes || echo no)" "yes"

    "$WYN_ABS" run "$f.wyn" >/dev/null 2>&1
    check "$f: wyn run exits nonzero" "$([ $? -ne 0 ] && echo yes || echo no)" "yes"

    # The build must stop BEFORE codegen. A leftover .c is the signature of the
    # original bug: it contained `long long ui = ;`.
    check "$f: no generated .c is left behind" "$(ls "$f.wyn.c" 2>/dev/null | wc -l | tr -d ' ')" "0"

    # And it must say why, in the user's terms.
    msg=$("$WYN_ABS" build "$f.wyn" 2>&1 || true)
    check "$f: reports the unresolved import" \
        "$(echo "$msg" | grep -cE "not found|not installed" | head -1)" "1"
    # It must NOT also claim success. That self-contradiction was the worst part.
    check "$f: does not also claim 'no errors'" "$(echo "$msg" | grep -c "no errors" || true)" "0"
done

# A BUILTIN module has no .wyn file and legitimately resolves to NULL. It must keep
# working - the fix latches inside the not-found branch, which builtins return before
# reaching, and getting that wrong would break `import math` for everyone.
cat > builtin.wyn <<'EOF'
import math

fn main() -> int {
    print(math.sqrt(16.0))
    return 0
}
EOF
out=$("$WYN_ABS" run builtin.wyn 2>&1)
rc=$?
check "a builtin import still compiles and runs" "$rc" "0"
check "and produces the right answer" "$(echo "$out" | grep -c '^4')" "1"

echo ""
echo "unresolved-import: $pass pass, $fail fail"
[ "$fail" -eq 0 ]
