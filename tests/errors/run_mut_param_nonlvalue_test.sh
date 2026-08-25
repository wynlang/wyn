#!/usr/bin/env bash
# Passing a NON-LVALUE to a `mut` parameter must be a CHECK-TIME error.
#
# THE DEFECT
#
# A `mut` parameter is written back through a pointer, so the argument needs an address.
# A literal or a temporary has none, and both were accepted:
#
#   fn add(mut n: int, by: int) { n = n + by }
#   add(3, 5)          # compiled clean, then SEGFAULTED -- the literal 3 was
#                      # dereferenced as an address
#   add(x + 2, 5)      # same
#   bump(make())       # failed the C COMPILE instead, naming generated code
#
# Two different late failures for one mistake, neither naming the argument. `xs[0]` was a
# third: `wyn run` segfaulted, because an index does not lower to C subscripting -- it
# lowers to `array_get_int(xs, 0)`, a call, so its result has no address either.
#
# All four are now rejected by `wyn check` with a message that names the argument position,
# the function, why it cannot work, and what to write instead.
#
# WHY A SHELL TEST AND NOT AN EXPECT FILE
#
# What is under test is the exit status and the TEXT of a diagnostic for programs that must
# NOT compile. An EXPECT file in tests/regression/ runs under `wyn run` and must succeed, so
# it can express neither. The positive cases (a variable, a nested field access) live in
# tests/regression/test_mut_param_free_fn.wyn.
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

cat > lit.wyn <<'EOF'
fn add(mut n: int, by: int) { n = n + by }
fn main() {
    add(3, 5)
    print("unreachable")
}
EOF

cat > expr.wyn <<'EOF'
fn add(mut n: int, by: int) { n = n + by }
fn main() {
    var x = 1
    add(x + 2, 5)
    print("unreachable")
}
EOF

cat > temp.wyn <<'EOF'
struct S { n: int }
fn bump(mut s: S) { s.n = s.n + 1 }
fn make() -> S { return S { n: 1 } }
fn main() {
    bump(make())
    print("unreachable")
}
EOF

cat > idx.wyn <<'EOF'
fn bump(mut n: int) { n = n + 1 }
fn main() {
    var xs = [1, 2, 3]
    bump(xs[0])
    print("unreachable")
}
EOF

# The forms that must be ACCEPTED. A guard that rejected these would be worse than the bug.
cat > ok_var.wyn <<'EOF'
fn add(mut n: int, by: int) { n = n + by }
fn main() {
    var x = 1
    add(x, 5)
    print("${x}")
}
EOF

# The EXPLICIT address-of spelling. Five stdlib tests write this (test_mut_refs,
# test_brutal_audit, test_stress, test_v16_final, test_final_validation) and it works, so a
# guard that rejected it would break in-use code. My first version DID reject it -- `make test`
# caught it as 5 new stdlib failures, which is the only reason this case is here.
cat > ok_amp.wyn <<'EOF'
fn inc(mut x: int) { x = x + 1 }
fn main() {
    var n = 10
    inc(&n)
    print("${n}")
}
EOF

# `&` on a NON-lvalue must still be rejected -- the recursion must not turn & into an
# escape hatch.
cat > bad_amp.wyn <<'EOF'
fn inc(mut x: int) { x = x + 1 }
fn main() {
    inc(&3)
    print("unreachable")
}
EOF

cat > ok_field.wyn <<'EOF'
struct Inner { n: int }
struct Outer { i: Inner }
fn bump(mut n: int) { n = n + 1 }
fn main() {
    var o = Outer { i: Inner { n: 5 } }
    bump(o.i.n)
    print("${o.i.n}")
}
EOF

# --- the bad forms are rejected, by every entry point -------------------------

for prog in lit expr temp idx; do
    for cmd in check build run; do
        "$WYN_ABS" $cmd "$prog.wyn" >/dev/null 2>&1
        check "$prog: wyn $cmd exits nonzero" "$([ $? -ne 0 ] && echo yes || echo no)" "yes"
    done

    msg=$("$WYN_ABS" check "$prog.wyn" 2>&1 || true)

    check "$prog: says the argument must be a variable" \
        "$(echo "$msg" | grep -c 'is declared `mut`, so it must be a variable')" "1"

    # The message must name the ARGUMENT POSITION and the FUNCTION, or the user has to guess
    # which argument of which call is at fault.
    check "$prog: names the argument position and the function" \
        "$(echo "$msg" | grep -cE "argument [0-9]+ of '(add|bump)'")" "1"

    check "$prog: explains why" "$(echo "$msg" | grep -c 'written back to the caller')" "1"
    check "$prog: suggests the fix" "$(echo "$msg" | grep -c 'assign it first')" "1"

    # It must NOT reach the C compiler or the runtime -- the whole point is that this is
    # caught at check time. A leftover .c means codegen ran.
    check "$prog: no generated .c is left behind" \
        "$(ls "$prog.wyn.c" 2>/dev/null | wc -l | tr -d ' ')" "0"
done

# The literal case used to SEGFAULT rather than fail to compile. Assert it is not a crash
# now: 139 is SIGSEGV, and a check-time rejection exits 1.
"$WYN_ABS" run lit.wyn >/dev/null 2>&1
rc=$?
check "lit: exits 1 (a clean error), not 139 (a segfault)" "$rc" "1"

# --- the good forms still work -----------------------------------------------

"$WYN_ABS" check ok_var.wyn >/dev/null 2>&1
check "ok_var: wyn check exits zero" "$([ $? -eq 0 ] && echo yes || echo no)" "yes"
check "ok_var: runs and the mutation lands" "$("$WYN_ABS" run ok_var.wyn 2>/dev/null | tail -1)" "6"

"$WYN_ABS" check ok_field.wyn >/dev/null 2>&1
check "ok_field: wyn check exits zero" "$([ $? -eq 0 ] && echo yes || echo no)" "yes"
check "ok_field: runs and the mutation lands" "$("$WYN_ABS" run ok_field.wyn 2>/dev/null | tail -1)" "6"

"$WYN_ABS" check ok_amp.wyn >/dev/null 2>&1
check "ok_amp: explicit &n is accepted" "$([ $? -eq 0 ] && echo yes || echo no)" "yes"
check "ok_amp: runs and the mutation lands" "$("$WYN_ABS" run ok_amp.wyn 2>/dev/null | tail -1)" "11"

"$WYN_ABS" check bad_amp.wyn >/dev/null 2>&1
check "bad_amp: &3 is still rejected" "$([ $? -ne 0 ] && echo yes || echo no)" "yes"

echo ""
echo "mut-param-nonlvalue: $pass pass, $fail fail"
[ "$fail" -eq 0 ] || exit 1
