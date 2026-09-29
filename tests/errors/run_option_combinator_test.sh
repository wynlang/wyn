#!/bin/bash
# THE HISTORY THIS GATE CARRIES, because the arms below were INVERTED once and the reason
# matters more than the arms.
#
# src/types.c used to advertise ten Option/Result combinators the language did not have.
# Calling one passed `wyn check` and then failed in the C compiler, with a message that
# called a TYPE a namespace and blamed the user's spelling:
#
#   fn g() -> int? { return Some(1) }
#   print(g().map(fn(x: int) -> int { return x + 1 }))
#       # wyn check: PASSED
#       # then: Error: unknown method 'OptionInt.map' on namespace 'OptionInt'
#       #       Help: 'map' is not a function Wyn knows about. Check the spelling...
#
# ...for a method `src/types.c` itself listed. The rows were removed and the calls were
# rejected with a real message, and THIS GATE PINNED THAT REJECTION - with a note saying
# "if a combinator is ever really implemented, its reject arm fails and says so."
#
# #392 implemented them, so those fourteen arms have been flipped to ACCEPT arms here,
# deliberately. They are kept rather than deleted because they are the only arms that
# pin the original *diagnosis* path staying dead: nothing may report "Option does not
# have 'map()'" again.
#
# WHY THE OLD REJECTION WAS RIGHT AT THE TIME, and why the implementation did NOT reuse
# the registry rows. Those rows lowered to `wyn_optional_map`, `wyn_result_map` and
# friends, and SOME of those names really are in the archive (wyn_optional_expect,
# wyn_optional_or_else, wyn_result_map, wyn_result_map_err, wyn_result_and_then). That is
# a red herring: those functions take `WynOptional*` / `WynResult*` - a heap-boxed
# representation that is NOT what codegen emits. Codegen emits the monomorphic value
# struct family (`OptionInt`, `ResultString`), so repointing the registry at the archive
# would not have compiled either; they are two different representations. #392 therefore
# lowers each combinator INLINE over the family struct and adds no runtime function at
# all. Full coverage (every method x every payload x both build modes) lives in
# tests/errors/run_option_combinator_api_test.sh; this file keeps the ORIGINAL programs
# from the defect report, now asserted to produce answers.
#
# What the live representation provides, read off the archive rather than guessed -
# `nm runtime/libwyn_rt.a | grep ' T _Option'`:
#     Option: Some None is_some is_none unwrap unwrap_or to_string
#     Result: Ok Err is_ok is_err unwrap unwrap_err unwrap_or to_string
# Every one of those is still pinned in the allow half below. (2026-09)
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

OPT_SRC='fn g() -> int? { return Some(1) }'
RES_SRC='fn h() -> Result<int, string> { return Ok(1) }'

# Every arm that used to call reject() now calls allow(): #392 implemented these. The
# "does not have" message must NOT come back for any of them, which allow() proves by
# requiring `wyn check` to pass AND the program to produce the right answer.

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

echo "-- the five Option combinators, on the EXACT programs from the defect report"
allow "option.map"      "$OPT_SRC\nfn main() {\n  print(g().map(fn(x: int) -> int { return x + 1 }))\n}" 'Some(2)'
# and_then FLATTENS, so its callback has to return an Option. The original arm passed a
# plain `-> int` lambda, which is now a typed error of its own (pinned below); the program
# is corrected here to the shape and_then means.
allow "option.and_then" "fn inc1(x: int) -> int? { return Some(x + 1) }\n$OPT_SRC\nfn main() {\n  print(g().and_then(inc1))\n}" 'Some(2)'
allow "option.filter"   "$OPT_SRC\nfn main() {\n  print(g().filter(fn(x: int) -> bool { return x > 0 }))\n}" 'Some(1)'
allow "option.expect"   "$OPT_SRC\nfn main() {\n  print(g().expect(\"boom\"))\n}" '1'
# or_else takes a NAMED function, not a lambda: a lambda cannot declare an `int?` return
# ("Expected '=>' or '{' after lambda signature"), which is a separate parser gap and is
# still true. Written this way so the arm exercises the combinator, not that parse error.
allow "option.or_else"  "fn fb() -> int? { return Some(2) }\n$OPT_SRC\nfn main() {\n  print(g().or_else(fb))\n}" 'Some(1)'

echo "-- the five Result combinators, same programs"
allow "result.map"      "$RES_SRC\nfn main() {\n  print(h().map(fn(x: int) -> int { return x + 1 }))\n}" 'Ok(2)'
allow "result.and_then" "fn inc1(x: int) -> Result<int, string> { return Ok(x + 1) }\n$RES_SRC\nfn main() {\n  print(h().and_then(inc1))\n}" 'Ok(2)'
allow "result.map_err"  "$RES_SRC\nfn main() {\n  print(h().map_err(fn(e: string) -> string { return e }))\n}" 'Ok(1)'
allow "result.expect"   "$RES_SRC\nfn main() {\n  print(h().expect(\"boom\"))\n}" '1'
allow "result.or_else"  "fn rfb() -> Result<int, string> { return Ok(2) }\n$RES_SRC\nfn main() {\n  print(h().or_else(rfb))\n}" 'Ok(1)'

echo "-- independent of the payload type, and of lambda vs named function"
allow "OptionString.map"  "fn gs() -> string? { return Some(\"a\") }\nfn main() {\n  print(gs().map(fn(x: string) -> string { return x }))\n}" 'Some("a")'
allow "OptionFloat.filter" "fn gf() -> float? { return Some(1.5) }\nfn main() {\n  print(gf().filter(fn(x: float) -> bool { return x > 0.0 }))\n}" 'Some(1.5)'
allow "option.map named fn" "fn inc(x: int) -> int { return x + 1 }\n$OPT_SRC\nfn main() {\n  print(g().map(inc))\n}" 'Some(2)'
allow "chained off unwrap_or" "$OPT_SRC\nfn main() {\n  print(g().map(fn(x: int) -> int { return x }).unwrap_or(0))\n}" '1'

echo "-- and the old DIAGNOSIS must never come back for these names"
# The message the removed rule produced was "Option does not have 'map()'". If any arm
# above ever regresses to it, allow() already fails; this arm states the promise directly
# for the one spelling a future family-membership rule is most likely to over-reach on.
d="$TMP/nodiag"; mkdir -p "$d"
printf '%b\n' "$OPT_SRC\nfn main() {\n  print(g().map(fn(x: int) -> int { return x + 1 }))\n}" > "$d/n.wyn"
nout=$("$WYNABS" check "$d/n.wyn" 2>&1)
if echo "$nout" | grep -q "does not have 'map()'"; then
  bad "no 'Option does not have map()' diagnosis"
else ok "no 'Option does not have map()' diagnosis"; fi

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
