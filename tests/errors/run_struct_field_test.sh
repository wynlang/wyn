#!/bin/bash
# Struct field typos used to pass `wyn check` and only fail at build with a raw
# C error ("no member named 'namee'"). Accessing a genuinely-absent field on a
# known user struct must now be a clean CHECK-TIME error. Valid field access is
# covered by tests/regression/struct_field_ok.wyn.
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

printf 'struct User { name: string, age: int }\nfn main() {\n var u = User{name: "a", age: 1}\n println(u.namee)\n}\n' > "$TMP/typo.wyn"
expect_check_error "field typo u.namee is a check error" "$TMP/typo.wyn" "has no field 'namee'"

printf 'struct Pt { x: int, y: int }\nfn main() {\n var p = Pt{x: 1, y: 2}\n println(p.z)\n}\n' > "$TMP/absent.wyn"
expect_check_error "absent field p.z is a check error" "$TMP/absent.wyn" "has no field 'z'"

echo ""; echo "struct-field: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
