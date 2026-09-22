#!/bin/bash
# V-28: an Option/Result predicate on an int / float / bool receiver must be
# rejected AT CHECK TIME, not lowered and left to the C compiler.
#
#   x = 5
#   print(x.is_err())     # v1.21.0-dev: `wyn check` PASSED, `wyn run` ICEd with
#                         # "passing 'long long' to parameter of incompatible
#                         #  type 'ResultInt'"
#
# The rejected set is CLOSED, and it is closed because of one place in codegen:
# src/codegen_expr.c's "try Result/Option method dispatch" fallback lowers these
# names by ASSUMING the receiver is an Option/Result struct (`ResultInt_is_err(x)`,
# `OptionInt_is_some(x)`), so a numeric receiver is a guaranteed C type error - there
# is no shape in which it builds. Measured before this test was written: 6 names x 8
# receiver shapes the checker types as a scalar = 48 combinations, 48 build failures,
# 48 clean `wyn check`es.
#
# `unwrap_or` is the one name from that codegen block deliberately NOT rejected:
# `m.get(k).unwrap_or(d)` has a real lowering AHEAD of the blind fallback
# (hashmap_get_or_int) and does build, so rejecting it would break working code.
# The `unwrap_or` arm below is what keeps it out of the set.
#
# The no-regression half matters as much as the rejection half: `TYPE_INT` is the
# checker's fallback type for anything it could not resolve, so a rule keyed on the
# RECEIVER instead of on the method name would reject `StringBuilder.new().append()`,
# `3.times(f)` and every `Test.assert_*` call in the tree. Those arms are here to
# fail if anyone widens this rule that way. (2026-09)
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# reject <label> <source> <method-in-message>
reject(){
  printf '%b\n' "$2" > "$TMP/r.wyn"
  out=$("$WYNABS" check "$TMP/r.wyn" 2>&1); code=$?
  if [ $code -ne 0 ] && echo "$out" | grep -q "'$3()' needs"; then
    ok "reject: $1"
  else bad "reject: $1 (code=$code) [$(echo "$out" | tr '\n' '|')]"; fi
}

# allow <label> <source> <expected-stdout>
allow(){
  d="$TMP/a$PASS$FAIL"; mkdir -p "$d"
  printf '%b\n' "$2" > "$d/a.wyn"
  cout=$("$WYNABS" check "$d/a.wyn" 2>&1); ccode=$?
  if [ $ccode -ne 0 ]; then
    bad "allow: $1 - check rejected it [$(echo "$cout" | tr '\n' '|')]"; return
  fi
  got=$("$WYNABS" run "$d/a.wyn" 2>&1 | grep -v 'Compiled in')
  if [ "$got" = "$3" ]; then ok "allow: $1"
  else bad "allow: $1 - got [$(echo "$got" | tr '\n' '|')] want [$(echo "$3" | tr '\n' '|')]"; fi
}

# allow_has <label> <source> <substring-of-stdout>
# For programs whose output carries ANSI colour or a suite banner, where an exact
# match would be asserting somebody else's formatting.
allow_has(){
  d="$TMP/h$PASS$FAIL"; mkdir -p "$d"
  printf '%b\n' "$2" > "$d/a.wyn"
  cout=$("$WYNABS" check "$d/a.wyn" 2>&1); ccode=$?
  if [ $ccode -ne 0 ]; then
    bad "allow: $1 - check rejected it [$(echo "$cout" | tr '\n' '|')]"; return
  fi
  got=$("$WYNABS" run "$d/a.wyn" 2>&1)
  if echo "$got" | grep -q "$3"; then ok "allow: $1"
  else bad "allow: $1 - got [$(echo "$got" | tr '\n' '|')] want to contain [$3]"; fi
}

echo "-- rejected: every Option/Result predicate on an int variable"
for m in is_ok is_err is_some is_none unwrap unwrap_err; do
  reject "int var .$m()" "x = 5\nprint(x.$m())" "$m"
