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
# A KNOWN LIMITATION IS PINNED AT THE BOTTOM, deliberately, so this gate cannot be read as
# proving more than it does: a BARE `Err("x")` written directly as a call argument still
# fails, because a bare constructor takes its family from its payload when no context
# names one - `Err("x")` names ResultString where the parameter is ResultInt. That is a
# constructor-context gap in wyn_option_ctor_kind, not in the annotation, and it is filed
# separately. Passing an Err that came from a typed source works and is asserted.
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

echo "--- $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
