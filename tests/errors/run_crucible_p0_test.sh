#!/bin/bash
# HN-crucible P0 batch (2026-07-22): the runtime-panic paths.
# Value paths (correct results) live in tests/regression/ and tests/expect/:
#   test_float_print_canonical.wyn, test_sci_notation_literals.wyn,
#   test_map_value_overwrite_read.wyn, test_stringbuilder_many.wyn.
# This script covers what must now be FATAL instead of silently wrong:
#   - out-of-bounds array/string index: panic + exit 1 (was: return 0/"" and continue)
#   - WYN_LENIENT=1 opt-out restores the old print-and-continue behavior
#   - .to_int()/.to_float() on garbage or overflow: panic (was: silent 0)
#   - concurrent array.push from awaited spawns: "concurrent array mutation" panic (was: heap corruption SIGABRT)
#   - 1M-deep recursion: named "stack overflow" panic (was: silent SIGILL/SIGSEGV)
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# run_case name file expected_rc pattern [env]
run_case() {
    local name="$1" file="$2" want_rc="$3" pat="$4" envs="${5:-}"
    local out rc
    out=$(env $envs perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$file" 2>&1); rc=$?
    if [ "$rc" -eq "$want_rc" ] && echo "$out" | grep -q "$pat"; then ok "$name"
    else bad "$name (rc=$rc, want $want_rc) [$(echo "$out" | grep -v Compiled | head -1)]"; fi
}

# 1. Array OOB read is fatal by default.
printf 'fn main() {\n    a = [1, 2, 3]\n    x = a[10]\n    print("unreachable ${x}")\n}\n' > "$TMP/oob.wyn"
run_case "array OOB read panics (exit 1)" "$TMP/oob.wyn" 1 "array index out of bounds: index 10, length 3"
out=$(perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/oob.wyn" 2>&1)
if echo "$out" | grep -q "unreachable"; then bad "array OOB must not continue"; else ok "array OOB does not continue"; fi

# 2. WYN_LENIENT=1 opt-out: prints the panic but continues with 0.
"$WYN" build "$TMP/oob.wyn" >/dev/null 2>&1
out=$(WYN_LENIENT=1 perl -e 'alarm(20); exec @ARGV' -- "$TMP/oob" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q "unreachable 0"; then ok "WYN_LENIENT=1 restores continue-with-0"
else bad "WYN_LENIENT=1 restores continue-with-0 (rc=$rc)"; fi

# 3. String OOB index is fatal too (sibling of the array check).
printf 'fn main() {\n    s = "abc"\n    c = s[10]\n    print("unreachable")\n}\n' > "$TMP/soob.wyn"
run_case "string OOB index panics" "$TMP/soob.wyn" 1 "string index out of bounds: index 10, length 3"

# 4. Negative index beyond -len is OOB, not wrapped twice.
printf 'fn main() {\n    a = [1, 2, 3]\n    x = a[-4]\n    print("unreachable")\n}\n' > "$TMP/negoob.wyn"
run_case "negative index beyond -len panics" "$TMP/negoob.wyn" 1 "array index out of bounds"

# 5. to_int on garbage panics.
printf 'fn main() {\n    print("abc".to_int())\n}\n' > "$TMP/garbage.wyn"
run_case "to_int(garbage) panics" "$TMP/garbage.wyn" 1 'to_int parse error: "abc"'

# 6. to_int on overflow panics with the value in the message.
printf 'fn main() {\n    print("99999999999999999999".to_int())\n}\n' > "$TMP/overflow.wyn"
run_case "to_int(overflow) panics" "$TMP/overflow.wyn" 1 'to_int overflow: "99999999999999999999"'

# 7. to_float on garbage panics.
printf 'fn main() {\n    print("xyz".to_float())\n}\n' > "$TMP/fgarbage.wyn"
run_case "to_float(garbage) panics" "$TMP/fgarbage.wyn" 1 'to_float parse error: "xyz"'

# 8. Lenient mode restores silent-0 parses.
printf 'fn main() {\n    print("abc".to_int())\n}\n' > "$TMP/lenient_parse.wyn"
"$WYN" build "$TMP/lenient_parse.wyn" >/dev/null 2>&1
out=$(WYN_LENIENT=1 perl -e 'alarm(20); exec @ARGV' -- "$TMP/lenient_parse" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q '^0$'; then ok "WYN_LENIENT=1 to_int returns 0"
else bad "WYN_LENIENT=1 to_int returns 0 (rc=$rc)"; fi

# 9. Valid parses still work (guard against over-strictness).
printf 'fn main() {\n    print("42".to_int())\n    print("-7".to_int())\n    print("3.5".to_float())\n}\n' > "$TMP/valid.wyn"
out=$(perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/valid.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q '^42$' && echo "$out" | grep -q '^-7$' && echo "$out" | grep -q '^3.5$'; then
    ok "valid to_int/to_float still parse"
else bad "valid to_int/to_float still parse (rc=$rc) [$out]"; fi

# 10. Concurrent array.push: clear panic, not heap-corruption SIGABRT.
cat > "$TMP/race_push.wyn" <<'EOF'
var items = []
fn writer(n: int) -> int {
    for i in 0..50000 { items.push(i) }
    return n
}
fn main() {
    a = spawn writer(1)
    b = spawn writer(2)
    c = spawn writer(3)
    d = spawn writer(4)
    r = await a + await b + await c + await d
    print(items.len())
}
EOF
out=$(perl -e 'alarm(30); exec @ARGV' -- "$WYN" run "$TMP/race_push.wyn" 2>&1); rc=$?
# The race is nondeterministic: either the writers happen to serialize (fine)
# or the detector fires. What must NEVER happen is heap corruption (SIGABRT
# "panic: abort" with exit 134) or a silent bogus result.
if echo "$out" | grep -q "concurrent array mutation detected"; then
    ok "concurrent push detected with clear panic"
elif [ $rc -eq 0 ] && echo "$out" | grep -q "200000"; then
    ok "concurrent push serialized cleanly (all 200000 elements)"
else
    bad "concurrent push (rc=$rc) [$(echo "$out" | grep -v Compiled | head -1)]"
fi

# 11. Deep recursion must not die SILENTLY. Depending on the environment's
# stack rlimit it either (a) overflows - then the panic must be NAMED (the
# old behavior was a zero-output SIGILL, exit 132), or (b) completes - then
# the answer must be right. Both are acceptable; silence is the bug.
printf 'fn r(n: int) -> int {\n    if n <= 0 { return 0 }\n    return r(n - 1) + 1\n}\nfn main() {\n    print(r(1000000))\n}\n' > "$TMP/deep.wyn"
out=$(perl -e 'alarm(30); exec @ARGV' -- "$WYN" run "$TMP/deep.wyn" 2>&1); rc=$?
if [ $rc -ne 0 ] && echo "$out" | grep -q "stack overflow"; then
    ok "deep recursion names stack overflow"
elif [ $rc -eq 0 ] && echo "$out" | grep -q "1000000"; then
    ok "deep recursion completed (large stack rlimit in this env)"
else
    bad "deep recursion silent crash (rc=$rc) [$(echo "$out" | grep -v Compiled | head -1)]"
fi

echo ""
echo "crucible-p0: $PASS pass, $FAIL fail"
[ $FAIL -eq 0 ] || exit 1
