#!/bin/bash
# Diagnostic-location + panic-path batch (fix batch 2026-07):
#   - Function-arg type/arity errors must carry a `--> file:line` caret like
#     every other diagnostic (was a bare "validation failed" line).
#   - Runtime panics must NOT leak the generated `.wyn.c` path; they point at
#     the user's `.wyn` source (or at least never the .c seam).
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# 1. Arg TYPE mismatch: error output must contain `-->` and the call's line (5).
printf 'fn add(a: int, b: int) -> int {\n  return a + b\n}\nfn main() {\n  x = add("hello", 3)\n  println("${x}")\n}\n' > "$TMP/type.wyn"
out=$(perl -e 'alarm(15); exec @ARGV' -- "$WYN" check "$TMP/type.wyn" 2>&1); rc=$?
if [ $rc -ge 1 ] && [ $rc -le 127 ] && echo "$out" | grep -q -- '-->' && echo "$out" | grep -q ':5'; then
    ok "arg type mismatch has --> file:line"
else
    bad "arg type mismatch has --> file:line (rc=$rc) [$(echo "$out" | head -2 | tr '\n' ' ')]"
fi

# 2. Arg COUNT mismatch: error output must also contain `-->` and the line.
printf 'fn add(a: int, b: int) -> int {\n  return a + b\n}\nfn main() {\n  x = add(1)\n  println("${x}")\n}\n' > "$TMP/arity.wyn"
out=$(perl -e 'alarm(15); exec @ARGV' -- "$WYN" check "$TMP/arity.wyn" 2>&1); rc=$?
if [ $rc -ge 1 ] && [ $rc -le 127 ] && echo "$out" | grep -q -- '-->' && echo "$out" | grep -q ':5'; then
    ok "arg count mismatch has --> file:line"
else
    bad "arg count mismatch has --> file:line (rc=$rc) [$(echo "$out" | head -2 | tr '\n' ' ')]"
fi

# 3. Runtime panic must NOT cite the generated `.wyn.c` file. Uses a lambda
#    body (hoisted; no #line remap) - the exact case that leaked `.wyn.c`.
printf 'fn main() {\n  xs = [1, 2, 3]\n  f = (i: int) -> int => xs[i]\n  y = f(20)\n  println("${y}")\n}\n' > "$TMP/panic.wyn"
rm -f "$TMP/panic.wyn.out" "$TMP/panic"
out=$(perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/panic.wyn" 2>&1); rc=$?
if echo "$out" | grep -q "index out of bounds" && ! echo "$out" | grep -q '\.wyn\.c'; then
    ok "runtime panic does not leak .wyn.c path"
else
    bad "runtime panic does not leak .wyn.c path (rc=$rc) [$(echo "$out" | tail -1)]"
fi

echo ""; echo "diagnostic-location: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
