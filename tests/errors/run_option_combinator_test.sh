#!/bin/bash
# The registry advertises ten Option/Result combinators the language does not have.
# Calling one passed `wyn check` and then failed in the C compiler, with a message that
# called a TYPE a namespace and blamed the user's spelling:
#
#   fn g() -> int? { return Some(1) }
#   print(g().map(fn(x: int) -> int { return x + 1 }))
#       # wyn check: PASSED
#       # then: Error: unknown method 'OptionInt.map' on namespace 'OptionInt'
#       #       Help: 'map' is not a function Wyn knows about. Check the spelling...
#
# ...for a method `src/types.c` itself lists. That is the worst kind of wrong diagnosis:
# it sends the reader to check a name the compiler advertised.
#
# THE TEN: option {map, and_then, filter, expect, or_else}
#          result {map, map_err, and_then, expect, or_else}
#
# WHY NONE OF THEM IS "NEARLY WORKING". The registry lowers them to `wyn_optional_map`,
# `wyn_result_map` and friends, and SOME of those names really are in the archive
# (wyn_optional_expect, wyn_optional_or_else, wyn_result_map, wyn_result_map_err,
# wyn_result_and_then). That is a red herring, and checking it is what took the time:
# those functions take `WynOptional*` / `WynResult*` - a heap-boxed representation that
# is NOT what codegen emits. Codegen emits the monomorphic value struct family instead
# (`OptionInt_map`, `ResultInt_expect`), which is why the C compiler asks for a name
# nothing defines. The archive functions belong to a retired parallel model, the same way
# types.c's own comment describes a retired WynJson* pairs model. So wiring the registry
# to them would not compile either; they are two different representations.
#
# What the live representation actually provides, read off the archive rather than
# guessed - `nm runtime/libwyn_rt.a | grep ' T _Option'`:
#     Option: Some None is_some is_none unwrap unwrap_or to_string
#     Result: Ok Err is_ok is_err unwrap unwrap_err unwrap_or to_string
# Every one of those is pinned in the allow half below, because a rule that rejects the
# ten must not touch the eight. (2026-09)
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

OPT_SRC='fn g() -> int? { return Some(1) }'
RES_SRC='fn h() -> Result<int, string> { return Ok(1) }'

# reject <label> <program>
reject(){
  printf '%b\n' "$2" > "$TMP/r.wyn"
  out=$("$WYNABS" check "$TMP/r.wyn" 2>&1); code=$?
  if [ $code -ne 0 ] && echo "$out" | grep -q "does not have"; then
    ok "reject: $1"
  else bad "reject: $1 (code=$code) [$(echo "$out" | tr '\n' '|' | cut -c1-130)]"; fi
}

# allow <label> <program> <expected-stdout>
allow(){
  d="$TMP/a$PASS$FAIL"; mkdir -p "$d"
  printf '%b\n' "$2" > "$d/a.wyn"
  cout=$("$WYNABS" check "$d/a.wyn" 2>&1); ccode=$?
  if [ $ccode -ne 0 ]; then
    bad "allow: $1 - check rejected it [$(echo "$cout" | tr '\n' '|' | cut -c1-130)]"; return
  fi
  got=$("$WYNABS" run "$d/a.wyn" 2>&1 | grep -vE 'Compiled in|^Warning|unused variable')
  if [ "$got" = "$3" ]; then ok "allow: $1"
  else bad "allow: $1 - got [$(echo "$got" | tr '\n' '|')] want [$(echo "$3" | tr '\n' '|')]"; fi
}

echo "-- rejected: the five Option combinators the language does not have"
reject "option.map"      "$OPT_SRC\nfn main() {\n  print(g().map(fn(x: int) -> int { return x + 1 }))\n}"
reject "option.and_then" "$OPT_SRC\nfn main() {\n  print(g().and_then(fn(x: int) -> int { return x + 1 }))\n}"
reject "option.filter"   "$OPT_SRC\nfn main() {\n  print(g().filter(fn(x: int) -> bool { return x > 0 }))\n}"
reject "option.expect"   "$OPT_SRC\nfn main() {\n  print(g().expect(\"boom\"))\n}"
# or_else takes a NAMED function, not a lambda: a lambda cannot declare an `int?` return
# ("Expected '=>' or '{' after lambda signature"), which is a separate parser gap. Written
# this way so the arm exercises THIS rule rather than that parse error.
reject "option.or_else"  "fn fb() -> int? { return Some(2) }\n$OPT_SRC\nfn main() {\n  print(g().or_else(fb))\n}"

