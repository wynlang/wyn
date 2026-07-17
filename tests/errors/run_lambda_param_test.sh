#!/bin/bash
# Lambda param-type inference (S1): int-param lambdas + .map/.filter/.reduce over ints
# still work; a string-param lambda gives a CLEAN "not supported yet" error (not a
# cryptic type mismatch, and not a silent miscompile). (2026-07)
set -uo pipefail
WYN="${WYN:-./wyn}"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# int lambda runs
printf 'fn main() {\n  var f = (n) => n * 2\n  println(f(21))\n}\n' > "$TMP/i.wyn"
out=$("$WYN" build "$TMP/i.wyn" >/dev/null 2>&1 && "$TMP/i" 2>&1; rm -f "$TMP/i" "$TMP/i.wyn.c")
[ "$out" = "42" ] && ok "int lambda runs" || bad "int lambda: [$out]"

# .map/.filter over ints run
printf 'fn main() {\n  var xs=[1,2,3,4]\n  println(xs.map((n) => n*10))\n  println(xs.filter((n) => n>2))\n}\n' > "$TMP/m.wyn"
out=$("$WYN" build "$TMP/m.wyn" >/dev/null 2>&1 && "$TMP/m" 2>&1; rm -f "$TMP/m" "$TMP/m.wyn.c")
echo "$out" | grep -q "10, 20, 30, 40" && ok "int .map runs" || bad "int .map: [$out]"

# string-param lambda → clean limitation error (exit non-zero, helpful message)
printf 'fn main() {\n  var f = (s) => s + "!"\n  println(f("hi"))\n}\n' > "$TMP/s.wyn"
out=$("$WYN" check "$TMP/s.wyn" 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "not supported yet"; then ok "string lambda: clean error"; else bad "string lambda err: code=$code [$out]"; fi

echo ""; echo "lambda-params: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
