#!/bin/bash
# Silent-wrong-answer batch (FLOWY_DESIGN P0): the error paths.
# The value paths (correct results) live in tests/regression/
# test_map_get_default.wyn, test_index_compound_assign.wyn,
# test_float_array_reductions.wyn. This script covers what must now be
# ERRORS instead of silently wrong output:
#   - unknown format specs in interpolation (${pi:.2}) - compile error
#   - map.get default type mismatch - compile error
#   - m[k] += v on a missing key - runtime panic (Python KeyError semantics)
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
    if [ $rc -eq 1 ] && echo "$out" | grep -q "$pat"; then ok "$name"
    else bad "$name (rc=$rc) [$(echo "$out" | head -1)]"; fi
}

# 1. Format spec in interpolation: silently swallowed before, error now.
printf 'pi = 3.14159\nprint("${pi:.2}")\n' > "$TMP/spec.wyn"
expect_check_error "format spec rejected" "$TMP/spec.wyn" "Unsupported syntax in string interpolation"

# 2. Width/align spec too.
printf 'n = 5\nprint("${n:>8}")\n' > "$TMP/spec2.wyn"
expect_check_error "align spec rejected" "$TMP/spec2.wyn" "Unsupported syntax in string interpolation"

# 3. map.get default type mismatch: the default IS the fallback value, so a
#    string default on an int map would reintroduce the garbage class.
printf 'm = {"a": 5}\nprint(m.get("a", "oops"))\n' > "$TMP/mismatch.wyn"
expect_check_error "get default type mismatch" "$TMP/mismatch.wyn" "map.get default is string but the map's values are int"

# 4. Compound assign on a missing map key: runtime panic, not invented 0.
printf 'm = {"a": 5}\nm["missing"] += 1\nprint(m["missing"])\n' > "$TMP/missing.wyn"
out=$(perl -e 'alarm(15); exec @ARGV' -- "$WYN" run "$TMP/missing.wyn" 2>&1); rc=$?
if echo "$out" | grep -q 'map key "missing" not found for compound assignment'; then
    ok "compound assign missing key panics"
else
    bad "compound assign missing key panics (rc=$rc) [$(echo "$out" | tail -1)]"
fi

# 5. Positive: compound assign on an EXISTING key works (guard against the
#    panic firing for present keys).
printf 'm = {"a": 5}\nm["a"] += 1\nprint(m["a"])\n' > "$TMP/present.wyn"
out=$(perl -e 'alarm(15); exec @ARGV' -- "$WYN" run "$TMP/present.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q '^6$'; then ok "compound assign present key works"
else bad "compound assign present key works (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 6. Positive: a colon INSIDE a string/expression is not a format spec.
printf 'url = "http://x.com:8080/y"\nprint("addr ${url}")\n' > "$TMP/colon.wyn"
out=$(perl -e 'alarm(15); exec @ARGV' -- "$WYN" run "$TMP/colon.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q 'addr http://x.com:8080/y'; then ok "colon in interpolated value still fine"
else bad "colon in interpolated value still fine (rc=$rc) [$(echo "$out" | tail -1)]"; fi

echo ""; echo "silent-wrong: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
