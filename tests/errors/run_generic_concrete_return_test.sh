#!/bin/bash
# A GENERIC FUNCTION'S DECLARED RETURN TYPE IS ITS RETURN TYPE.
#
# THE BUG. `wyn_infer_generic_call_type` (src/generics.c) resolved a generic function's
# return type only when it was a TYPE PARAMETER (`-> T`) or an array of one (`-> [T]`).
# A declared CONCRETE return type is neither, so it matched nothing and the function fell
# through to its last line:
#
#     // Fallback: first argument's inferred type (legacy behavior).
#
# which typed the CALL as its own ARGUMENT. One five-line program, four outcomes depending
# only on what you passed:
#
#     fn f<T>(v: T) -> int { return 1 }
#     var r = f(5)        print("${r}")  ->  1         correct, but only because int==int
#     var r = f(true)     print("${r}")  ->  "true"    WRONG VALUE, exit 0
#     var r = f("a")      print("${r}")  ->  SIGSEGV   (an int rendered as a char*)
#     var r = f(P{x:1})   print("${r}")  ->  internal codegen error
#
# `wyn check` passed every one of them. The bool row is the worst: a wrong answer at
# exit 0, which this project treats as worse than a crash - and the int row is why it
# survived, since every short example anyone writes passes an int.
#
# WHY THE ARMS ARE THE ARMS. The outcome depends on the PAIR (declared return type,
# argument type), and the bug is invisible whenever the two coincide. So this gate is a
# MATRIX, not a list of examples: every return shape {T, int, float, string, bool, [int],
# [string], void} against argument types that differ from it. An arm where the return type
# and the argument type agree cannot distinguish the fix from the bug, and the arms that
# do are marked.
#
# THE `-> T` AND VOID ARMS ARE CONTROLS, not coverage: they resolved correctly before the
# fix (`-> T` through the type-parameter branch, void by having no return type at all) and
# must keep doing so, because the fix adds a branch ahead of the fallback they rely on.
#
# NOT FIXED HERE, and pinned at the bottom so this gate cannot be read as covering it: a
# STRUCT or ENUM return type (`fn mk<T>(v: T) -> P`) still has no resolution and still
# reaches the fallback. Only the four primitives and arrays of them are resolved.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

PRE='struct P { x: int }'

# expect <label> <fn-decl> <main-body> <expected-stdout>   -- BOTH modes
expect(){
  d="$TMP/c$PASS$FAIL"; mkdir -p "$d"
  printf '%b\n' "$PRE\n$2\nfn main() {\n  $3\n}" > "$d/a.wyn"
  for mode in debug release; do
    if [ "$mode" = release ]; then
      got=$(perl -e 'alarm(90); exec @ARGV' -- "$WYNABS" run --release "$d/a.wyn" 2>&1)
    else
      got=$(perl -e 'alarm(90); exec @ARGV' -- "$WYNABS" run "$d/a.wyn" 2>&1)
    fi
    got=$(printf '%s' "$got" | sed 's/\x1b\[[0-9;]*m//g' | grep -vE 'Compiled in|^Warning|unused variable')
    if [ "$got" = "$4" ]; then ok "[$mode] $1"
    else bad "[$mode] $1 - got [$(printf '%s' "$got" | tr '\n' '|' | cut -c1-110)] want [$4]"; fi
  done
}

echo "=== a generic function's declared return type is its return type ==="

# ---------------- the arms that DISTINGUISH the fix from the bug -----------------------
# Each one's return type differs from its argument type, so the fallback produced a
# visibly wrong answer. The comment records what it did before.
expect "-> int with a BOOL argument (was the wrong value, 'true', at exit 0)" \
  'fn f<T>(v: T) -> int { return 1 }' 'var r = f(true)\n  print("${r}")' '1'

expect "-> int with a STRING argument (was a SIGSEGV)" \
  'fn f<T>(v: T) -> int { return 1 }' 'var r = f("a")\n  print("${r}")' '1'

expect "-> int with TWO string arguments (was a SIGSEGV)" \
  'fn f<T>(a: T, b: T) -> int { return 1 }' 'var r = f("a", "b")\n  print("${r}")' '1'

