#!/bin/bash
# The Option/Result COMBINATOR API (#392): map, and_then, filter, expect, or_else,
# map_err.
#
# THESE USED TO BE REJECTED. src/types.c advertised ten of them while lowering them to
# `wyn_optional_map` / `wyn_result_map`, archive functions that take `WynOptional*` /
# `WynResult*` - a heap-boxed representation codegen never emits. Codegen emits the
# monomorphic value-struct families (`OptionInt`, `ResultString`, ...), so those rows
# could not link, and a call passed `wyn check` and then died in the C compiler with
# "unknown method 'OptionInt.map' on namespace 'OptionInt'".
#
# They are now lowered INLINE, as a GNU statement expression over the family struct. No
# runtime function was added, which is why this gate does not need a static header matrix
# the way the family-completeness gate does: the only names these lowerings call are
# `<Family>_Some/_None/_Ok/_Err`, and that gate already pins all of them in BOTH headers.
#
# WHY EVERY ARM RUNS IN BOTH BUILD MODES. `wyn build` and `wyn build --release` emit
# different runtime headers (wyn_runtime.h vs the hand-maintained wyn_runtime_slim.h), so
# a lowering that names something absent from the slim one is green in one mode and red
# in the other. And `wyn build --release` deliberately keeps the FULL header, so the last
# section uses `wyn run --release` - the only command that actually compiles the slim
# header.
#
# WHY THE ARMS ASSERT PAYLOAD-SPECIFIC OPERATIONS AND NOT JUST PRINTED BYTES. `map`
# CHANGES THE FAMILY - the result family comes from the callback's RETURN type, not the
# receiver's - so `int?.map(fn(x: int) -> string {..})` must become an OptionString. A
# family resolved to the wrong payload can still print something plausible (that is
# exactly how #413's `Result<string,E>.unwrap_or` printed a pointer as a number), so each
# cross-type arm then calls an operation only the NEW payload supports: `.upper()` on a
# string, `+ 0.25` on a float.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# both <label> <program> <expected-stdout>  - builds and runs in dev AND --release.
both() {
    local label="$1" prog="$2" want="$3" mode got
    printf '%s\n' "$prog" > "$TMP/t.wyn"
    for mode in "" "--release"; do
        rm -f "$TMP/t" "$TMP/t.wyn.c"
        if ! (cd "$TMP" && perl -e 'alarm(180); exec @ARGV' -- "$WYN" build $mode "$TMP/t.wyn") \
             > "$TMP/b.log" 2>&1; then
            bad "$label (${mode:-dev}) builds"
            grep -m1 -oE "(error|Error)[:@] .*" "$TMP/b.log" | cut -c1-110 | sed 's/^/          /'
            continue
        fi
        got=$(perl -e 'alarm(60); exec @ARGV' -- "$TMP/t" 2>&1)
        if [ "$got" = "$want" ]; then ok "$label (${mode:-dev})"
        else bad "$label (${mode:-dev}): want [$(echo "$want" | tr '\n' '|')] got [$(echo "$got" | tr '\n' '|')]"; fi
    done
}

# reject <label> <program> <substring the message must contain>
reject() {
    local label="$1" out code
    printf '%s\n' "$2" > "$TMP/r.wyn"
    out=$(perl -e 'alarm(120); exec @ARGV' -- "$WYN" check "$TMP/r.wyn" 2>&1); code=$?
    if [ $code -ne 0 ] && echo "$out" | grep -q "$3"; then ok "reject: $label"
    else bad "reject: $label (code=$code) [$(echo "$out" | tr '\n' '|' | cut -c1-130)]"; fi
}

# ---------------------------------------------------------------------------
# Option, per payload. Each arm exercises BOTH the Some path and the None path, so a
# lowering that hard-codes one branch cannot pass.
# ---------------------------------------------------------------------------
OI='fn s_() -> int? { return Some(4) }
fn n_() -> int? { return None }'
OS='fn s_() -> string? { return Some("ab") }
fn n_() -> string? { return None }'
OF='fn s_() -> float? { return Some(1.5) }
fn n_() -> float? { return None }'
OB='fn s_() -> bool? { return Some(true) }
fn n_() -> bool? { return None }'

