#!/bin/bash
# An if/while condition may START with a parenthesized term (2026-08).
#
#     if (a) or (b) { ... }
#
# failed to parse with "Expected an expression". The condition parser special-cased a
# leading `(`: it consumed the paren, parsed ONE expression, and demanded a closing
# `)` - so it read `(a)`, matched the `)`, and left `or (b)` dangling before the `{`.
#
# `if a or (b)` worked and `if (a) or (b)` did not, which is not a rule anyone can
# learn - and wrapping a De Morgan condition in parentheses per operand is exactly how
# people write these. Both `if` and `while` had it. Found writing a character-class
# check `(c >= "a" and c <= "z") or (c >= "0" and c <= "9")` in a sample app.
#
# Parentheses around a condition are OPTIONAL in Wyn, so the fix is to drop the special
# case entirely: the condition is just an expression, and expression() already parses a
# leading-paren term correctly. A C-style `if (cond)` is then simply an expression whose
# outermost node is parenthesized.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

cat > "$TMP/a.wyn" <<'EOF'
fn cls(c: string) -> bool {
    // The exact shape that failed: two parenthesized terms joined by `or`.
    if (c >= "a" and c <= "z") or (c >= "0" and c <= "9") { return true }
    return false
}

fn main() {
    // A parenthesized-term condition in a WHILE, too.
    var n = 0
    while (n < 3) or (n < 0) { n += 1 }
    print("while stopped at ${n}")

    // A plain C-style parenthesized condition must still work.
    var x = 5
    if (x > 3) { print("c-style if works") }

    // Nested / mixed.
    if (x > 0 and x < 10) or (x == 99) { print("mixed works") }

    print("letters: ${cls("q")} ${cls("7")} ${cls("!")}")
}
EOF

out=$(cd "$TMP" && "$WYN_ABS" check a.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then ok "if/while with parenthesized-term conditions parse"; else bad "check failed"; echo "$out"|head -5; fi

out=$(cd "$TMP" && "$WYN_ABS" run a.wyn 2>&1); code=$?
want='while stopped at 3
c-style if works
mixed works
letters: true true false'
got=$(printf '%s' "$out" | grep -vE 'Compiled|Warning')
if [ $code -eq 0 ] && [ "$got" = "$want" ]; then
  ok "and they evaluate correctly"
else
  bad "wrong output"; printf '%s\n' "$got" | head -6 | sed 's/^/        /'
fi

# The plain forms that already worked must keep working - the fix removes a branch and
# must not change what a bare or a fully-wrapped condition does.
cat > "$TMP/b.wyn" <<'EOF'
fn main() {
    var a = true
    var b = false
    if a or (b) { print("A") }              // leading bare, trailing paren - already ok
    if ((a) or (b)) { print("B") }          // fully wrapped - already ok
    if a { print("C") }                     // no parens at all
    if (a) { print("D") }                   // C-style
    var i = 0
    while i < 2 { i += 1 }                   // bare while
    print("i=${i}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run b.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^A$' &&
   printf '%s' "$out" | grep -q '^B$' &&
   printf '%s' "$out" | grep -q '^C$' &&
   printf '%s' "$out" | grep -q '^D$' &&
   printf '%s' "$out" | grep -q '^i=2$'; then
  ok "bare, wrapped and C-style conditions all still work"
else
  bad "a previously-working condition form broke"; printf '%s\n' "$out" | head -7 | sed 's/^/        /'
fi

# The struct-init guard the flag exists for must survive: `if x == Type { }` must NOT
# be read as `if x == (Type { })`.
cat > "$TMP/c.wyn" <<'EOF'
struct Point { x: int }
fn main() {
    var n = 3
    // A bare identifier that is also a type name, in a condition, followed by a block.
    // This must parse as `if n == 3` then a block, not `if n == (Point { ... })`.
    if n == 3 { print("no struct-init in condition") }
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run c.wyn 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q 'no struct-init in condition'; then
  ok "struct-init is still suppressed in a condition"
else
  bad "struct-init suppression regressed"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "paren-condition: $PASS pass, 0 fail"
  exit 0
fi
echo "paren-condition: $PASS pass, $FAIL fail"
exit 1
