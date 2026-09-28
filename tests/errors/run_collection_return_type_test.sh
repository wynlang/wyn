#!/bin/bash
# A method the signature table declares as returning a COLLECTION must type as that
# collection, not as int.
#
#   a = {:"x"}
#   b = {:"y"}
#   print(a.union(b).len())    // Error: Unknown method 'len' for type 'int'
#
# types.c declares `{"set", "union", "set", 1}`, and the runtime has set_union /
# set_intersection / set_difference. What was missing sat between them: the checker's
# return-type chain mapped the strings "string", "int", "float", "bool", a capitalised
# NAMED type, "array", "json" and "void" - and neither "set" nor "map". So every row
# declaring one of those two fell through to the int default at the end of the chain, and
# the entire set-algebra half of the HashSet API returned a value nothing could be done
# with. Not a wrong answer - an unusable one, and only at the point of USE, which is why
# the error named `len` rather than `union`.
#
# The `map` half is fixed in the same place even though no caller reaches it today (the
# rows declaring `map` - filter_keys, map_values - are refused earlier as unknown methods
# on a map receiver, tracked separately). Leaving one of a pair out is how this recurs.
#
# Both modes, because a collection-typed result changes what codegen emits downstream and
# --release uses a different header. (2026-09)
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# expect <label> <program> <expected-stdout>
expect(){
  d="$TMP/c$PASS$FAIL"; mkdir -p "$d"
  printf '%b\n' "$2" > "$d/a.wyn"
  for mode in debug release; do
    if [ "$mode" = release ]; then got=$("$WYNABS" run --release "$d/a.wyn" 2>&1); else got=$("$WYNABS" run "$d/a.wyn" 2>&1); fi
    got=$(printf '%s' "$got" | sed 's/\x1b\[[0-9;]*m//g' | grep -vE 'Compiled in|^Warning|unused variable')
    if [ "$got" = "$3" ]; then ok "[$mode] $1"
    else bad "[$mode] $1 - got [$(echo "$got" | tr '\n' '|' | cut -c1-90)] want [$(echo "$3" | tr '\n' '|')]"; fi
  done
}

echo "-- a set-returning method returns a usable SET, not an int"
expect "union().len()"        'fn main() {\n  a = {:"x"}\n  b = {:"y"}\n  print(a.union(b).len())\n}' '2'
expect "intersection().len()" 'fn main() {\n  a = {:"x","y"}\n  b = {:"y"}\n  print(a.intersection(b).len())\n}' '1'
expect "difference().len()"   'fn main() {\n  a = {:"x","y"}\n  b = {:"y"}\n  print(a.difference(b).len())\n}' '1'

echo "-- and the result is a real set: membership, and chaining"
expect "union() membership" 'fn main() {\n  a = {:"x"}\n  b = {:"y"}\n  c = a.union(b)\n  print(c.contains("x"))\n  print(c.contains("y"))\n  print(c.contains("z"))\n}' 'true
true
false'
expect "chained union"     'fn main() {\n  a = {:"x"}\n  b = {:"y"}\n  d = {:"z"}\n  print(a.union(b).union(d).len())\n}' '3'
expect "intersection is empty when disjoint" 'fn main() {\n  a = {:"x"}\n  b = {:"y"}\n  print(a.intersection(b).is_empty())\n}' 'true'

echo "-- the predicates that return bool are unaffected (control)"
expect "is_subset / is_disjoint" 'fn main() {\n  a = {:"x"}\n  b = {:"x","y"}\n  print(a.is_subset(b))\n  print(a.is_disjoint(b))\n}' 'true
false'

echo "-- the plain set API still works (control)"
expect "len / contains / is_empty" 'fn main() {\n  s = {:"a","b"}\n  print(s.len())\n  print(s.contains("a"))\n  print(s.is_empty())\n}' '2
true
false'

echo ""; echo "collection-return-type: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
