#!/bin/bash
# An empty `0x` / `0b` literal (no digits after the prefix) used to lex as a
# 0-value INT, pass `wyn check`, and then hit an internal codegen error. It must
# now be a clean LEXER error at check time (rc in [1,127]). Valid radix literals
# are covered by tests/regression/radix_literal_ok.wyn.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

expect_check_error() {
    local name="$1"; local file="$2"; local pat="$3"
    local out rc
    out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$file" 2>&1); rc=$?
    if [ $rc -ge 1 ] && [ $rc -le 127 ] && echo "$out" | grep -q "$pat"; then ok "$name"
    else bad "$name (rc=$rc) [$out]"; fi
}

printf 'fn main() { var a = 0x }\n' > "$TMP/hex.wyn"
expect_check_error "empty 0x is a check error" "$TMP/hex.wyn" "hex literal"

printf 'fn main() { var a = 0b }\n' > "$TMP/bin.wyn"
expect_check_error "empty 0b is a check error" "$TMP/bin.wyn" "binary literal"

echo ""; echo "empty-radix-literal: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
