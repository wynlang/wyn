#!/bin/bash
# A MAP LITERAL'S VALUE TYPE MUST COVER EVERY ENTRY, NOT JUST THE FIRST.
#
# `{"a": 1, "b": "x"}` printed `1` then `0` at exit 0, and `{"a": "x", "b": 1}`
# printed `x` then the empty string. The checker inferred the map's value type
# from elements[1] - the FIRST value - and its own comment said "a literal has
# homogeneous value types" while nothing enforced that. So `m["b"]` was read back
# through the wrong getter and invented a 0 / "".
#
# Separately `{"k": Some(1)}` and `{"k": Ok(1)}` passed `wyn check` clean and then
# died in the C compiler with `passing 'OptionInt' to parameter of incompatible
# type 'int'`, because the map-LITERAL codegen only ever chose a SCALAR insert
# (hashmap_insert_int/string/float/bool). The `m[k] = v` path next to it already
# knew to box an aggregate with hashmap_insert_struct - so `m["k"] = Some(1)`
# worked while `{"k": Some(1)}` did not. Two copies of one rule, one of them
# incomplete.
#
# ACCEPTANCE, per PLAN_v1.22 V-2: a mixed literal is a CLEAN CHECK-TIME error
# naming the conflicting types and the line; Option/Result values round-trip.
#
# Wyn has no union value type and the read side emits ONE getter for the whole
# map, so "correctly typed" is not available for a mixed literal - the honest
# outcome is the check-time error. A silent 0/"" is not acceptable either way.
#
# Every run arm asserts OUTPUT: these defects exited 0 with the wrong answer, so
# an exit-status-only test would be vacuous.
set -uo pipefail
WYN="${WYN:-./wyn}"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# $1=name, then N needles ; source on stdin. Must FAIL `wyn check` and the
# message must contain EVERY needle (both type names and the line number).
gate(){
  local name="$1"; shift
  local f="$TMP/$name.wyn"; cat > "$f"
  local out code missing=""
  out=$("$WYN" check "$f" 2>&1); code=$?
  if [ $code -eq 0 ]; then bad "$name: check PASSED, expected a clean error [$out]"; return; fi
  for n in "$@"; do echo "$out" | grep -qF "$n" || missing="$missing '$n'"; done
  if [ -n "$missing" ]; then bad "$name: message missing$missing -- got [$out]"
  else ok "$name: clean check error naming $*"; fi
}

# $1=name $2=expected-output (newlines as '|') ; source on stdin.
# Asserts wyn run, wyn build AND wyn run --release - three different paths.
#
# `--release` earns its place here rather than being boilerplate: it is the only
# command that compiles against src/wyn_runtime_slim.h, and hashmap_insert_struct /
# hashmap_index_struct were defined ONLY in wyn_runtime.h. So every aggregate-valued
# map was rejected under --release alone - including `m["k"] = Some(1)`, which
# worked under run and build. Flags go BEFORE the path for `run`.
runok(){
  local name="$1" want="$2" f="$TMP/$1.wyn" got
  cat > "$f"
  got=$("$WYN" run "$f" 2>/dev/null | tr '\n' '|')
  [ "$got" = "$want" ] && ok "$name/run → $want" || bad "$name/run: want [$want] got [$got]"
  got=$("$WYN" build "$f" -o "$TMP/$name.bin" >/dev/null 2>&1 && "$TMP/$name.bin" 2>/dev/null | tr '\n' '|')
  [ "$got" = "$want" ] && ok "$name/build → $want" || bad "$name/build: want [$want] got [$got]"
  got=$("$WYN" run --release "$f" 2>/dev/null | tr '\n' '|')
  [ "$got" = "$want" ] && ok "$name/release → $want" || bad "$name/release: want [$want] got [$got]"
  rm -f "$TMP/$name.bin" "$f.c" "$f.out"
}

echo "--- mixed value types are a clean CHECK-TIME error ---"