done

echo "-- rejected: the other scalar receiver shapes"
reject "int literal .is_err()"   'print(5.is_err())'          is_err
reject "parenthesised literal"   'print((5).is_some())'       is_some
reject "float var .is_some()"    "f = 1.5\nprint(f.is_some())"  is_some
reject "bool var .is_ok()"       "b = true\nprint(b.is_ok())"   is_ok

echo "-- rejected: the :: spelling lowers to a different C symbol (#369)"
reject ":: on an int var"        "x = 5\nprint(x::is_err())"    is_err
reject ":: on a float var"       "f = 1.5\nprint(f::is_none())" is_none

echo "-- allowed: every int method the language really has"
allow "int methods"  'x = 7
print(x.to_string())
print(x.to_int())
print(x.to_float())
print(x.abs())
print(x.pow(2))
print(x.min(3))
print(x.max(3))
print(x.clamp(1,5))
print(x.sign())
print(x.to_binary())
print(x.to_hex())' '7
7
7.0
7
49
3
7
5
1
111
7'
# Branched on rather than printed: how a bool RENDERS is a separate open item, and
# asserting it here would couple this gate to it.
allow "int bool-returning methods" 'x = 7
if x.is_even() { print("even") } else { print("odd") }
if x.is_odd() { print("is_odd") } else { print("not_odd") }
if x.is_positive() { print("pos") } else { print("nonpos") }
if x.is_negative() { print("neg") } else { print("nonneg") }
if x.is_zero() { print("zero") } else { print("nonzero") }' 'odd
is_odd
pos
nonneg
nonzero'

echo "-- allowed: float and bool methods"
allow "float methods" 'f = 2.5
print(f.to_string())
print(f.to_int())
print(f.floor())
print(f.ceil())
print(f.abs())
print(f.round_to(1))
if f.is_nan() { print("nan") } else { print("notnan") }
if f.is_finite() { print("finite") } else { print("infinite") }' '2.5
2
2.0
3.0
2.5
2.5
notnan
finite'
allow "bool methods" 'b = true
print(b.to_string())
print(b.to_int())
print(b.not())
print(b.and(false))
print(b.or(false))
print(b.xor(true))' 'true
1
false
false
true
false'

echo "-- allowed: unwrap_or, the one name with a real lowering (must stay OUT of the set)"
allow "m.get(k).unwrap_or(d)" 'm = {"a": 1}
print(m.get("a").unwrap_or(9))
print(m.get("zz").unwrap_or(9))' '1
9'

echo "-- allowed: the predicates on a RESOLVED Option/Result still work"
allow "resolved Result" 'fn f(n: int) -> Result<int, string> {
    if n > 0 { return Ok(n) }
    return Err("neg")
}
r = f(3)
print(r.is_ok())
print(r.is_err())
print(r.unwrap())' 'true
false
3'
allow "resolved Option" 'fn g(n: int) -> int? {
    if n > 0 { return Some(n) }
    return None
}
o = g(3)
print(o.is_some())
print(o.is_none())
print(o.unwrap_or(0))' 'true
false
3'

echo "-- allowed: receivers the checker could NOT resolve keep their lenient path"
# `TYPE_INT` is the checker's fallback for an unresolved expression, so all three of
# these receivers type as int. A rule keyed on the receiver instead of the method
# name would reject them; that is exactly what must not happen.
allow "StringBuilder handle (types as int)" 'fn main() {
    var sb = StringBuilder.new()
    sb.append("a")
    sb.append("b")
    print(sb.to_string())
}' 'ab'
allow "3.times(f) - a real int method absent from the signature table" 'fn nop() -> int { return 1 }
3.times(nop)
print("ok")' 'ok'
allow_has "Test namespace call (registered as an int placeholder)" 'import Test
Test.init("v28")
Test.assert_eq_int(1, 1, "one")
print("done")' 'done'

echo ""; echo "scalar-option-method: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
