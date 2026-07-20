#!/bin/bash
# struct ==/!=/< error paths: things that used to ICE (C-level struct ==) or
# would miscompile must be clean CHECK-TIME errors with actionable messages.
# Positive cases live in tests/expect/test_struct_equality.wyn.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# expect_error <name> <file> <grep-pattern>
expect_error() {
    local name="$1"; local file="$2"; local pat="$3"
    local out rc
    out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$file" 2>&1); rc=$?
    if [ $rc -eq 1 ] && echo "$out" | grep -q "$pat"; then ok "$name"
    else bad "$name (rc=$rc) [$out]"; fi
}

# 1. Struct with an array field: == is a clean error, not an ICE.
printf 'struct Bag { items: [int] }\nfn main() {\n    a = Bag{items: [1]}\n    b = Bag{items: [1]}\n    println(a == b)\n}\n' > "$TMP/a.wyn"
expect_error "array-field struct == is a check error" "$TMP/a.wyn" "not comparable"

# 2. Different struct types can't compare.
printf 'struct Point { x: int, y: int }\nstruct Size { w: int, h: int }\nfn main() {\n    a = Point{x: 1, y: 2}\n    b = Size{w: 1, h: 2}\n    println(a == b)\n}\n' > "$TMP/b.wyn"
expect_error "Point == Size is a check error" "$TMP/b.wyn" "Cannot compare different types"

# 3. Ordering on structs is rejected (raw C < on structs doesn't compile).
printf 'struct Point { x: int, y: int }\nfn main() {\n    a = Point{x: 1, y: 2}\n    b = Point{x: 1, y: 3}\n    println(a < b)\n}\n' > "$TMP/c.wyn"
expect_error "struct < struct is a check error" "$TMP/c.wyn" "cannot be ordered"

# 4. Option rejections unchanged (must NOT route to the struct-eq path).
printf 'fn find(n: int) -> int? {\n    if n > 0 { return Some(n) }\n    return none\n}\nfn main() {\n    a = find(5)\n    println(a == none)\n}\n' > "$TMP/d.wyn"
expect_error "Option == none still gives is_none hint" "$TMP/d.wyn" "is_none"

printf 'fn find(n: int) -> int?  {\n    if n > 0 { return Some(n) }\n    return none\n}\nfn main() {\n    a = find(5)\n    b = find(5)\n    println(a == b)\n}\n' > "$TMP/e.wyn"
expect_error "Option == Option still rejected" "$TMP/e.wyn" "cannot be compared directly"

echo ""; echo "struct-eq: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
