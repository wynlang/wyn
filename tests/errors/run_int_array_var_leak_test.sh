#!/usr/bin/env bash
# The [int]-array dispatch table must be forgotten at a function boundary.
#
# THE DEFECT
#
# `int_array_var_names` is keyed on the variable NAME only, and is consulted to
# choose the C representation ([int] packs into WynIntArray; everything else is a
# WynArray) plus the matching accessors. It had no reset at a function boundary,
# so it accumulated every [int] local in the translation unit:
#
#     fn a() -> int { var xs: [int] = [1,2,3]; return xs.len() }
#     fn b() -> int { var xs = ["a","b"];      return xs.len() }
#
#   -> initializing 'WynArray' with an expression of incompatible type 'WynIntArray'
#      passing 'WynArray' to parameter of incompatible type 'WynIntArray'
#
# surfacing as a bare "internal codegen error". ORDER-DEPENDENT: swap the two
# functions and it compiles, which is why it hid.
#
# THE FAMILY
#
# This is the FOURTH of these name-keyed dispatch tables to bite. `float_var_names`
# (#233), `array_var_names` and `sb_var_names` (#259) all had the identical missing
# reset; the StringBuilder one was worse than a compile error -- it made a plain
# string answer .len() from a builder, printing 0 where the answer was 2.
#
# A census while fixing this found 13 name-keyed tables in codegen.c. The other
# five without a reset -- str_array_var_names, int_array_veto_names,
# spawn_array_vars, string_future_vars, tuple_var_names -- were each probed with
# the shape above and did NOT reproduce, so they are deliberately left alone rather
# than "fixed" speculatively. They remain suspects, not proven safe: the cheap
# check is to declare the typed thing in one function, then a differently-typed
# variable of the SAME NAME in a later one, and call a method they share.
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

# [int] first, then a same-named string array. This is the order that failed.
cat > intfirst.wyn <<'WYN'
fn a() -> int {
    var xs: [int] = [1, 2, 3]
    return xs.len()
}
fn b() -> int {
    var xs = ["a", "b"]
    return xs.len()
}
fn main() -> int {
    print(a())
    print(b())
    return 0
}
WYN
out="$("$WYN_ABS" run intfirst.wyn 2>/dev/null)"
check "an [int] local does not poison a later same-named string array (int)" \
    "$(echo "$out" | sed -n '1p')" "3"
check "an [int] local does not poison a later same-named string array (string)" \
    "$(echo "$out" | sed -n '2p')" "2"

# The reverse order compiled before the fix; keep it covered so the reset cannot
# break the direction that already worked.
cat > strfirst.wyn <<'WYN'
fn a() -> int {
    var xs = ["a", "b"]
    return xs.len()
}
fn b() -> int {
    var xs: [int] = [1, 2, 3]
    return xs.len()
}
fn main() -> int {
    print(a())
    print(b())
    return 0
}
WYN
out="$("$WYN_ABS" run strfirst.wyn 2>/dev/null)"
check "the reverse order still works (string)" "$(echo "$out" | sed -n '1p')" "2"
check "the reverse order still works (int)"    "$(echo "$out" | sed -n '2p')" "3"

# The packing itself must survive the reset: a [int] array used WITHIN one
# function still needs its WynIntArray representation and accessors. If the reset
# fired too early this would regress.
cat > within.wyn <<'WYN'
fn sum_of() -> int {
    var ns: [int] = [10, 20, 30]
    var total = 0
    for n in ns { total = total + n }
    return total + ns.len()
}
fn main() -> int {
    print(sum_of())
    return 0
}
WYN
check "an [int] array still works within its own function" \
    "$("$WYN_ABS" run within.wyn 2>/dev/null | tail -1)" "63"

# Two [int] arrays of the same name in different functions must BOTH still pack.
cat > twoint.wyn <<'WYN'
fn a() -> int {
    var ns: [int] = [1, 2, 3]
    return ns.len()
}
fn b() -> int {
    var ns: [int] = [4, 5]
    return ns.len()
}
fn main() -> int {
    print(a())
    print(b())
    return 0
}
WYN
out="$("$WYN_ABS" run twoint.wyn 2>/dev/null)"
check "two same-named [int] arrays both work (first)"  "$(echo "$out" | sed -n '1p')" "3"
check "two same-named [int] arrays both work (second)" "$(echo "$out" | sed -n '2p')" "2"

echo
echo "int-array var leak: $pass pass, $fail fail"
[ "$fail" -eq 0 ] || exit 1