echo "-- rejected: the five Result combinators, same story"
reject "result.map"      "$RES_SRC\nfn main() {\n  print(h().map(fn(x: int) -> int { return x + 1 }))\n}"
reject "result.and_then" "$RES_SRC\nfn main() {\n  print(h().and_then(fn(x: int) -> int { return x + 1 }))\n}"
reject "result.map_err"  "$RES_SRC\nfn main() {\n  print(h().map_err(fn(e: string) -> string { return e }))\n}"
reject "result.expect"   "$RES_SRC\nfn main() {\n  print(h().expect(\"boom\"))\n}"
reject "result.or_else"  "fn rfb() -> Result<int, string> { return Ok(2) }\n$RES_SRC\nfn main() {\n  print(h().or_else(rfb))\n}"

echo "-- rejected: independent of the payload type, and of lambda vs named function"
reject "OptionString.map"  "fn gs() -> string? { return Some(\"a\") }\nfn main() {\n  print(gs().map(fn(x: string) -> string { return x }))\n}"
reject "OptionFloat.filter" "fn gf() -> float? { return Some(1.5) }\nfn main() {\n  print(gf().filter(fn(x: float) -> bool { return x > 0.0 }))\n}"
reject "option.map named fn" "fn inc(x: int) -> int { return x + 1 }\n$OPT_SRC\nfn main() {\n  print(g().map(inc))\n}"
reject "chained off unwrap_or" "$OPT_SRC\nfn main() {\n  print(g().map(fn(x: int) -> int { return x }).unwrap_or(0))\n}"

echo "-- allowed: everything the live representation really provides (read off the archive)"
allow "option.is_some / is_none" "$OPT_SRC\nfn main() {\n  print(g().is_some())\n  print(g().is_none())\n}" 'true
false'
allow "option.unwrap"    "$OPT_SRC\nfn main() {\n  print(g().unwrap())\n}" '1'
allow "option.unwrap_or" "$OPT_SRC\nfn main() {\n  print(g().unwrap_or(9))\n}" '1'
allow "option.to_string" "$OPT_SRC\nfn main() {\n  print(g().to_string())\n}" 'Some(1)'
allow "option None path"  'fn n() -> int? { return None }\nfn main() {\n  print(n().is_some())\n  print(n().unwrap_or(9))\n}' 'false
9'
allow "result.is_ok / is_err" "$RES_SRC\nfn main() {\n  print(h().is_ok())\n  print(h().is_err())\n}" 'true
false'
allow "result.unwrap"     "$RES_SRC\nfn main() {\n  print(h().unwrap())\n}" '1'
allow "result.unwrap_or"  "$RES_SRC\nfn main() {\n  print(h().unwrap_or(9))\n}" '1'
allow "result.to_string"  "$RES_SRC\nfn main() {\n  print(h().to_string())\n}" 'Ok(1)'
allow "result.unwrap_err" 'fn e() -> Result<int, string> { return Err("bad") }\nfn main() {\n  print(e().unwrap_err())\n}' 'bad'
allow "string payload family" 'fn gs() -> string? { return Some("a") }\nfn main() {\n  print(gs().unwrap_or("z"))\n  print(gs().is_some())\n}' 'a
true'
# The #372 carve-out: `m.get(k).unwrap_or(d)` has a real lowering of its own and must not
# be caught by a rule about Option methods.
allow "map.get().unwrap_or()" 'fn main() {\n  m = {"a": 1}\n  print(m.get("a").unwrap_or(9))\n  print(m.get("zz").unwrap_or(9))\n}' '1
9'
# The receiver is recognised by its MONOMORPHIC FAMILY NAME ("OptionInt", "ResultString"),
# so a USER struct whose name merely starts with Option/Result and which really defines one
# of these methods must keep working. The struct's own definition is asked before
# rejecting; this arm is what fails if that guard is ever dropped.
allow "user struct named Result*/Option* with such a method" 'struct ResultSet {\n  n: int\n  fn map(self) -> int { return self.n * 2 }\n}\nstruct OptionalBag {\n  n: int\n  fn filter(self) -> int { return self.n }\n}\nfn main() {\n  r = ResultSet { n: 3 }\n  print(r.map())\n  b = OptionalBag { n: 5 }\n  print(b.filter())\n}' '6
5'

echo ""; echo "option-combinator: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