echo "-- Option.map: Some(x) -> Some(f(x)), None -> None"
both "Option<int>.map same payload" "$OI
fn main() {
  print(s_().map(fn(x: int) -> int { return x * 3 }).unwrap_or(-1))
  print(n_().map(fn(x: int) -> int { return x * 3 }).unwrap_or(-1))
}" '12
-1'
both "Option<string>.map same payload" "$OS
fn main() {
  print(s_().map(fn(x: string) -> string { return x.upper() }).unwrap_or(\"-\"))
  print(n_().map(fn(x: string) -> string { return x.upper() }).unwrap_or(\"-\"))
}" 'AB
-'
both "Option<float>.map same payload" "$OF
fn main() {
  print(s_().map(fn(x: float) -> float { return x + 0.25 }).unwrap_or(9.5))
  print(n_().map(fn(x: float) -> float { return x + 0.25 }).unwrap_or(9.5))
}" '1.75
9.5'
both "Option<bool>.map same payload" "$OB
fn main() {
  print(s_().map(fn(x: bool) -> bool { return not x }).unwrap_or(true))
  print(n_().map(fn(x: bool) -> bool { return not x }).unwrap_or(true))
}" 'false
true'

echo "-- Option.map CHANGES the family: the result family is the callback's return type"
# `.upper()` / `+ 0.25` on the mapped value is the load-bearing part: it can only compile
# and answer correctly if the result really is the NEW payload family.
both "Option<int>.map -> Option<string>" "$OI
fn main() {
  print(s_().map(fn(x: int) -> string { return \"i\${x}\" }).unwrap_or(\"-\").upper())
  print(n_().map(fn(x: int) -> string { return \"i\${x}\" }).unwrap_or(\"-\").upper())
}" 'I4
-'
both "Option<string>.map -> Option<int>" "$OS
fn main() {
  print(s_().map(fn(x: string) -> int { return x.len() }).unwrap_or(-1) + 1)
  print(n_().map(fn(x: string) -> int { return x.len() }).unwrap_or(-1) + 1)
}" '3
0'
both "Option<int>.map -> Option<float>" "$OI
fn main() {
  print(s_().map(fn(x: int) -> float { return x.to_float() }).unwrap_or(9.5) + 0.25)
  print(n_().map(fn(x: int) -> float { return x.to_float() }).unwrap_or(9.5) + 0.25)
}" '4.25
9.75'
both "Option<bool>.map -> Option<int>" "$OB
fn main() {
  print(s_().map(fn(x: bool) -> int { if x { return 1 } return 0 }).unwrap_or(-1))
  print(n_().map(fn(x: bool) -> int { if x { return 1 } return 0 }).unwrap_or(-1))
}" '1
-1'
# A NAMED function here, not a lambda, and the reason is a PRE-EXISTING gap pinned at the
# end of this file: LambdaExpr carries no declared return type at all (the parser drops
# `-> bool`), so a lambda's return type is its BODY's inferred type, and a comparison body
# types as int. A named function's declared return type IS recorded, so this arm is also
# the one that shows the result family following the DECLARATION.
both "Option<float>.map -> Option<bool>" "$OF
fn big(x: float) -> bool { return x > 1.0 }
fn main() {
  print(s_().map(big).unwrap_or(false))
  print(n_().map(big).unwrap_or(false))
}" 'true
false'

echo "-- Option.and_then: the callback already returns an Option, so it is flattened"
both "Option<int>.and_then same family" "$OI
fn dbl(x: int) -> int? { return Some(x + x) }
fn main() {
  print(s_().and_then(dbl).unwrap_or(-1))
  print(n_().and_then(dbl).unwrap_or(-1))
}" '8
-1'
both "Option<int>.and_then can answer None" "$OI
fn odd(x: int) -> int? { if x % 2 == 1 { return Some(x) } return None }
fn main() {
  print(s_().and_then(odd).unwrap_or(-1))
  print(n_().and_then(odd).unwrap_or(-1))
}" '-1
-1'
both "Option<string>.and_then -> Option<int>" "$OS
fn sz(x: string) -> int? { return Some(x.len()) }
fn main() {
  print(s_().and_then(sz).unwrap_or(-1) + 1)
  print(n_().and_then(sz).unwrap_or(-1) + 1)
}" '3
0'
both "Option<float>.and_then -> Option<string>" "$OF
fn fs(x: float) -> string? { return Some(\"f\") }
fn main() {
  print(s_().and_then(fs).unwrap_or(\"-\").upper())
  print(n_().and_then(fs).unwrap_or(\"-\").upper())
}" 'F
-'
both "Option<bool>.and_then -> Option<bool>" "$OB
fn flip(x: bool) -> bool? { return Some(not x) }
fn main() {
  print(s_().and_then(flip).unwrap_or(true))
  print(n_().and_then(flip).unwrap_or(true))
}" 'false
true'

echo "-- Option.filter: Some(x) when p(x), else None - and the family never changes"
both "Option<int>.filter" "$OI
fn main() {
  print(s_().filter(fn(x: int) -> bool { return x > 0 }).unwrap_or(-1))
  print(s_().filter(fn(x: int) -> bool { return x > 9 }).unwrap_or(-1))
  print(n_().filter(fn(x: int) -> bool { return x > 0 }).unwrap_or(-1))
}" '4
-1
-1'
both "Option<string>.filter" "$OS
fn main() {
  print(s_().filter(fn(x: string) -> bool { return x.len() > 1 }).unwrap_or(\"-\").upper())
  print(s_().filter(fn(x: string) -> bool { return x.len() > 5 }).unwrap_or(\"-\").upper())
  print(n_().filter(fn(x: string) -> bool { return x.len() > 1 }).unwrap_or(\"-\").upper())
}" 'AB
-
-'
both "Option<float>.filter" "$OF
fn main() {
  print(s_().filter(fn(x: float) -> bool { return x > 1.0 }).unwrap_or(9.5) + 0.25)
  print(s_().filter(fn(x: float) -> bool { return x > 9.0 }).unwrap_or(9.5) + 0.25)
  print(n_().filter(fn(x: float) -> bool { return x > 1.0 }).unwrap_or(9.5) + 0.25)
}" '1.75
9.75
9.75'
both "Option<bool>.filter" "$OB
fn main() {
  print(s_().filter(fn(x: bool) -> bool { return x }).unwrap_or(false))
  print(s_().filter(fn(x: bool) -> bool { return not x }).unwrap_or(false))
  print(n_().filter(fn(x: bool) -> bool { return x }).unwrap_or(false))
}" 'true
false
false'

echo "-- Option.expect: yields the payload, and the type is the payload's"
both "Option<int>.expect" "$OI
fn main() { print(s_().expect(\"need it\") + 1) }" '5'
both "Option<string>.expect" "$OS
fn main() { print(s_().expect(\"need it\").upper()) }" 'AB'
both "Option<float>.expect" "$OF
fn main() { print(s_().expect(\"need it\") + 0.25) }" '1.75'
both "Option<bool>.expect" "$OB
fn main() { print(s_().expect(\"need it\")) }" 'true'

echo "-- Option.or_else: None -> f(), a value passes straight through"
both "Option<int>.or_else" "$OI
fn fb() -> int? { return Some(7) }
fn main() {
  print(s_().or_else(fb).unwrap_or(-1))
  print(n_().or_else(fb).unwrap_or(-1))
}" '4
7'
both "Option<string>.or_else" "$OS
fn fb() -> string? { return Some(\"zz\") }
fn main() {
  print(s_().or_else(fb).unwrap_or(\"-\").upper())
  print(n_().or_else(fb).unwrap_or(\"-\").upper())
}" 'AB
ZZ'
both "Option<float>.or_else" "$OF
fn fb() -> float? { return Some(2.5) }
fn main() {
  print(s_().or_else(fb).unwrap_or(9.5) + 0.25)
  print(n_().or_else(fb).unwrap_or(9.5) + 0.25)
}" '1.75
2.75'
both "Option<bool>.or_else" "$OB
fn fb() -> bool? { return Some(false) }
fn main() {
  print(s_().or_else(fb).unwrap_or(true))
  print(n_().or_else(fb).unwrap_or(true))
}" 'true
false'

# ---------------------------------------------------------------------------
# Result, per payload. Same structure; every arm exercises the Ok path AND the Err path.
# ---------------------------------------------------------------------------
RI='fn k_() -> Result<int, string> { return Ok(4) }
fn e_() -> Result<int, string> { return Err("boom") }'
RS='fn k_() -> Result<string, string> { return Ok("ab") }
fn e_() -> Result<string, string> { return Err("boom") }'
RF='fn k_() -> Result<float, string> { return Ok(1.5) }
fn e_() -> Result<float, string> { return Err("boom") }'
RB='fn k_() -> Result<bool, string> { return Ok(true) }
fn e_() -> Result<bool, string> { return Err("boom") }'

echo "-- Result.map: Ok(x) -> Ok(f(x)), Err survives UNCHANGED"
both "Result<int>.map same payload" "$RI
fn main() {
  print(k_().map(fn(x: int) -> int { return x * 3 }).unwrap_or(-1))
  print(e_().map(fn(x: int) -> int { return x * 3 }).unwrap_or(-1))
  print(e_().map(fn(x: int) -> int { return x * 3 }).unwrap_err())
}" '12
-1
boom'
both "Result<string>.map same payload" "$RS
fn main() {
  print(k_().map(fn(x: string) -> string { return x.upper() }).unwrap_or(\"-\"))
  print(e_().map(fn(x: string) -> string { return x.upper() }).unwrap_err())
}" 'AB
boom'
both "Result<float>.map same payload" "$RF
fn main() {
  print(k_().map(fn(x: float) -> float { return x + 0.25 }).unwrap_or(9.5))
  print(e_().map(fn(x: float) -> float { return x + 0.25 }).unwrap_or(9.5))
}" '1.75
9.5'
both "Result<bool>.map same payload" "$RB
fn main() {
  print(k_().map(fn(x: bool) -> bool { return not x }).unwrap_or(true))
  print(e_().map(fn(x: bool) -> bool { return not x }).unwrap_or(true))
}" 'false
true'

echo "-- Result.map CHANGES the family, and carries the error across the change"
both "Result<int>.map -> Result<string>" "$RI
fn main() {
  print(k_().map(fn(x: int) -> string { return \"i\${x}\" }).unwrap_or(\"-\").upper())
  print(e_().map(fn(x: int) -> string { return \"i\${x}\" }).unwrap_err().upper())
}" 'I4
BOOM'
both "Result<string>.map -> Result<int>" "$RS
fn main() {
  print(k_().map(fn(x: string) -> int { return x.len() }).unwrap_or(-1) + 1)
  print(e_().map(fn(x: string) -> int { return x.len() }).unwrap_or(-1) + 1)
}" '3
0'
both "Result<int>.map -> Result<float>" "$RI
fn main() {
  print(k_().map(fn(x: int) -> float { return x.to_float() }).unwrap_or(9.5) + 0.25)
  print(e_().map(fn(x: int) -> float { return x.to_float() }).unwrap_or(9.5) + 0.25)
}" '4.25
9.75'
both "Result<bool>.map -> Result<int>" "$RB
fn main() {
  print(k_().map(fn(x: bool) -> int { if x { return 1 } return 0 }).unwrap_or(-1))
  print(e_().map(fn(x: bool) -> int { if x { return 1 } return 0 }).unwrap_or(-1))
}" '1
-1'
both "Result<float>.map -> Result<bool>" "$RF
fn big(x: float) -> bool { return x > 1.0 }
fn main() {
  print(k_().map(big).unwrap_or(false))
  print(e_().map(big).unwrap_or(false))
}" 'true
false'

echo "-- Result.and_then: flattened, and an Err from either side is reported"
both "Result<int>.and_then same family" "$RI
fn dbl(x: int) -> Result<int, string> { return Ok(x + x) }
fn main() {
  print(k_().and_then(dbl).unwrap_or(-1))
  print(e_().and_then(dbl).unwrap_err())
}" '8
boom'
both "Result<int>.and_then can answer Err" "$RI
fn refuse(x: int) -> Result<int, string> { return Err(\"inner\") }
fn main() {
  print(k_().and_then(refuse).unwrap_err())
  print(e_().and_then(refuse).unwrap_err())
}" 'inner
boom'
both "Result<string>.and_then -> Result<int>" "$RS
fn sz(x: string) -> Result<int, string> { return Ok(x.len()) }
fn main() {
  print(k_().and_then(sz).unwrap_or(-1) + 1)
  print(e_().and_then(sz).unwrap_err())
}" '3
boom'
both "Result<float>.and_then -> Result<string>" "$RF
fn fs(x: float) -> Result<string, string> { return Ok(\"f\") }
fn main() {
  print(k_().and_then(fs).unwrap_or(\"-\").upper())
  print(e_().and_then(fs).unwrap_err().upper())
}" 'F
BOOM'
both "Result<bool>.and_then -> Result<bool>" "$RB
fn flip(x: bool) -> Result<bool, string> { return Ok(not x) }
fn main() {
  print(k_().and_then(flip).unwrap_or(true))
  print(e_().and_then(flip).unwrap_or(true))
}" 'false
true'

echo "-- Result.map_err: Err(e) -> Err(f(e)), Ok untouched, family unchanged"
both "Result<int>.map_err" "$RI
fn shout(e: string) -> string { return e.upper() }
fn main() {
  print(e_().map_err(shout).unwrap_err())
  print(k_().map_err(shout).unwrap_or(-1))
}" 'BOOM
4'
both "Result<string>.map_err" "$RS
fn tag(e: string) -> string { return \"E:\" + e }
fn main() {
  print(e_().map_err(tag).unwrap_err())
  print(k_().map_err(tag).unwrap_or(\"-\").upper())
}" 'E:boom
AB'
both "Result<float>.map_err" "$RF
fn shout(e: string) -> string { return e.upper() }
fn main() {
  print(e_().map_err(shout).unwrap_err())
  print(k_().map_err(shout).unwrap_or(9.5) + 0.25)
}" 'BOOM
1.75'
both "Result<bool>.map_err" "$RB
fn shout(e: string) -> string { return e.upper() }
fn main() {
  print(e_().map_err(shout).unwrap_err())
  print(k_().map_err(shout).unwrap_or(false))
}" 'BOOM
true'

echo "-- Result.expect: yields the Ok payload, typed as that payload"
both "Result<int>.expect" "$RI
fn main() { print(k_().expect(\"need it\") + 1) }" '5'
both "Result<string>.expect" "$RS
fn main() { print(k_().expect(\"need it\").upper()) }" 'AB'
both "Result<float>.expect" "$RF
fn main() { print(k_().expect(\"need it\") + 0.25) }" '1.75'
both "Result<bool>.expect" "$RB
fn main() { print(k_().expect(\"need it\")) }" 'true'

echo "-- Result.or_else: Err -> f(), an Ok passes straight through"
both "Result<int>.or_else" "$RI
fn fb() -> Result<int, string> { return Ok(7) }
fn main() {
  print(k_().or_else(fb).unwrap_or(-1))
  print(e_().or_else(fb).unwrap_or(-1))
}" '4
7'
both "Result<string>.or_else" "$RS
fn fb() -> Result<string, string> { return Ok(\"zz\") }
fn main() {
  print(k_().or_else(fb).unwrap_or(\"-\").upper())
  print(e_().or_else(fb).unwrap_or(\"-\").upper())
}" 'AB
ZZ'
both "Result<float>.or_else" "$RF
fn fb() -> Result<float, string> { return Ok(2.5) }
fn main() {
  print(k_().or_else(fb).unwrap_or(9.5) + 0.25)
  print(e_().or_else(fb).unwrap_or(9.5) + 0.25)
}" '1.75
2.75'
both "Result<bool>.or_else" "$RB
fn fb() -> Result<bool, string> { return Ok(false) }
fn main() {
  print(k_().or_else(fb).unwrap_or(true))
  print(e_().or_else(fb).unwrap_or(true))
}" 'true
false'

# ---------------------------------------------------------------------------
# expect() on an EMPTY value: the CALLER's message, and a failing exit status.
# ---------------------------------------------------------------------------
echo "-- expect() panics with the caller's message"
expect_panic() {   # $1 label  $2 program  $3 expected stderr line
    local label="$1" mode out code
    printf '%s\n' "$2" > "$TMP/x.wyn"
    for mode in "" "--release"; do
        rm -f "$TMP/x" "$TMP/x.wyn.c"
        if ! (cd "$TMP" && perl -e 'alarm(180); exec @ARGV' -- "$WYN" build $mode "$TMP/x.wyn") \
             > "$TMP/b.log" 2>&1; then
            bad "$label (${mode:-dev}) builds"; continue
        fi
        out=$(perl -e 'alarm(60); exec @ARGV' -- "$TMP/x" 2>&1); code=$?
        if [ $code -ne 0 ] && [ "$out" = "$3" ]; then ok "$label (${mode:-dev})"
        else bad "$label (${mode:-dev}): code=$code out=[$out] want=[$3]"; fi
    done
}
expect_panic "Option.expect on None" "$OI
fn main() { print(n_().expect(\"value must be present\")) }" 'value must be present'
expect_panic "Result.expect on Err" "$RS
fn main() { print(e_().expect(\"config must load\")) }" 'config must load'
# A `%` in the message must print LITERALLY. The lowering passes the message through
# "%s" rather than using it as the format string, so it cannot read the stack; written
# this way because `fprintf(stderr, msg)` is the obvious spelling and is wrong.
expect_panic "expect message containing % prints literally" "$OI
fn main() { print(n_().expect(\"100% required: %s %d %n\")) }" '100% required: %s %d %n'

# ---------------------------------------------------------------------------
# Chaining, and the SLIM header.
# ---------------------------------------------------------------------------
echo "-- chained combinators"
both "map.map.filter.unwrap_or chain" "$OI
fn main() {
  print(s_().map(fn(x: int) -> int { return x * 3 }).map(fn(y: int) -> string { return \"y\${y}\" }).filter(fn(s: string) -> bool { return s.len() > 1 }).unwrap_or(\"-\").upper())
}" 'Y12'
both "and_then then map_err on a Result" "$RI
fn refuse(x: int) -> Result<int, string> { return Err(\"inner\") }
fn shout(e: string) -> string { return e.upper() }
fn main() { print(k_().and_then(refuse).map_err(shout).unwrap_err()) }" 'INNER'
both "a combinator result stored in a variable" "$OI
fn main() {
  o = s_().map(fn(x: int) -> int { return x + 1 })
  print(o)
  print(o.unwrap_or(-1))
}" 'Some(5)
5'

# `wyn build --release` deliberately keeps the FULL runtime header, so none of the arms
# above compile wyn_runtime_slim.h at all. `wyn run --release` is the one command that
# emits it, so it is the only way a missing slim declaration becomes a compile error.
echo "-- the SLIM header, actually compiled (wyn run --release)"
cat > "$TMP/slim.wyn" <<'WYN'
fn s_() -> string? { return Some("ab") }
fn e_() -> Result<int, string> { return Err("boom") }
fn shout(x: string) -> string { return x.upper() }
fn main() {
  print(s_().map(shout).unwrap_or("-"))
  print(e_().map_err(shout).unwrap_err())
  print(s_().expect("here"))
}
WYN
out=$(cd "$TMP" && perl -e 'alarm(240); exec @ARGV' -- "$WYN" run --release "$TMP/slim.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q "^AB$" && echo "$out" | grep -q "^BOOM$"; then
    ok "combinators compile against the SLIM header (wyn run --release)"
else
    bad "combinators against the slim header (rc=$rc) [$(echo "$out" | tr '\n' '|' | cut -c1-140)]"
fi

# ---------------------------------------------------------------------------
# What is still REFUSED, and refused with a reason. A combinator API that silently
# accepts a shape it cannot lower is how the original defect looked.
# ---------------------------------------------------------------------------
echo "-- refused, with the reason named"
reject "map_err on an Option" 'fn g() -> int? { return Some(1) }
fn main() { print(g().map_err(fn(e: string) -> string { return e })) }' \
        "Option does not have 'map_err()'"
reject "filter on a Result" 'fn k() -> Result<int, string> { return Ok(1) }
fn main() { print(k().filter(fn(x: int) -> bool { return true })) }' \
        "Result does not have 'filter()'"
reject "and_then returning a plain value" 'fn g() -> int? { return Some(1) }
fn main() { print(g().and_then(fn(x: int) -> int { return x })) }' \
        "needs a function returning an Option"
reject "or_else returning a different family" 'fn g() -> int? { return Some(1) }
fn fb() -> string? { return Some("z") }
fn main() { print(g().or_else(fb)) }' \
        "must return the same Option type"
reject "map_err returning a non-string" 'fn k() -> Result<int, string> { return Ok(1) }
fn main() { print(k().map_err(fn(e: string) -> int { return 1 })) }' \
        "must return a string"
reject "expect given a non-string message" 'fn g() -> int? { return Some(1) }
fn main() { print(g().expect(7)) }' \
        "takes a message string"
# A struct payload has a per-program family rather than one of the four builtin ones, so
# it is refused rather than typed as something codegen cannot emit. Stated in the message.
reject "map on a struct payload" 'struct P { x: int }
fn gp() -> P? { return Some(P { x: 1 }) }
fn main() { print(gp().map(fn(p: P) -> int { return p.x })) }' \
        "scalar payload"

# The receiver is recognised by its MONOMORPHIC FAMILY NAME ("OptionInt"), so a USER
# struct whose name merely starts with Option/Result and which really defines one of
# these methods must keep working. This arm is what fails if that guard is dropped.
echo "-- a user struct named Option*/Result* keeps its own methods"
both "user struct with its own map/filter" 'struct ResultSet {
  n: int
  fn map(self) -> int { return self.n * 2 }
}
struct OptionalBag {
  n: int
  fn filter(self) -> int { return self.n }
}
fn main() {
  r = ResultSet { n: 3 }
  print(r.map())
  b = OptionalBag { n: 5 }
  print(b.filter())
}' '6
5'
# `xs.map(f)` / `xs.filter(p)` on an ARRAY share these names and must be untouched: the
# var-decl type decision used to pick WynArray from the method NAME alone.
both "array map/filter still lower as arrays" 'fn main() {
  xs = [1, 2, 3]
  ys = xs.map(fn(x: int) -> int { return x * 2 })
  print(ys)
  print(xs.filter(fn(x: int) -> bool { return x > 1 }))
}' '[2, 4, 6]
[2, 3]'

# ---------------------------------------------------------------------------
# A PRE-EXISTING gap, pinned here so it is not mistaken for this feature's bug.
#
# `LambdaExpr` (src/types.h) has no declared-return-type field: the parser drops the
# `-> bool` in `fn(x: float) -> bool { .. }`, so the checker uses the BODY's inferred type
# (src/checker.c, `lambda_type->fn_type.return_type = body_type`) and a comparison body
# types as int. `map` reads the callback's return type to pick the result family, so a
# comparison-bodied LAMBDA yields the Int family and prints 1/0 where a NAMED function
# with the same signature yields the Bool family and prints true/false.
#
# It predates #392 and is not specific to it: the array path has the identical symptom on
# dev, `[1, 2].map(fn(x: int) -> bool { return x > 1 })` printing `[0, 1]`. Both spellings
# are asserted so that whoever gives LambdaExpr a return type sees BOTH change together
# and does not fix one while leaving the other.
# ---------------------------------------------------------------------------
echo "-- pre-existing: a lambda's declared '-> bool' is dropped by the parser"
both "comparison-bodied LAMBDA picks the Int family (pre-existing)" "$OF
fn main() { print(s_().map(fn(x: float) -> bool { return x > 1.0 }).unwrap_or(false)) }" '1'
both "the array path has the identical symptom (pre-existing)" 'fn main() {
  print([1, 2].map(fn(x: int) -> bool { return x > 1 }))
}' '[0, 1]'
both "a NAMED fn with the same signature picks the Bool family" "$OF
fn big(x: float) -> bool { return x > 1.0 }
fn main() { print(s_().map(big).unwrap_or(false)) }" 'true'

echo ""; echo "option-combinator-api: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