expect "-> int with a STRUCT argument (was an internal codegen error)" \
  'fn f<T>(v: T) -> int { return 1 }' 'var r = f(P { x: 9 })\n  print("${r}")' '1'

expect "-> string with a STRUCT argument (was an internal codegen error)" \
  'fn f<T>(v: T) -> string { return "s" }' 'var r = f(P { x: 9 })\n  print("${r}")' 's'

expect "-> string with an INT argument" \
  'fn f<T>(v: T) -> string { return "s" }' 'var r = f(5)\n  print("${r}")' 's'

expect "-> bool with an INT argument" \
  'fn f<T>(v: T) -> bool { return true }' 'var r = f(5)\n  print("${r}")' 'true'

expect "-> float with a STRING argument" \
  'fn f<T>(v: T) -> float { return 1.5 }' 'var r = f("a")\n  print("${r}")' '1.5'

expect "-> int with a FLOAT argument" \
  'fn f<T>(v: T) -> int { return 7 }' 'var r = f(1.5)\n  print("${r}")' '7'

# An array of a concrete NON-int element: the `-> [T]` branch defaulted the element to
# TYPE_INT when the name was not a type parameter, which is right for [int] by luck.
#
# THESE ASSERT AN ELEMENT, NOT THE LENGTH. The first version of both arms asserted
# `r.len()`, and mutating the element-resolution line out changed nothing - `len()` is the
# same whatever the element type, so the arms were vacuous. With `r[0]` the mutation makes
# the `[string]` arm print `0` instead of `a`: a silently wrong value, which is the actual
# defect.
expect "-> [string] with an INT argument (element used to default to int; prints 0 without the fix)" \
  'fn f<T>(v: T) -> [string] { return ["a", "b"] }' 'var r = f(5)\n  print("${r[0]}")' 'a'

# Stays green under the mutation, because [int] is exactly what the old default produced.
# Kept as a control, labelled, so it is not read as covering the element resolution.
expect "control: -> [int] with a STRING argument (agrees with the old int default)" \
  'fn f<T>(v: T) -> [int] { return [7, 8, 9] }' 'var r = f("a")\n  print("${r[0]}")' '7'

# ---------------- CONTROLS: the shapes that already worked ----------------------------
# The fix inserts a branch ahead of the fallback these rely on, so they are asserted.
expect "control: -> T with an INT argument" \
  'fn f<T>(v: T) -> T { return v }' 'var r = f(5)\n  print("${r}")' '5'
expect "control: -> T with a STRING argument" \
  'fn f<T>(v: T) -> T { return v }' 'var r = f("a")\n  print("${r}")' 'a'
expect "control: -> T with a STRUCT argument" \
  'fn f<T>(v: T) -> T { return v }' 'var r = f(P { x: 9 })\n  print("${r.x}")' '9'
expect "control: -> int with an INT argument (agreed, so it always worked)" \
  'fn f<T>(v: T) -> int { return 1 }' 'var r = f(5)\n  print("${r}")' '1'
expect "control: a VOID generic with a STRUCT argument" \
  'fn f<T>(v: T) { print("got") }' 'f(P { x: 9 })' 'got'

# ---------------- A PRE-EXISTING LIMITATION, PINNED -----------------------------------
# A STRUCT return type is still unresolved and still reaches the fallback, so the call is
# typed as its argument. Asserted as the CURRENT behaviour: if someone resolves struct
# return types, this arm goes red and should be promoted.
d="$TMP/pin"; mkdir -p "$d"
printf '%b\n' 'struct P { x: int }\nfn mk<T>(v: T) -> P { return P { x: 1 } }\nfn main() {\n  var r = mk("a")\n  print("${r.x}")\n}' > "$d/a.wyn"
out=$(perl -e 'alarm(90); exec @ARGV' -- "$WYNABS" run "$d/a.wyn" 2>&1)
if printf '%s\n' "$out" | grep -qE 'internal codegen error|Segmentation|error'; then
    ok "pinned (still broken, filed): a STRUCT return type is not resolved"
else
    bad "pinned limitation CHANGED - a struct return type now works; promote this arm"
fi

echo ""
echo "generic-concrete-return: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