# The map literal is on line 2 in every gate program, so the reported line is
# asserted too - "names the conflicting types and the line".

gate mixed_int_string "int" "string" "line 2" <<'WYN'
fn main() {
    m = {"a": 1, "b": "x"}
    print(m["a"])
    print(m["b"])
}
WYN

gate mixed_string_int "string" "int" "line 2" <<'WYN'
fn main() {
    m = {"a": "x", "b": 1}
    print(m["a"])
    print(m["b"])
}
WYN

gate mixed_int_float "int" "float" "line 2" <<'WYN'
fn main() {
    m = {"a": 1, "b": 2.5}
    print(m["b"])
}
WYN

gate mixed_int_bool "int" "bool" "line 2" <<'WYN'
fn main() {
    m = {"a": 1, "b": true}
    print(m["b"])
}
WYN

# The conflict is in the THIRD entry: proves every entry is compared, not just
# elements[1] against elements[3].
gate third_entry_conflicts "int" "string" "line 2" <<'WYN'
fn main() {
    m = {"a": 1, "b": 2, "c": "x"}
    print(m["c"])
}
WYN

# Two DIFFERENT aggregates. Both are TYPE_STRUCT, so a kind-only comparison
# would wave this through and the read side would use one getter for two
# incompatible C types - the original silent-wrong-answer shape, one level up.
gate mixed_aggregates "OptionInt" "A" "line 3" <<'WYN'
struct A { b: int }
fn main() {
    m = {"k": Some(1), "j": A { b: 1 }}
    print(m["j"].b)
}
WYN

echo "--- Option / Result / struct values round-trip through a literal ---"

runok some_int '1|' <<'WYN'
fn main() {
    m = {"k": Some(1)}
    match m["k"] { Some(v) => print(v) None => print("none") }
}
WYN

runok ok_int '1|' <<'WYN'
fn main() {
    m = {"k": Ok(1)}
    match m["k"] { Ok(v) => print(v) Err(e) => print("err") }
}
WYN

# A Result whose payload is a STRING, so the box type is ResultString rather than
# ResultInt. Written through a typed function ON PURPOSE: a BARE `Err("bad")` as a
# literal value is still broken, but by a different root - the checker types a bare
# `Err(x)` as ResultInt (its Ok side defaults to int) while codegen names the family
# from the payload (ResultString), so the map's read side and the constructor
# disagree no matter which one the insert follows. That is the Option/Result family
# authority, not the map value-type authority, and it is logged rather than fixed
# here. It fails LOUDLY (C type error, exit 1), not silently.
runok err_string_typed 'bad|' <<'WYN'
fn mk() -> Result<int, string> { return Err("bad") }
fn main() {
    m = {"k": mk()}
    match m["k"] { Ok(v) => print("ok") Err(e) => print(e) }
}
WYN

runok some_string 's|' <<'WYN'
fn main() {
    m = {"k": Some("s")}
    match m["k"] { Some(v) => print(v) None => print("none") }
}
WYN

runok struct_value '7|' <<'WYN'
struct A { b: int }
fn main() {
    m = {"k": A { b: 7 }}
    print(m["k"].b)
}
WYN

# Nested Option: the box type is a monomorphic OptionOptionInt family.
runok nested_option '1|' <<'WYN'
fn main() {
    m = {"k": Some(Some(1))}
    match m["k"] { Some(v) => { match v { Some(w) => print(w) None => print("in") } } None => print("out") }
}
WYN

# Two entries of the SAME aggregate type must still be fine.
runok two_same_aggregates '1|2|' <<'WYN'
fn main() {
    m = {"a": Some(1), "b": Some(2)}
    match m["a"] { Some(v) => print(v) None => print("none") }
    match m["b"] { Some(v) => print(v) None => print("none") }
}
WYN

echo "--- homogeneous literals still work (no regression) ---"

