#!/bin/bash
# #424: `Result<T, E>` works as a PARAMETER annotation.
#
# WHAT THIS REPLACES. The annotation was dropped and the parameter typed as `int`, so the
# first `.is_ok()` in the body was rejected with
#
#   Error at line 2: 'is_ok()' needs a Result receiver, not int
#
# - a message that sends the author to look at their VALUE when the ANNOTATION is what was
# thrown away. `r: int?` in the same position already resolved, which is what made this one
# missing arm rather than a design gap. A function could not take the language's own error
# type, so a Result had to be produced and consumed inside one function body.
#
# WHY THE ARMS BELOW ARE THE ARMS. Four separate places had to agree before this worked,
# and each one fails differently, so each is asserted:
#   the checker's THREE parameter ladders  - module fns, the signature pass, the body pass.
#     The body pass is what `r.is_ok()` sees; the signature pass is what the CALL is
#     validated against. Getting only the body pass emits "passing 'ResultInt' to parameter
#     of incompatible type 'long long'" at the call instead.
#   codegen's TWO parameter ladders        - the forward declaration and the definition.
#     Disagreeing between those two is "conflicting types for '<fn>'".
# The `param + return together` and `two Result params` arms are there because they are the
# shapes that catch a signature/definition mismatch rather than a body-typing one.
#
# EVERY PAYLOAD KIND, because the family name is computed per payload (ResultInt /
# ResultString / ResultFloat / ResultBool / Result<Struct>) and a fix that handled only the
# int family would pass the issue's own reproduction while leaving the rest broken.
#
# THE LIMITATION THIS GATE USED TO PIN IS NOW FIXED, and its arms are at the bottom (#450).
# A BARE `Err("x")` written directly as a call argument used to fail, because a bare
# constructor takes its family from its payload when no context names one - `Err("x")`
# names ResultString where the parameter is ResultInt - and the result was `internal
# codegen error` on ordinary user code. The call-argument path now sets
# current_assign_target_kind from the callee's DECLARED parameter, the way the
# struct-literal path already did per field.
#
# Read the #450 arms with one thing in mind: `take(Ok(5))` passed throughout, because an
# int ok payload names ResultInt anyway. Only the Err arms, and the non-int ok families,
# ever distinguished the bug from the fix.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# expect <label> <source> <expected-stdout>   -- BOTH modes
expect(){
  d="$TMP/c$PASS$FAIL"; mkdir -p "$d"
  printf '%b\n' "$2" > "$d/a.wyn"
  for mode in debug release; do
    if [ "$mode" = release ]; then
      got=$(perl -e 'alarm(90); exec @ARGV' -- "$WYNABS" run --release "$d/a.wyn" 2>&1)
    else
      got=$(perl -e 'alarm(90); exec @ARGV' -- "$WYNABS" run "$d/a.wyn" 2>&1)
    fi
    got=$(printf '%s' "$got" | sed 's/\x1b\[[0-9;]*m//g' | grep -vE 'Compiled in|^Warning|unused variable')
    if [ "$got" = "$3" ]; then ok "[$mode] $1"
    else bad "[$mode] $1 - got [$(echo "$got" | tr '\n' '|' | cut -c1-120)] want [$(echo "$3" | tr '\n' '|')]"; fi
  done
}

echo "=== #424: Result<T, E> as a parameter annotation ==="

# ------------------------------------------------------- the issue's own reproduction
expect "the reported repro: Result<int, string> param" \
  'fn take(r: Result<int, string>) -> int {\n    if r.is_ok() { return r.unwrap() }\n    return -1\n}\nfn main() { print("${take(Ok(5))}") }' \
  '5'

# ------------------------------------------------------------------ every payload kind
expect "Result<string, string>" \
  'fn take(r: Result<string, string>) -> string {\n    if r.is_ok() { return r.unwrap() }\n    return "none"\n}\nfn main() { print("${take(Ok("hi"))}") }' \
  'hi'

expect "Result<float, string>" \
  'fn take(r: Result<float, string>) -> float {\n    if r.is_ok() { return r.unwrap() }\n    return 0.0\n}\nfn main() { print("${take(Ok(2.5))}") }' \
  '2.5'

expect "Result<bool, string>" \
  'fn take(r: Result<bool, string>) -> bool {\n    if r.is_ok() { return r.unwrap() }\n    return false\n}\nfn main() { print("${take(Ok(true))}") }' \
  'true'

expect "Result<Struct, string> - the monomorphic family" \
  'struct P { x: int }\nfn take(r: Result<P, string>) -> int {\n    if r.is_ok() { return r.unwrap().x }\n    return -1\n}\nfn main() { print("${take(Ok(P { x: 7 }))}") }' \
  '7'

# ------------------------------------- the ERR side, from a typed source (not a bare literal)
expect "an Err value reaches the param and unwrap_err works" \
  'fn mk() -> Result<int, string> { return Err("boom") }\nfn take(r: Result<int, string>) -> string {\n    if r.is_err() { return r.unwrap_err() }\n    return "ok"\n}\nfn main() { print("${take(mk())}") }' \
  'boom'

expect "is_ok/is_err both answer on a param" \
  'fn mk(n: int) -> Result<int, string> {\n    if n > 0 { return Ok(n) }\n    return Err("neg")\n}\nfn take(r: Result<int, string>) -> string { return "${r.is_ok()} ${r.is_err()}" }\nfn main() {\n    print("${take(mk(1))}")\n    print("${take(mk(-1))}")\n}' \
  'true false
