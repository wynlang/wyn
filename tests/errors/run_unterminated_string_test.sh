#!/bin/bash
# Unterminated string literals must report "Unterminated string literal"
# pointing at the OPENING quote — not a misleading "Expected '}'/')'" at the
# end of the file (the lexer used to silently swallow the rest of the file
# and fabricate an EOF token). Also: exactly ONE error per typo (no cascade),
# and identical consecutive diagnostics are deduped.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# expect_one_error <name> <file> <grep-pattern> <must-point-at>
expect_one_error() {
    local name="$1" file="$2" pat="$3" loc="$4"
    local out rc nerr
    out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$file" 2>&1); rc=$?
    nerr=$(echo "$out" | grep -c "^Error")
    if [ $rc -eq 1 ] && [ "$nerr" -eq 1 ] && echo "$out" | grep -q "$pat" \
       && echo "$out" | grep -q -- "$loc"; then ok "$name"
    else bad "$name (rc=$rc nerr=$nerr) [$(echo "$out" | head -2 | tr '\n' ' ')]"; fi
}

# 1. Mid-function: points at line 2 (the opening quote), not the file end.
printf 'fn main() {\n    s = "hello\n    println(s)\n}\n' > "$TMP/a.wyn"
expect_one_error "unterminated string mid-fn" "$TMP/a.wyn" "Unterminated string literal" ":2:9"

# 2. At EOF (no trailing newline).
printf 'fn main() {\n    s = "hello' > "$TMP/b.wyn"
expect_one_error "unterminated string at EOF" "$TMP/b.wyn" "Unterminated string literal" ":2:"

# 3. In call args: one error, no "Expected ')'" cascade.
printf 'fn main() {\n    println("hello\n}\n' > "$TMP/c.wyn"
out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$TMP/c.wyn" 2>&1); rc=$?
if [ $rc -eq 1 ] && ! echo "$out" | grep -q "Expected ')'" \
   && echo "$out" | grep -q "Unterminated string literal"; then ok "call-arg string: no ')' cascade"
else bad "call-arg string (rc=$rc)"; fi

# 4. Unterminated interpolation "${...
printf 'fn main() {\n    name = "x"\n    s = "hi ${name\n}\n' > "$TMP/d.wyn"
out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$TMP/d.wyn" 2>&1); rc=$?
if [ $rc -eq 1 ] && echo "$out" | grep -q "Unterminated string"; then ok "unterminated interpolation"
else bad "interpolation (rc=$rc)"; fi

# 5. Single-quote string.
printf "fn main() {\n    s = 'oops\n    println(s)\n}\n" > "$TMP/e.wyn"
expect_one_error "unterminated single-quote string" "$TMP/e.wyn" "Unterminated string literal" ":2:"

# 6. Multi-line """ hits EOF: reports the OPENING line (2), not the last line.
printf 'fn main() {\n    s = """hello\n    println(s)\n}\n' > "$TMP/f.wyn"
expect_one_error "unterminated triple-quote string" "$TMP/f.wyn" "Unterminated multi-line string" ":2:"

# 7. Dedupe: nested unclosed ifs used to print the identical error 2-3x.
printf 'fn main() {\n if true {\n  if true {\n' > "$TMP/g.wyn"
out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$TMP/g.wyn" 2>&1); rc=$?
nerr=$(echo "$out" | grep -c "^Error")
if [ $rc -eq 1 ] && [ "$nerr" -eq 1 ]; then ok "identical errors deduped"
else bad "dedupe (rc=$rc nerr=$nerr)"; fi

# 8. No regression: escapes, interpolation, multi-line, single-quote all valid.
printf 'fn main() {\n    a = "x ${1 + 1} y"\n    b = """multi\nline"""\n    c = %s\n    d = "es\\"cape"\n    println(a)\n}\n' "'single'" > "$TMP/h.wyn"
out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$TMP/h.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ]; then ok "valid strings still pass"
else bad "valid strings (rc=$rc) [$(echo "$out" | head -2 | tr '\n' ' ')]"; fi

echo ""; echo "unterminated-string: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
