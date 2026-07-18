#!/bin/bash
# Unknown methods on string/array receivers are rejected AT CHECK TIME with a
# "did you mean" hint. Regression guard: in v1.18.0 `"hi".to_upper()` and
# `xs.frobnicate()` passed `wyn check` clean and died at C-compile with a bare
# "Unknown method" error. (2026-07)
set -uo pipefail
WYN="${WYN:-./wyn}"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# unknown string method → clean check error
printf 'fn main() { println("hi".frobnicate()) }\n' > "$TMP/s.wyn"
out=$("$WYN" check "$TMP/s.wyn" 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "no method 'frobnicate'"; then
  ok "unknown string method: check error"
else bad "unknown string method: code=$code [$out]"; fi

# typo gets a suggestion
printf 'fn main() { println("hi".uppr()) }\n' > "$TMP/t.wyn"
out=$("$WYN" check "$TMP/t.wyn" 2>&1)
if echo "$out" | grep -q "Did you mean:.*upper"; then
  ok "typo suggestion (.uppr -> .upper)"
else bad "typo suggestion: [$out]"; fi

# unknown array method → clean check error
printf 'fn main() {\n  var xs = [1,2,3]\n  println(xs.frobnicate())\n}\n' > "$TMP/a.wyn"
out=$("$WYN" check "$TMP/a.wyn" 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "no method 'frobnicate'"; then
  ok "unknown array method: check error"
else bad "unknown array method: code=$code [$out]"; fi

# newly-tabled methods still run end-to-end
printf 'fn main() {\n  var xs = [3,1,4]\n  println(xs.min())\n  println(xs.max())\n  println(xs.sum())\n}\n' > "$TMP/m.wyn"
out=$("$WYN" build "$TMP/m.wyn" >/dev/null 2>&1 && "$TMP/m" 2>&1; rm -f "$TMP/m" "$TMP/m.wyn.c")
if [ "$out" = "1
4
8" ]; then ok "array min/max/sum run"; else bad "min/max/sum: [$out]"; fi

echo ""; echo "unknown-method: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
