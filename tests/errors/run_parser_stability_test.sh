#!/bin/bash
# Parser/lexer stability: constructs that used to HANG or SEGFAULT the
# compiler must now produce clean errors (nonzero exit, no signal death).
# Found by the fuzz harness + forgiveness audit (2026-07-20).
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# Clean rejection = exit 1 (error), NOT >=128 (signal: hang-timeout or crash).
expect_clean_error() {
    local name="$1"; local file="$2"
    perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$file" >/dev/null 2>&1
    local rc=$?
    if [ $rc -ge 1 ] && [ $rc -lt 128 ]; then ok "$name"; else bad "$name (rc=$rc)"; fi
}

# 1. `if (x) == 1 { } else { }` - used to hang the fn-body parse loop forever.
printf 'fn main() {\n    x = 5\n    if (x) == 1 {\n        println(1)\n    } else {\n        println(2)\n    }\n}\n' > "$TMP/a.wyn"
expect_clean_error "if (x) == 1 +else no longer hangs" "$TMP/a.wyn"

# 2. Python lambda - used to hang.
printf 'fn main() {\n    f = lambda x: x + 1\n}\n' > "$TMP/b.wyn"
expect_clean_error "python lambda no longer hangs" "$TMP/b.wyn"

# 3. `!cond` - used to hang.
printf 'fn main() {\n    ok = true\n    if !ok {\n        println(1)\n    }\n}\n' > "$TMP/c.wyn"
expect_clean_error "bang-negation no longer hangs" "$TMP/c.wyn"

# 4. Type-annotated declaration without var - used to hang.
printf 'fn main() {\n    a: int = 5\n    println(a)\n}\n' > "$TMP/d.wyn"
expect_clean_error "bare annotated decl no longer hangs" "$TMP/d.wyn"

# 5. Nested duplicated fn header - used to SEGFAULT the checker.
printf 'fn f(p: int) -> int {\nfn f(p: int) -> int {\n    return 1\n}\nfn main() {\n    println(1)\n}\n' > "$TMP/e.wyn"
expect_clean_error "nested fn decl no longer segfaults" "$TMP/e.wyn"

# 6. `===` - used to pass check then SEGFAULT the build; now a clean error.
printf 'fn main() {\n    x = 5\n    if x === 5 {\n        println(1)\n    }\n}\n' > "$TMP/f.wyn"
out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$TMP/f.wyn" 2>&1); rc=$?
if [ $rc -eq 1 ] && echo "$out" | grep -q "is not an operator"; then ok "=== gives use-== error"; else bad "===: rc=$rc [$out]"; fi

# 7. UTF-8 BOM - used to silently drop the whole program body.
printf '\xef\xbb\xbffn main() {\n    println("bom-ok")\n}\n' > "$TMP/g.wyn"
out=$(perl -e 'alarm(15); exec @ARGV' -- "$WYN" run "$TMP/g.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q "bom-ok"; then ok "UTF-8 BOM skipped, program runs"; else bad "BOM: rc=$rc [$out]"; fi

echo ""; echo "parser-stability: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
