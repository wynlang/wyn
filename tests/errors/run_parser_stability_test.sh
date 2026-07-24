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

# 8. Leading binary operator (no left operand) - used to SEGFAULT `wyn check`
#    itself: primary() returned NULL and multiplication() dereferenced it. Now a
#    clean "Expected an expression" error. (2026-07 crash-safety batch)
printf 'fn main() {\n    x = * 5\n    println("hi")\n}\n' > "$TMP/h.wyn"
expect_clean_error "leading * no longer segfaults check" "$TMP/h.wyn"
printf 'fn main() {\n    x = / 5\n    println("hi")\n}\n' > "$TMP/i.wyn"
expect_clean_error "leading / no longer segfaults check" "$TMP/i.wyn"

# 9. Doubled binary operator - passed check then SEGFAULTED `wyn build` (NULL
#    right operand). Now rejected cleanly at check.
printf 'fn main() {\n    x = 2 ** 3\n    println("${x}")\n}\n' > "$TMP/j.wyn"
expect_clean_error "2 ** 3 no longer segfaults build" "$TMP/j.wyn"
printf 'fn main() {\n    x = 1 << << 2\n    println("${x}")\n}\n' > "$TMP/k.wyn"
expect_clean_error "1 << << 2 no longer segfaults build" "$TMP/k.wyn"

# 10. Empty string interpolation ${} - passed check AND build, then SEGFAULTED at
#     RUNTIME (null/empty interp expr). Now rejected cleanly at check.
printf 'fn main() {\n    println("x${}y")\n}\n' > "$TMP/l.wyn"
expect_clean_error "empty \${} no longer segfaults at runtime" "$TMP/l.wyn"
printf 'fn main() {\n    println("x${ }y")\n}\n' > "$TMP/m.wyn"
expect_clean_error "whitespace \${ } no longer segfaults at runtime" "$TMP/m.wyn"

# 11. Bare `return` (no value) inside a function stays valid - guard against the
#     crash-safety fix over-rejecting the value-less return form.
printf 'fn main() {\n    var x = 5\n    if x > 3 {\n        return\n    }\n    println("small")\n}\n' > "$TMP/n.wyn"
out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$TMP/n.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ]; then ok "bare return still accepted"; else bad "bare return rejected: rc=$rc [$out]"; fi

# 12. Deeply nested `(` / `[` - used to overflow the C stack and SIGSEGV the
#     compiler (exit 139). Now a clean "nesting too deep" error, not a signal.
python3 -c "print('fn main() -> int {\n x = ' + '('*5000 + '1' + ')'*5000 + '\n return x\n}')" > "$TMP/deep_paren.wyn"
out=$(perl -e 'alarm(12); exec @ARGV' -- "$WYN" check "$TMP/deep_paren.wyn" 2>&1); rc=$?
if [ $rc -ge 1 ] && [ $rc -lt 128 ] && echo "$out" | grep -q "nesting too deep"; then ok "5000 nested parens: clean error, no segfault"
else bad "deep parens (rc=$rc) [$(echo "$out" | tail -1)]"; fi
python3 -c "print('fn main() -> int {\n x = ' + '['*2000 + '1' + ']'*2000 + '\n return 0\n}')" > "$TMP/deep_arr.wyn"
out=$(perl -e 'alarm(12); exec @ARGV' -- "$WYN" check "$TMP/deep_arr.wyn" 2>&1); rc=$?
if [ $rc -ge 1 ] && [ $rc -lt 128 ] && echo "$out" | grep -q "nesting too deep"; then ok "2000 nested arrays: clean error, no segfault"
else bad "deep arrays (rc=$rc) [$(echo "$out" | tail -1)]"; fi
# A modestly deep (100) nest must still parse - the guard must not reject real code.
python3 -c "print('fn main() -> int {\n x = ' + '('*100 + '1' + ')'*100 + '\n return x\n}')" > "$TMP/ok_paren.wyn"
out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$TMP/ok_paren.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ]; then ok "100 nested parens still parse (no over-reject)"; else bad "100-deep over-rejected (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 13. An illegal character used to be lexed as premature EOF, making the parser
#     blame the enclosing block ("Expected '}'") at the wrong line. Now the lexer
#     reports the offending char/byte at its own column.
for ch in '@' '`' '\'; do
    printf 'fn main() -> int {\n    x = 5 %s 3\n    return x\n}\n' "$ch" > "$TMP/illegal.wyn"
    out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$TMP/illegal.wyn" 2>&1); rc=$?
    if [ $rc -ge 1 ] && [ $rc -lt 128 ] && echo "$out" | grep -q "unexpected character" && ! echo "$out" | grep -q "Expected '}'"; then
        ok "illegal char '$ch' → unexpected-character (not Expected '}')"
    else bad "illegal '$ch' (rc=$rc) [$(echo "$out" | tail -1)]"; fi
done
# A non-ASCII byte is reported as a hex byte.
printf 'fn main() -> int {\n    x = 5 \xC3 3\n    return x\n}\n' > "$TMP/illegal_byte.wyn"
out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$TMP/illegal_byte.wyn" 2>&1); rc=$?
if [ $rc -ge 1 ] && [ $rc -lt 128 ] && echo "$out" | grep -q "unexpected byte"; then ok "illegal non-ASCII byte → unexpected-byte"
else bad "illegal byte (rc=$rc) [$(echo "$out" | tail -1)]"; fi

echo ""; echo "parser-stability: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
