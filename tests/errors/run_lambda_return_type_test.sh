#!/bin/bash
# A lambda may declare any return type the language has - and the methods that requires
# must not crash.
#
#   f = fn() -> int? { return Some(1) }
#       # Error at line 1: Expected '=>' or '{' after lambda signature
#
# The lambda return-type annotation consumed exactly ONE identifier, so every type
# spelled with more than a bare name left tokens behind - and the next check blamed the
# BODY for a fault in the signature:
#
#   -> int?                 consumed `int`, left `?`
#   -> [int]                consumed nothing, `[` is not an identifier
#   -> Result<int, string>  consumed `Result`, left `<...`
#
# `-> int?` is the one that mattered most: a function returning an Option could not be
# written as a lambda at all.
#
# AND IT WAS HIDING A SEGFAULT. `a.flat_map(f)` takes a mapper returning an array, so the
# only lambda spelling for it was `-> [int]` - which did not parse. Reached through a NAMED
# function instead, flat_map segfaulted, on the SHIPPED v1.21.0 as well as on dev: the
# runtime took `long long (*fn)(long long)` and dereferenced the result as a `WynArray*`,
# while codegen emits a mapper returning WynArray BY VALUE. Struct-return and
# integer-return are different ABIs, so the "pointer" was whatever sat in the return
# register. `--release` printed a plausible answer, which was luck - both modes were
# undefined. Fixing the annotation without fixing that would have turned a parse error
# into a crash, so the arms below cover both.
#
# Both modes throughout, because the flat_map declaration lives in two headers. (2026-09)
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

expect(){   # <label> <program> <expected-stdout>
  d="$TMP/l$PASS$FAIL"; mkdir -p "$d"
  printf '%b\n' "$2" > "$d/a.wyn"
  for mode in debug release; do
    if [ "$mode" = release ]; then got=$("$WYNABS" run --release "$d/a.wyn" 2>&1); else got=$("$WYNABS" run "$d/a.wyn" 2>&1); fi
    got=$(printf '%s' "$got" | sed 's/\x1b\[[0-9;]*m//g' | grep -vE 'Compiled in|^Warning|unused variable')
    if [ "$got" = "$3" ]; then ok "[$mode] $1"
    else bad "[$mode] $1 - got [$(echo "$got" | tr '\n' '|' | cut -c1-90)] want [$(echo "$3" | tr '\n' '|')]"; fi
  done
}

echo "-- a lambda may declare every return-type shape"
expect "-> int? (an Option)"  'fn main() {\n  f = fn() -> int? { return Some(1) }\n  print(f().unwrap_or(0))\n}' '1'
expect "-> int? returning None" 'fn main() {\n  f = fn() -> int? { return None }\n  print(f().unwrap_or(9))\n}' '9'
expect "-> [int] (an array)"  'fn main() {\n  f = fn(x: int) -> [int] { return [x, x] }\n  print(f(2).len())\n}' '2'
expect "-> Result<int, string>" 'fn main() {\n  f = fn() -> Result<int, string> { return Ok(7) }\n  print(f().unwrap_or(0))\n}' '7'
expect "-> string? (string payload)" 'fn main() {\n  f = fn() -> string? { return Some("a") }\n  print(f().unwrap_or("z"))\n}' 'a'

echo "-- the plain shapes still parse (control: the one-identifier case)"
expect "-> int"    'fn main() {\n  f = fn(x: int) -> int { return x * 2 }\n  print(f(3))\n}' '6'
expect "-> bool"   'fn main() {\n  a = [1, 2, 3]\n  print(a.any(fn(x: int) -> bool { return x > 2 }))\n}' 'true'
expect "-> string" 'fn main() {\n  f = fn(x: string) -> string { return x }\n  print(f("q"))\n}' 'q'
expect "no annotation, => form" 'fn main() {\n  f = fn(x: int) => x + 1\n  print(f(1))\n}' '2'

echo "-- flat_map: the method that annotation unblocked, and the segfault behind it"
expect "flat_map with a lambda" 'fn main() {\n  a = [1, 2]\n  b = a.flat_map(fn(x: int) -> [int] { return [x, x] })\n  print(b.len())\n}' '4'
# The spelling that ALREADY parsed before the annotation fix, and segfaulted on v1.21.0.
expect "flat_map with a named fn" 'fn dup(x: int) -> [int] { return [x, x] }\nfn main() {\n  a = [1, 2]\n  b = a.flat_map(dup)\n  print(b.len())\n}' '4'
expect "flat_map contents are right, not just the length" 'fn dup(x: int) -> [int] { return [x, x + 10] }\nfn main() {\n  a = [1, 2]\n  b = a.flat_map(dup)\n  print(b[0])\n  print(b[1])\n  print(b[2])\n  print(b[3])\n}' '1
11
2
12'
expect "flat_map over an empty array" 'fn dup(x: int) -> [int] { return [x] }\nfn main() {\n  a = []\n  b = a.flat_map(dup)\n  print(b.len())\n}' '0'

echo ""; echo "lambda-return-type: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
