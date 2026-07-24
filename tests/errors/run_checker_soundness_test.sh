#!/bin/bash
# Checker soundness gate (batch C, K5-K11): programs that USED to pass `wyn check`
# and then crash / ICE the C compiler / produce silent garbage at build or run
# time. The checker validated arms and operands but not overall soundness, so the
# error surfaced as a raw clang message (or wrong output) instead of a Wyn
# diagnostic. Each case below must now be a clean `wyn check` error (rc=1) with a
# helpful message - never a C-compiler ICE and never a silent pass.
#
# The matching VALUE paths (things that must still succeed) live in
# tests/regression/test_result_try_variants.wyn and the positive cases at the
# bottom of this script.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# A `wyn check` that must fail with rc=1 AND print a matching diagnostic.
expect_check_error() {
    local name="$1"; local file="$2"; local pat="$3"
    local out rc
    out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$file" 2>&1); rc=$?
    if [ $rc -eq 1 ] && echo "$out" | grep -q "$pat"; then ok "$name"
    else bad "$name (rc=$rc) [$(echo "$out" | head -1)]"; fi
}

# A `wyn check` that must SUCCEED (rc=0) - guards against over-rejection.
expect_check_ok() {
    local name="$1"; local file="$2"
    local out rc
    out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$file" 2>&1); rc=$?
    if [ $rc -eq 0 ]; then ok "$name"
    else bad "$name (rc=$rc) [$(echo "$out" | head -1)]"; fi
}

# --- K6: `?` only applies to a Result value ---------------------------------
# `?` on an Option unwrapped to a ResultInt temp in codegen -> C type mismatch.
printf 'fn g() -> int? { return 5 }\nfn f() -> int { x = g()?\n return x }\nfn main(){ print(f()) }\n' > "$TMP/k6_opt.wyn"
expect_check_error "K6 ? on Option rejected" "$TMP/k6_opt.wyn" "'?' operator can only be applied to a Result value"

# `?` on a bare scalar too.
printf 'fn main(){ x = 5\n y = x?\n print(y) }\n' > "$TMP/k6_scalar.wyn"
expect_check_error "K6 ? on scalar rejected" "$TMP/k6_scalar.wyn" "'?' operator can only be applied to a Result value"

# --- K5: unknown struct-init field ------------------------------------------
printf 'struct P { x: int, y: int }\nfn main(){ p = P{x:1,y:2,z:3}\n print(p.x) }\n' > "$TMP/k5.wyn"
expect_check_error "K5 unknown struct field rejected" "$TMP/k5.wyn" "struct 'P' has no field 'z'"

# --- K11: missing struct-init field -----------------------------------------
printf 'struct P { x: int, y: int }\nfn main(){ p = P{x:1}\n print(p.x) }\n' > "$TMP/k11.wyn"
expect_check_error "K11 missing struct field rejected" "$TMP/k11.wyn" "struct 'P' is missing field 'y'"

# --- K7: calling a non-function local ---------------------------------------
printf 'fn main(){ x = 5\n print(x(3)) }\n' > "$TMP/k7.wyn"
expect_check_error "K7 calling a scalar rejected" "$TMP/k7.wyn" "is not a function and cannot be called"

# --- K8: integer literal overflow -------------------------------------------
printf 'fn main(){ x = 99999999999999999999\n print(x) }\n' > "$TMP/k8.wyn"
expect_check_error "K8 oversized int literal rejected" "$TMP/k8.wyn" "too large for a 64-bit int"

# --- K9: non-string map index / non-int array index -------------------------
printf 'fn main(){ m = {"a":1}\n print(m[3]) }\n' > "$TMP/k9_map.wyn"
expect_check_error "K9 non-string map index rejected" "$TMP/k9_map.wyn" "Map index must be string"

# --- K10: filter predicate must return bool ---------------------------------
printf 'fn main(){ xs=[1,2,3]\n ys = xs.filter(fn(n:int)->string{return "x"})\n print(ys.len()) }\n' > "$TMP/k10.wyn"
expect_check_error "K10 non-bool filter predicate rejected" "$TMP/k10.wyn" "predicate must return bool"

# --- K4: unwrap_or default must match the wrapped type ----------------------
# A string default on an int? emitted OptionInt_unwrap_or(g(), "x") - the string
# pointer was printed as an int on the None path (silent garbage).
printf 'fn g() -> int? { return 5 }\nfn main(){ x = g().unwrap_or("hello")\n print(x) }\n' > "$TMP/k4.wyn"
expect_check_error "K4 unwrap_or default type mismatch rejected" "$TMP/k4.wyn" "unwrap_or default is string but the value is int"

