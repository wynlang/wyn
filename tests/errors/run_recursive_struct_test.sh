#!/bin/bash
# Recursive struct fields are rejected AT CHECK TIME with a clear message.
# Regression guard: in v1.18.0 `struct Node { next: Node? }` passed `wyn check`
# and died later with a bare "internal codegen error" at build. (2026-07)
set -uo pipefail
WYN="${WYN:-./wyn}"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# direct self-reference → clean check-time error
printf 'struct Node { val: int, next: Node }\nfn main() -> int { return 0 }\n' > "$TMP/d.wyn"
out=$("$WYN" check "$TMP/d.wyn" 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "cannot contain a field of its own type"; then
  ok "direct recursive field: clean check error"
else bad "direct recursive: code=$code [$out]"; fi

# optional self-reference (Node?) → same clean error
printf 'struct Node { val: int, next: Node? }\nfn main() -> int { return 0 }\n' > "$TMP/o.wyn"
out=$("$WYN" check "$TMP/o.wyn" 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "cannot contain a field of its own type"; then
  ok "optional recursive field: clean check error"
else bad "optional recursive: code=$code [$out]"; fi

# mutually-recursive pair (A has B?, B has A?) → clean check-time error
printf 'struct A { val: int, b: B? }\nstruct B { val: int, a: A? }\nfn main() -> int { return 0 }\n' > "$TMP/m.wyn"
out=$("$WYN" check "$TMP/m.wyn" 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "recursive struct cycle"; then
  ok "mutually-recursive pair: clean check error"
else bad "mutual recursion: code=$code [$out]"; fi

# longer cycle (A -> B -> C -> A) → clean check-time error
printf 'struct A { b: B }\nstruct B { c: C }\nstruct C { a: A? }\nfn main() -> int { return 0 }\n' > "$TMP/c.wyn"
out=$("$WYN" check "$TMP/c.wyn" 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "recursive struct cycle"; then
  ok "3-struct cycle: clean check error"
else bad "3-struct cycle: code=$code [$out]"; fi

# array of own type ([Node]) is heap-indirected and stays legal
printf 'struct Node { val: int, kids: [Node] }\nfn main() {\n  var n = Node { val: 9, kids: [] }\n  println(n.val)\n}\n' > "$TMP/a.wyn"
out=$("$WYN" build "$TMP/a.wyn" >/dev/null 2>&1 && "$TMP/a" 2>&1; rm -f "$TMP/a" "$TMP/a.wyn.c")
[ "$out" = "9" ] && ok "array-of-own-type field still runs" || bad "array-of-own-type: [$out]"

# non-recursive struct-in-struct still compiles and runs
printf 'struct Point { x: int, y: int }\nstruct Line { a: Point, b: Point }\nfn main() {\n  var l = Line { a: Point { x: 0, y: 0 }, b: Point { x: 1, y: 7 } }\n  println(l.b.y)\n}\n' > "$TMP/n.wyn"
out=$("$WYN" build "$TMP/n.wyn" >/dev/null 2>&1 && "$TMP/n" 2>&1; rm -f "$TMP/n" "$TMP/n.wyn.c")
[ "$out" = "7" ] && ok "non-recursive struct-in-struct still runs" || bad "struct-in-struct: [$out]"

echo ""; echo "recursive-struct: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
