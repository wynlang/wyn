#!/bin/bash
# Cross-type comparison soundness: a comparison whose operands are not in the
# same comparable family used to type-check (rc 0) and then SIGSEGV at runtime
# (codegen emitted strcmp((char*)5, ...)). These must now be clean CHECK-TIME
# errors (rc 1), never a signal. Positive cases live in
# tests/regression/cross_type_cmp_ok.wyn.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# expect_check_error <name> <file> <grep-pattern>
expect_check_error() {
    local name="$1"; local file="$2"; local pat="$3"
    local out rc
    out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$file" 2>&1); rc=$?
    if [ $rc -ge 1 ] && [ $rc -le 127 ] && echo "$out" | grep -q "$pat"; then ok "$name"
    else bad "$name (rc=$rc) [$out]"; fi
}

printf 'fn main() {\n var a = 5\n var b = "5"\n if a == b { println("eq") }\n}\n' > "$TMP/is.wyn"
expect_check_error "int == string is a check error (not segfault)" "$TMP/is.wyn" "Cannot compare"

printf 'fn main() {\n var a = "5"\n var b = 5\n if a == b { println("eq") }\n}\n' > "$TMP/si.wyn"
expect_check_error "string == int is a check error" "$TMP/si.wyn" "Cannot compare"

printf 'fn main() {\n var a = true\n var b = "x"\n if a == b { println("eq") }\n}\n' > "$TMP/bs.wyn"
expect_check_error "bool == string is a check error" "$TMP/bs.wyn" "Cannot compare"

printf 'fn main() {\n var a = [1,2]\n var b = [1,2]\n if a == b { println("eq") }\n}\n' > "$TMP/aa.wyn"
expect_check_error "array == array is a check error" "$TMP/aa.wyn" "Cannot compare"

echo ""; echo "cross-type-cmp: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
