#!/bin/bash
# Two calls that passed `wyn check` and then died in the C compiler now say what is wrong.
# Both are the release's exit criterion - if it checks, it must build - and both are
# narrowed so they cannot reject anything that builds.
#
# 1. `::` ON A VALUE
#
#   s = {:"a"}
#   s::add("b")      # undeclared function 's_add'
#
# The `::` lowering joins the qualifier and the method into one C symbol, so a local
# variable was treated as a namespace. It CANNOT simply be lowered like `.`, because `::`
# on a MODULE is the common correct case - `tui::display_width(s)` and friends appear in
# 110 files in this tree - so the module tests have to pass first, and only then is the
# qualifier known to be a value.
#
# Gated on the receiver's type NOT being int, and that is not fussiness: an `import`
# registers the module name as an int-typed placeholder symbol, so an int-typed qualifier
# is indistinguishable from a module here. `x::foo()` on an int variable therefore still
# reaches the C compiler - pre-existing, and pinned as an allow arm below so the limit is
# recorded rather than discovered.
#
# 2. `unwrap_or` ACROSS A VARIABLE
#
#   o = m.get("a")
#   print(o.unwrap_or(9))     # internal codegen error
#
# `map.get(k)` does not return an Option: it returns the value, and the zero value for a
# missing key. `m.get(k).unwrap_or(d)` works only because the whole chain is lowered as one
# operation - breaking it across a variable loses that lowering.
#
# V-28 deliberately left `unwrap_or` out of its set precisely because the inline chain
# builds. This rule keys on the receiver being an IDENTIFIER, which is what separates them:
# the inline chain's receiver is a CALL. Measured before it was written: that split chain on
# an int-valued map is the ONLY remaining broken shape - a string-valued map already gives a
# clean "string has no method 'unwrap_or'", and a real Option or Result in a variable works,
# because those type as a STRUCT (OptionInt) rather than as a scalar. All three are allow
# arms. (2026-09)
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

reject(){   # <label> <program> <substring the message must contain>
  printf '%b\n' "$2" > "$TMP/r.wyn"
  out=$("$WYNABS" check "$TMP/r.wyn" 2>&1); code=$?
  clean=$(printf '%s' "$out" | sed 's/\x1b\[[0-9;]*m//g')
  if [ $code -ne 0 ] && printf '%s' "$clean" | grep -q "$3"; then ok "reject: $1"
  else bad "reject: $1 (code=$code) [$(echo "$clean" | tr '\n' '|' | cut -c1-110)]"; fi
}
allow(){    # <label> <program> <expected-stdout>
  d="$TMP/a$PASS$FAIL"; mkdir -p "$d"
  printf '%b\n' "$2" > "$d/a.wyn"
  cout=$("$WYNABS" check "$d/a.wyn" 2>&1); if [ $? -ne 0 ]; then
    bad "allow: $1 - check rejected it [$(printf '%s' "$cout" | sed 's/\x1b\[[0-9;]*m//g' | tr '\n' '|' | cut -c1-110)]"; return; fi
  got=$("$WYNABS" run "$d/a.wyn" 2>&1 | sed 's/\x1b\[[0-9;]*m//g' | grep -vE 'Compiled in|^Warning|unused variable')
  if [ "$got" = "$3" ]; then ok "allow: $1"
  else bad "allow: $1 - got [$(echo "$got" | tr '\n' '|' | cut -c1-80)] want [$3]"; fi
}

echo "-- rejected: '::' used on a value, for every collection and value type"
reject "set"    'fn main() {\n  s = {:"a"}\n  s::add("b")\n  print(s.len())\n}'        "use '.' to call a method on a value"
reject "string" 'fn main() {\n  t = "ab"\n  print(t::upper())\n}'                      "use '.' to call a method on a value"
reject "array"  'fn main() {\n  a = [1, 2]\n  print(a::len())\n}'                      "use '.' to call a method on a value"
reject "map"    'fn main() {\n  m = {"a": 1}\n  print(m::len())\n}'                    "use '.' to call a method on a value"
reject "struct" 'struct P { x: int }\nfn main() {\n  p = P { x: 1 }\n  print(p::x())\n}' "use '.' to call a method on a value"
# The message must name the dot form, not merely complain.
reject "names the fix" 'fn main() {\n  s = {:"a"}\n  s::add("b")\n  print(s.len())\n}' "s.add()"

