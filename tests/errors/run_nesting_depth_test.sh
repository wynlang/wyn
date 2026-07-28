#!/bin/bash
# Pathologically deep nesting used to overflow the C stack and SIGSEGV the
# COMPILER (rc 139). The expression() depth guard didn't cover the
# statement/block path nor the prefix-unary operand path. Deeply nested blocks,
# nested `if`, and repeated unary `not` must now yield a clean "nesting too
# deep" diagnostic (rc in [1,127]), and a modest 100-deep nest must still parse.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# Watchdog is 60s, not 15s. These inputs are deliberately pathological (1500
# nested blocks), and the point of the test is that the compiler produces a clean
# "too deep" DIAGNOSTIC instead of a segfault or a hang - not that it does so
# within any particular time. 15s was enough locally (measured 0.6s) but the
# macos-15-intel CI runner timed out at it (rc=142), the same runner that needs
# ~3.3x this machine's wall clock elsewhere in the suite. A watchdog that fires
# on the slowest platform turns a correctness gate into a flake, and this gate's
# value is being believed. 60s still catches a genuine hang or runaway recursion.
expect_deep_error() {
    local name="$1"; local file="$2"
    local out rc
    out=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" check "$file" 2>&1); rc=$?
    if [ $rc -ge 1 ] && [ $rc -le 127 ] && echo "$out" | grep -qi "too deep"; then ok "$name"
    else bad "$name (rc=$rc) [$(echo "$out" | head -2)]"; fi
}

python3 -c "print('fn main(){' + '{'*1500 + '}'*1500 + '}')" > "$TMP/blocks.wyn"
expect_deep_error "1500 nested blocks -> clean error, not segfault" "$TMP/blocks.wyn"

python3 -c "print('fn main(){' + 'if true '*1500 + '{}' + '}')" > "$TMP/ifs.wyn"
expect_deep_error "1500 nested if -> clean error, not segfault" "$TMP/ifs.wyn"

python3 -c "print('fn main(){ var x = ' + 'not '*600 + 'true }')" > "$TMP/nots.wyn"
expect_deep_error "600 nested not -> clean error, not segfault" "$TMP/nots.wyn"

# Nested ARRAY literals recurse through the whole precedence chain per level, so
# each `[` pushes far more C frames than a bare `(`. Deeply nested `[[[...` used
# to SIGSEGV at ~460 levels - BELOW the 500 expr_depth guard, which only counted
# one increment per expression(). The array-literal branch now charges an extra
# depth unit, so it trips the guard cleanly instead of crashing the native stack.
python3 -c "print('fn main(){ var x = ' + '['*519 + '1' + ']'*519 + ' }')" > "$TMP/arrs.wyn"
expect_deep_error "519 nested array literals -> clean error, not segfault" "$TMP/arrs.wyn"

# A modest 100-deep block nest must still parse cleanly (no over-reject).
python3 -c "print('fn main(){' + '{'*100 + 'println(\"ok\")' + '}'*100 + '}')" > "$TMP/ok100.wyn"
out=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" check "$TMP/ok100.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ]; then ok "100 nested blocks still parse"; else bad "100-deep over-rejected (rc=$rc) [$out]"; fi

# A modest 100-deep array-literal nest must still parse cleanly (no over-reject).
python3 -c "print('fn main(){ var x = ' + '['*100 + '1' + ']'*100 + '; println(\"ok\") }')" > "$TMP/okarr100.wyn"
out=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" check "$TMP/okarr100.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ]; then ok "100 nested array literals still parse"; else bad "100-deep array over-rejected (rc=$rc) [$out]"; fi

echo ""; echo "nesting-depth: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
