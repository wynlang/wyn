#!/bin/bash
# `wyn fix` migrator regression: removed syntax is rewritten to canonical form,
# string literals + comments are preserved, and the result compiles. (R1, 2026-07)
set -uo pipefail

WYN="${WYN:-./wyn}"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
PASS=0
FAIL=0

ok()   { echo "  ok    $1"; PASS=$((PASS+1)); }
bad()  { echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# 1) Operators + keywords are migrated; strings/comments preserved.
f="$TMP/mig.wyn"
cat > "$f" <<'EOF'
fn cls(n: int) -> string {
  if n > 0 && n < 10 { return "small" }
  elseif n == 0 || n > 99 { return "edge" }
  if !(n == 5) { return "x" }
  return "other"
}
fn main() {
  print(cls(5))
  var lit = "a && b || c elseif"
  print(lit)
}
EOF
"$WYN" fix "$f" >/dev/null 2>&1
# Operators must be gone from CODE. The string literal `"a && b || c elseif"` is
# expected to keep them (preservation is the point), so check the two code lines
# that had operators, not the whole file.
if grep -qE '(n > 0|n == 0).*(&&|\|\|)' "$f"; then bad "operators still present in code after fix"; else ok "operators migrated"; fi
if grep -q 'elseif n == 0' "$f"; then bad "elseif keyword not migrated"; else ok "elseif migrated"; fi
if grep -q '"a && b || c elseif"' "$f"; then ok "string literal preserved"; else bad "string literal mangled"; fi
# 2) Migrated file compiles + runs correctly.
out=$("$WYN" build "$f" >/dev/null 2>&1 && "${f%.wyn}" 2>&1; rm -f "${f%.wyn}" "${f}.c" 2>/dev/null)
if [ "$out" = "small
a && b || c elseif" ]; then ok "migrated file runs correctly"; else bad "migrated output wrong: [$out]"; fi
# 3) Idempotent: a clean file reports no changes (exit 0, no 'fixed').
out2=$("$WYN" fix "$f" 2>&1)
if echo "$out2" | grep -q "No removed syntax found"; then ok "idempotent on clean file"; else bad "not idempotent: $out2"; fi
# 4) --check exits non-zero when changes are needed.
cat > "$f" <<'EOF'
fn main() { if true && false { print("x") } }
EOF
"$WYN" fix "$f" --check >/dev/null 2>&1
if [ $? -ne 0 ]; then ok "--check exits non-zero on dirty file"; else bad "--check should exit 1 on dirty file"; fi

echo ""
echo "wyn-fix: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ]