false true'

# --------------------------- the shapes that catch a SIGNATURE / DEFINITION mismatch
expect "two Result params in one signature" \
  'fn add(a: Result<int, string>, b: Result<int, string>) -> int {\n    return a.unwrap() + b.unwrap()\n}\nfn main() { print("${add(Ok(2), Ok(3))}") }' \
  '5'

expect "Result as BOTH a param and the return type" \
  'fn pass(r: Result<int, string>) -> Result<int, string> { return r }\nfn main() { print("${pass(Ok(9)).unwrap()}") }' \
  '9'

expect "a Result param beside ordinary params" \
  'fn f(n: int, r: Result<int, string>, s: string) -> string {\n    return "${n} ${r.unwrap()} ${s}"\n}\nfn main() { print("${f(1, Ok(2), "x")}") }' \
  '1 2 x'

# ------------------ THE CONTROL: the optional spelling that already worked must keep working
expect "control: int? in the same position (already worked)" \
  'fn take(o: int?) -> int {\n    if o.is_some() { return o.unwrap() }\n    return -1\n}\nfn main() {\n    print("${take(Some(5))}")\n    print("${take(None)}")\n}' \
  '5
-1'

# ------------------ THE RETURN TYPE, whose naming logic this change moved into one authority
# Behaviour must be byte-identical; the golden-C snapshots pin the emitted C, and these pin
# the observable answers for each family.
expect "return-type Result still works for every family" \
  'fn a() -> Result<int, string> { return Ok(1) }\nfn b() -> Result<string, string> { return Ok("s") }\nfn c() -> Result<float, string> { return Ok(1.5) }\nfn d() -> Result<bool, string> { return Ok(true) }\nfn main() { print("${a().unwrap()} ${b().unwrap()} ${c().unwrap()} ${d().unwrap()}") }' \
  '1 s 1.5 true'

expect "return-type Result<Struct, E> still works" \
  'struct Q { v: int }\nfn mk() -> Result<Q, string> { return Ok(Q { v: 3 }) }\nfn main() { print("${mk().unwrap().v}") }' \
  '3'

# ------------------ #450: A BARE CONSTRUCTOR AS A CALL ARGUMENT TAKES THE PARAMETER'S FAMILY
# Each of these was `internal codegen error` before the fix, except where noted. The
# `Ok(...)` arms are controls: an int ok payload already named ResultInt, so an arm built
# only on `take(Ok(5))` would have passed against the bug.
expect "#450 the reported repro: a bare Err as a call argument" \
  'fn take(r: Result<int, string>) -> int {\n    if r.is_ok() { return r.unwrap() }\n    return -1\n}\nfn main() { print("${take(Err("bad"))}") }' \
  '-1'

expect "#450 control: a bare Ok as a call argument (passed before the fix too)" \
  'fn take(r: Result<int, string>) -> int {\n    if r.is_ok() { return r.unwrap() }\n    return -1\n}\nfn main() { print("${take(Ok(5))}") }' \
  '5'

# The ok payload here is NOT int, so the family is not ResultInt and the Ok arm is a real
# test rather than a coincidence.
expect "#450 bare Ok and Err into a Result<string, string> param" \
  'fn f(r: Result<string, string>) -> string {\n    if r.is_ok() { return r.unwrap() }\n    return "ERR"\n}\nfn main() {\n    print("${f(Ok("yes"))}")\n    print("${f(Err("boom"))}")\n}' \
  'yes
ERR'

expect "#450 bare Err into a Result<float, string> param" \
  'fn f(r: Result<float, string>) -> float {\n    if r.is_ok() { return r.unwrap() }\n    return -1.5\n}\nfn main() { print("${f(Err("boom"))}") }' \
  '-1.5'

expect "#450 bare Err into a Result<bool, string> param" \
  'fn f(r: Result<bool, string>) -> bool {\n    if r.is_ok() { return r.unwrap() }\n    return false\n}\nfn main() { print("${f(Err("boom"))}") }' \
  'false'

# TWO constructors of DIFFERENT families in ONE argument list. This is the arm that fails
# if the target family is set and not restored: the second argument would be emitted with
# the first one's family. 1 + (-1) = 0.
expect "#450 two bare constructors in one call do not leak family to each other" \
  'fn take(r: Result<int, string>) -> int {\n    if r.is_ok() { return r.unwrap() }\n    return -1\n}\nfn pair(a: Result<int, string>, b: Result<int, string>) -> int {\n    return take(a) + take(b)\n}\nfn main() { print("${pair(Ok(1), Err("x"))}") }' \
  '0'

expect "#450 a bare Err beside ordinary arguments, at a non-zero index" \
  'fn f(n: int, r: Result<int, string>, s: string) -> string {\n    if r.is_ok() { return "${n} ${r.unwrap()} ${s}" }\n    return "${n} ERR ${s}"\n}\nfn main() { print("${f(1, Err("x"), "z")}") }' \
  '1 ERR z'

# The Option half of the same mechanism: a bare Some whose payload is not an int.
expect "#450 bare Some/None into a string? param" \
  'fn f(o: string?) -> string {\n    if o.is_some() { return o.unwrap() }\n    return "NONE"\n}\nfn main() {\n    print("${f(Some("hi"))}")\n    print("${f(None)}")\n}' \
  'hi
NONE'

echo "--- $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
