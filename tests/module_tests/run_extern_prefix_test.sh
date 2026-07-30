#!/bin/bash
# An `extern fn` declared inside a MODULE must be emitted and called with the C
# name the library actually exports - unprefixed. A module's own functions get a
# `<module>_` prefix so two modules can each define `parse`; an extern is the
# opposite case, because the symbol already exists in someone else's library.
#
# The declaration was always emitted correctly (`extern double sqrt(double)`);
# only the USES were prefixed, so declaration and call disagreed:
#
#     extern double sqrt(double);        // right
#     return mathlib_sqrt(...);          // wrong - no such symbol
#
# Result: any module that wrapped a C function failed to link, or reported
# "conflicting types" when some unrelated symbol happened to match the name.
# That made it impossible to put an FFI binding behind a Wyn module - which is
# the entire purpose of a binding package (gui, sqlite, http, ...).
#
# Three call forms, which took THREE different code paths and so must each be
# asserted separately - two of them were fixed by one change and the third was
# not, and only having all three caught that:
#
#   1. mod.extern_fn(x)          qualified, from outside  -> EXPR_METHOD_CALL arm
#   2. mod.pub_fn(x) -> extern   bare, inside the module  -> EXPR_CALL arm, which
#                                emits the prefix BEFORE recursing for the name
#   3. two-argument extern       guards arity handling in the same paths
#
# libm is used rather than a fake symbol so the test proves the program LINKS and
# produces correct values, not merely that it compiled. A fake extern would link-
# fail even when the naming is right, which cannot distinguish the two outcomes.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
ROOT="$(dirname "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

mkdir -p "$TMP/src"
printf '[project]\nname = "xmod"\nversion = "0.1.0"\n' > "$TMP/wyn.toml"

cat > "$TMP/src/mathlib.wyn" <<'WYN'
extern fn sqrt(x: float) -> float;
extern fn pow(b: float, e: float) -> float;

pub fn hypot2(a: float, b: float) -> float {
    return sqrt(a * a + b * b)
}
WYN

cat > "$TMP/src/main.wyn" <<'WYN'
import mathlib
fn main() {
    println(mathlib.sqrt(16.0))
    println(mathlib.hypot2(3.0, 4.0))
    println(mathlib.pow(2.0, 10.0))
}
WYN

out=$(cd "$TMP" && WYN_ROOT="$ROOT" perl -e 'alarm(60); exec @ARGV' -- "$WYN" run src/main.wyn 2>&1)
rc=$?

if [ $rc -ne 0 ]; then
    bad "module with extern fn builds and links (rc=$rc)"
    echo "$out" | grep -E 'error:|^Error' | head -3 | sed 's/^/        /'
else
    ok "module with extern fn builds and links"
fi

check_line() {
    if echo "$out" | grep -qxF "$2"; then ok "$1"
    else bad "$1 (want '$2', got: $(echo "$out" | grep -vE 'Compiled in|^$' | tr '\n' '|'))"; fi
}
check_line "qualified call to a module's extern fn"          "4.0"
check_line "module pub fn calling its own extern fn"         "5.0"
check_line "two-argument extern fn through a module"         "1024.0"

# The naming itself, checked in the generated C rather than only via behaviour:
# a value assertion would also pass if some unrelated `mathlib_sqrt` existed.
gen=$(cd "$TMP" && WYN_ROOT="$ROOT" perl -e 'alarm(60); exec @ARGV' -- "$WYN" build src/main.wyn --debug >/dev/null 2>&1; cat src/main.wyn.c 2>/dev/null)
if [ -n "$gen" ]; then
    if echo "$gen" | grep -q 'mathlib_sqrt\|mathlib_pow'; then
        bad "generated C must not prefix an extern fn"
        echo "$gen" | grep -n 'mathlib_sqrt\|mathlib_pow' | head -2 | sed 's/^/        /'
    else
        ok "generated C does not prefix an extern fn"
    fi
    # And the module's OWN functions must still BE prefixed - the fix must not
    # have turned the prefixing off wholesale.
    if echo "$gen" | grep -q 'mathlib_hypot2'; then ok "a module's own pub fn is still prefixed"
    else bad "a module's own pub fn lost its prefix (over-broad fix)"; fi
else
    bad "could not read generated C"
fi

echo ""; echo "extern-prefix: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
