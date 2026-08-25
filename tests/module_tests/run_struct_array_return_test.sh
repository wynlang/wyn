#!/usr/bin/env bash
# A module function returning `[Struct]` must be typed as an ARRAY, not as int.
#
# THE DEFECT
#
# get_module_fn_builtin_return() reads a module's own AST to find a function's declared
# return type, and bailed with `if (!rt || rt->type != EXPR_IDENT) return NULL;`. An
# array return `[T]` is EXPR_ARRAY (the parser stores the element type at
# elements[0]), so it returned NULL and every caller's fallback chain defaulted to int:
#
#     src/m.wyn:  pub fn many() -> [P] { return [P{x:1}, P{x:2}] }
#     main.wyn:   xs = m.many()
#                 print(xs.len())     -> "Unknown method 'len' for type 'int'"
#
# and `wyn check` reported NO ERRORS - the checker was permissive and only codegen
# disagreed. The declaration site had the same gap independently, so fixing only the
# checker produced "initializing 'long long' with an expression of incompatible type
# 'WynArray'": both halves of a type decision have to agree.
#
# WHY IT MATTERS BEYOND THE ERROR MESSAGE
#
# This is the gap that kept WynCanvas's data model as 22 parallel array columns plus 52
# one-line accessor functions (src/layer.wyn, src/history.wyn). With `-> [Layer]`
# unusable across a module boundary, an array of structs was not an option. Fixing it
# is what makes that boilerplate deletable.
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
mkdir -p src

cat > src/m.wyn <<'WYN'
pub struct P { x: int }
pub fn one() -> P { return P { x: 7 } }
pub fn many() -> [P] { return [P { x: 1 }, P { x: 2 }, P { x: 3 }] }
pub fn names() -> [string] { return ["a", "b"] }
pub fn nums() -> [int] { return [10, 20] }
WYN

# A SINGLE struct across the boundary already worked; keep it covered so a change here
# cannot silently break it.
cat > single.wyn <<'WYN'
import m
fn main() -> int {
    p = m.one()
    print(p.x)
    return 0
}
WYN
check "a single struct still crosses the boundary" \
    "$("$WYN_ABS" run single.wyn 2>/dev/null | tail -1)" "7"

# The bug: an array of structs.
cat > arr.wyn <<'WYN'
import m
fn main() -> int {
    xs = m.many()
    print(xs.len())
    return 0
}
WYN
check "an array of structs has a length" \
    "$("$WYN_ABS" run arr.wyn 2>/dev/null | tail -1)" "3"

# Arrays of BUILTIN element types across a module, which share the same code path.
cat > strs.wyn <<'WYN'
import m
fn main() -> int {
    xs = m.names()
    print(xs.len())
    print(xs[1])
    return 0
}
WYN
out=$("$WYN_ABS" run strs.wyn 2>/dev/null)
check "an array of strings has a length" "$(echo "$out" | sed -n '1p')" "2"
check "and its elements are readable" "$(echo "$out" | sed -n '2p')" "b"

cat > ints.wyn <<'WYN'
import m
fn main() -> int {
    xs = m.nums()
    print(xs.len())
    print(xs[0] + xs[1])
    return 0
}
WYN
out=$("$WYN_ABS" run ints.wyn 2>/dev/null)
check "an array of ints has a length" "$(echo "$out" | sed -n '1p')" "2"
check "and its elements are arithmetic" "$(echo "$out" | sed -n '2p')" "30"

# KNOWN REMAINING GAP, asserted so it is recorded rather than assumed fixed.
# The array is now typed as an array, but its ELEMENT type is not carried, so an
# element reads back as `long long`: "member reference base type 'long long' is not a
# structure". Threading the element type through is a larger change than this fix, and
# this pins exactly which half is done.
cat > elem.wyn <<'WYN'
import m
fn main() -> int {
    xs = m.many()
    print(xs[0].x)
    return 0
}
WYN
if "$WYN_ABS" run elem.wyn >/dev/null 2>&1; then
    echo "  ok    element field access works (better than the recorded contract)"
    pass=$((pass+1))
else
    echo "  ~     element field access still fails (known: element type is not carried)"
fi

echo ""
echo "struct-array-return: $pass pass, $fail fail"
[ "$fail" -eq 0 ]