# --- K3: a match EXPRESSION on a scalar needs a wildcard --------------------
# Non-exhaustive scalar match in expr position had no fall-through arm -> silent
# 0 / (null) / ICE. int-literal, `none`-inferred Option, and builtin-namespace
# enum (Color) all land here.
printf 'fn main(){ n = 5\n y = match n { 99 => 1 }\n print(y) }\n' > "$TMP/k3_int.wyn"
expect_check_error "K3 non-exhaustive int match rejected" "$TMP/k3_int.wyn" "must end with a wildcard"
printf 'fn main(){ x = none\n y = match x { Some(v) => v }\n print(y) }\n' > "$TMP/k3_opt.wyn"
expect_check_error "K3 non-exhaustive none-match rejected" "$TMP/k3_opt.wyn" "non-exhaustive match"
printf 'enum Color { Red, Green, Blue }\nfn main(){ c = Color.Blue\n y = match c { Color.Red => 1 }\n print(y) }\n' > "$TMP/k3_color.wyn"
expect_check_error "K3 builtin-namespace enum match rejected" "$TMP/k3_color.wyn" "non-exhaustive match"

# ============================================================================
# POSITIVE guards: legitimate programs that must still type-check (no over-reject)
# ============================================================================

# Valid struct init, correct fields.
printf 'struct P { x: int, y: int }\nfn main(){ p = P{x:1,y:2}\n print(p.x) }\n' > "$TMP/ok_struct.wyn"
expect_check_ok "valid struct init still ok" "$TMP/ok_struct.wyn"

# `?` on a real Result still ok.
printf 'fn parse(s: string) -> ResultInt {\n if s == "42" { return ResultInt_Ok(42) }\n return ResultInt_Err("bad")\n}\nfn use(s: string) -> ResultInt {\n n = parse(s)?\n return ResultInt_Ok(n * 2)\n}\nfn main(){ print("ok") }\n' > "$TMP/ok_try.wyn"
expect_check_ok "? on Result still ok" "$TMP/ok_try.wyn"

# A user function that shares a stdlib builtin name must still be callable (K7
# must not reject globally-registered callables or user shadows).
printf 'fn clamp(v: int, lo: int, hi: int) -> int {\n if v < lo { return lo }\n if v > hi { return hi }\n return v\n}\nfn main(){ print(clamp(5, 0, 10)) }\n' > "$TMP/ok_shadow.wyn"
expect_check_ok "user fn shadowing a builtin name still callable" "$TMP/ok_shadow.wyn"

# unwrap_or with a matching default is fine; int<->float default coerces (K4
# must not over-reject a numeric default).
printf 'fn g() -> int? { return 5 }\nfn main(){ print(g().unwrap_or(-1)) }\n' > "$TMP/ok_unwrap.wyn"
expect_check_ok "unwrap_or matching default still ok" "$TMP/ok_unwrap.wyn"
printf 'fn g() -> float? { return 1.5 }\nfn main(){ print(g().unwrap_or(0)) }\n' > "$TMP/ok_unwrap_coerce.wyn"
expect_check_ok "unwrap_or int default on float? coerces" "$TMP/ok_unwrap_coerce.wyn"

# A scalar match WITH a wildcard is fine; a bool match without one is exhaustive.
printf 'fn main(){ n = 5\n y = match n { 99 => 1, _ => 0 }\n print(y) }\n' > "$TMP/ok_int_wc.wyn"
expect_check_ok "int match with wildcard still ok" "$TMP/ok_int_wc.wyn"
printf 'fn main(){ b = true\n y = match b { true => 1, false => 0 }\n print(y) }\n' > "$TMP/ok_bool_match.wyn"
expect_check_ok "exhaustive bool match still ok" "$TMP/ok_bool_match.wyn"
# A well-typed Option match (Some+None) must still pass without a wildcard.
printf 'fn g() -> int? { return 5 }\nfn main(){ y = match g() { Some(v) => v, None => -1 }\n print(y) }\n' > "$TMP/ok_opt_match.wyn"
expect_check_ok "exhaustive Some+None match still ok" "$TMP/ok_opt_match.wyn"

echo ""; echo "checker-soundness: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