echo "-- allowed: '::' on a MODULE is the common correct case and must not move"
# A REAL imported module called with `::` - the shape the rejection above must never
# touch, and the one that appears in 110 files in this tree (tui::display_width, ...).
# Written as a two-file fixture because that is the only way to exercise it honestly.
modcase(){
  d="$TMP/mod$PASS$FAIL"; mkdir -p "$d"
  printf 'pub fn double(n: int) -> int {\n    return n * 2\n}\n' > "$d/helper.wyn"
  printf 'import helper\nfn main() {\n    print(helper::double(21))\n}\n' > "$d/main.wyn"
  got=$(cd "$d" && "$WYNABS" run main.wyn 2>&1 | sed 's/\x1b\[[0-9;]*m//g' | grep -vE 'Compiled in|^Warning|unused variable')
  if [ "$got" = "42" ]; then ok "allow: module-qualified :: call (imported module)"
  else bad "allow: module-qualified :: call - got [$(echo "$got" | tr '\n' '|' | cut -c1-90)] want [42]"; fi
}
modcase
allow "namespace :: call"  'fn main() {\n  print(Math::abs(0 - 3))\n}' '3'
# A STATIC FUNCTION ON A TYPE. `fn User.default()` called as `User::default()` is
# documented (book ch.11) and worked before this rule - which rejected it, because a struct
# name resolves to a symbol here exactly as a variable does. Caught only by running the
# BOOK's snippets against the packaged artifact: the 12,106-file corpus sweep cannot see
# them, because they live in markdown rather than in .wyn files. That is the arm.
allow "static fn on a struct type" 'struct User {\n  name: string,\n  age: int\n}\nfn User.default() -> User {\n  return User { name: "Guest", age: 0 }\n}\nfn main() {\n  u = User::default()\n  print(u.name)\n}' 'Guest'
allow "enum :: variant"    'enum Color { Red, Green }\nfn main() {\n  c = Color::Red\n  match c {\n    Color::Red => print("red"),\n    Color::Green => print("green")\n  }\n}' 'red'
# The documented limit: an int-typed qualifier cannot be told apart from an imported
# module name, so this shape is NOT rejected. Recorded so the boundary is deliberate.
allow "int-typed qualifier is left alone" 'fn main() {\n  x = 5\n  print(x.to_string())\n}' '5'

echo "-- rejected: unwrap_or across a variable (the one broken shape)"
reject "int map, split chain" 'fn main() {\n  m = {"a": 1}\n  o = m.get("a")\n  print(o.unwrap_or(9))\n}' "needs an Option or Result receiver"
reject "message names map.get" 'fn main() {\n  m = {"a": 1}\n  o = m.get("a")\n  print(o.unwrap_or(9))\n}' "does not return an Option"
reject "message names contains" 'fn main() {\n  m = {"a": 1}\n  o = m.get("a")\n  print(o.unwrap_or(9))\n}' "m.contains(k)"

echo "-- allowed: everything about unwrap_or that already built"
allow "inline chain, present key" 'fn main() {\n  m = {"a": 1}\n  print(m.get("a").unwrap_or(9))\n}' '1'
allow "inline chain, missing key" 'fn main() {\n  m = {"a": 1}\n  print(m.get("zz").unwrap_or(9))\n}' '9'
allow "a real Option in a variable" 'fn g() -> int? { return Some(1) }\nfn main() {\n  o = g()\n  print(o.unwrap_or(9))\n}' '1'
allow "a real Option, None, in a variable" 'fn n() -> int? { return None }\nfn main() {\n  o = n()\n  print(o.unwrap_or(9))\n}' '9'
allow "a real Result in a variable" 'fn h() -> Result<int, string> { return Ok(2) }\nfn main() {\n  r = h()\n  print(r.unwrap_or(9))\n}' '2'
allow "the remedy the message names" 'fn main() {\n  m = {"a": 1}\n  if m.contains("a") { print(m.get("a")) } else { print("absent") }\n}' '1'

echo ""; echo "value-call-diagnostics: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
