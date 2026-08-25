#!/bin/bash
# A selective import may name more than 32 things (2026-08).
#
#     import { a, b, c, ... 33 names ... } from mod
#
# failed with "Expected '}' after import list". The list was a fixed malloc(32) and the
# loop stopped at `item_count < 32`, so past 32 names the parser stopped consuming and
# then expect(TOKEN_RBRACE) hit the next comma - reporting a MISSING BRACE for what was
# actually a size limit, which is unactionable. The guard was also on the wrong side of
# the store, so a reorder would have overrun the allocation.
#
# There are TWO import-list parsers in parser.c (a duplicate), which is how the first fix
# landed in the one that does not run. Both are now growable.
#
# A 33+ name import is not exotic: it is what importing the surface of one real module
# looks like - found importing a game engine's API into its test file, which needed 34.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# A module exporting 40 functions, and a main importing all of them by name.
gen() {
  local n="$1" out="$2"
  : > "$out"
  local i=1
  while [ "$i" -le "$n" ]; do echo "pub fn f$i() -> int => $i" >> "$out"; i=$((i+1)); done
}

gen 40 "$TMP/lib.wyn"

names_upto() {
  local n="$1" i=1 s=""
  while [ "$i" -le "$n" ]; do
    if [ "$i" -eq 1 ]; then s="f1"; else s="$s, f$i"; fi
    i=$((i+1))
  done
  echo "$s"
}

# ---- the boundary: 32 worked, 33 did not -----------------------------------

for n in 32 33 34 40; do
  printf 'import { %s } from lib\nfn main() { print("ok ${f1()}") }\n' "$(names_upto $n)" > "$TMP/m$n.wyn"
  out=$(cd "$TMP" && "$WYN_ABS" run "m$n.wyn" 2>&1); code=$?
  if [ $code -eq 0 ] && printf '%s' "$out" | grep -q '^ok 1$'; then
    ok "$n-name import list works"
  else
    bad "$n-name import list failed"; printf '%s\n' "$out" | grep -iE 'error|Expected' | head -2 | sed 's/^/        /'
  fi
done

# The specific misleading error must not come back for a valid, large list.
printf 'import { %s } from lib\nfn main() { print("x") }\n' "$(names_upto 36)" > "$TMP/big.wyn"
out=$(cd "$TMP" && "$WYN_ABS" check "big.wyn" 2>&1); code=$?
if echo "$out" | grep -q "Expected '}' after import list"; then
  bad "a 36-name list still reports a missing brace"
else
  ok "no bogus 'missing brace' on a large list"
fi

# ---- and small lists are unchanged -----------------------------------------
printf 'import { f1, f2, f3 } from lib\nfn main() { print("s ${f1() + f2() + f3()}") }\n' > "$TMP/small.wyn"
out=$(cd "$TMP" && "$WYN_ABS" run "small.wyn" 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q '^s 6$'; then
  ok "a small import list still works"
else
  bad "small list regressed"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "import-list: $PASS pass, 0 fail"
  exit 0
fi
echo "import-list: $PASS pass, $FAIL fail"
exit 1