runok homog_int '1|2|' <<'WYN'
fn main() { m = {"a": 1, "b": 2} print(m["a"]) print(m["b"]) }
WYN

runok homog_string 'x|y|' <<'WYN'
fn main() { m = {"a": "x", "b": "y"} print(m["a"]) print(m["b"]) }
WYN

runok homog_float '1.5|2.5|' <<'WYN'
fn main() { m = {"a": 1.5, "b": 2.5} print(m["a"]) print(m["b"]) }
WYN

runok single_entry '1|' <<'WYN'
fn main() { m = {"a": 1} print(m["a"]) }
WYN

echo "--- the m[k]=v / m.set() / iteration API is untouched ---"

# This path ALREADY boxed aggregates correctly and is the one the literal path
# now shares an authority with - assert it did not regress.
runok index_assign_aggregate '1|' <<'WYN'
fn main() {
    m = {}
    m["k"] = Some(1)
    match m["k"] { Some(v) => print(v) None => print("none") }
}
WYN

# m.set() went through the same incomplete scalar-only selector as the literal,
# so this was a C error too and is fixed by the same shared authority.
runok set_method_aggregate '1|' <<'WYN'
fn main() {
    m = {}
    m.set("k", Some(1))
    match m["k"] { Some(v) => print(v) None => print("none") }
}
WYN

runok empty_map_store_iter 'a=1|b=2|' <<'WYN'
fn main() {
    m = {}
    m["a"] = 1
    m["b"] = 2
    for k, v in m { print("${k}=${v}") }
}
WYN

runok index_assign_scalars '1|x|' <<'WYN'
fn main() {
    mi = {}
    mi["a"] = 1
    ms = {}
    ms["a"] = "x"
    print(mi["a"])
    print(ms["a"])
}
WYN

echo "--- the one shape still broken must stay LOUD, never silently wrong ---"

# A bare `Ok(1)` / `Some(1)` inside a function that RETURNS Option/Result<string,…>
# is STILL broken, by a different root: wyn_option_ctor_kind resolves a bare
# constructor's family from the enclosing function's RETURN KIND before the
# payload, so it emits `OptionString_Some(1)` for an int payload, while both the
# box and the read (correctly) say OptionInt. That is the Option/Result family
# precedence, not the map value-type authority, and it is logged not fixed.
#
# It is pinned here anyway, because the WRONG fix is attractive: naming the box
# from the constructor's authority instead of the checker's makes box and
# constructor agree, compiles, passes under `run` and `build` by struct-layout
# luck - and prints 0 under `--release`. That is a silent wrong answer where there
# had been a loud error, i.e. worse than the bug. This arm fails the day anyone
# reintroduces it.
loud(){  # $1=name $2=the answer that would mean "silently wrong" ; source on stdin
  local name="$1" wrong="$2" f="$TMP/$1.wyn" got code
  cat > "$f"
  for mode in "run" "run --release"; do
    got=$("$WYN" $mode "$f" 2>/dev/null | tr '\n' '|'); code=$?
    if [ "$got" = "$wrong" ]; then
      bad "$name/$mode: SILENTLY WRONG - printed [$got] at exit $code"
    elif [ $code -ne 0 ] || [ -z "$got" ]; then
      ok "$name/$mode: fails loudly (exit $code), not silently"
    else
      ok "$name/$mode: produced [$got]"
    fi
  done
  rm -f "$f.c" "$f.out"
}

loud some_in_option_fn '0|done|' <<'WYN'
fn f() -> Option<string> {
    m = {"k": Some(1)}
    match m["k"] { Some(v) => print(v) None => print("n") }
    return Some("done")
}
fn main() { match f() { Some(v) => print(v) None => print("N") } }
WYN

loud bare_err_in_literal '|' <<'WYN'
fn main() {
    m = {"k": Err("bad")}
    match m["k"] { Ok(v) => print("ok") Err(e) => print(e) }
}
WYN

echo ""; echo "map-literal-value-type: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
