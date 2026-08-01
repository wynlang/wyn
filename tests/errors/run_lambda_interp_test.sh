#!/bin/bash
# A lambda may appear inside a ${} interpolation (2026-08).
#
#     print("top: ${xs.filter((n) => n > 2)}")
#
# failed with "compilation failed (internal codegen error)". The generated C referenced
# `__lambda_1` but no such function was ever emitted:
#
#     to_string(wyn_array_map(ns, __lambda_1))
#     error: use of undeclared identifier '__lambda_1'
#
# scan_expr_for_lambdas() - the pre-pass that assigns each lambda an id and emits its
# top-level C function - had no EXPR_STRING_INTERP arm, so it never descended into the
# interpolated parts. The expression emitter did, and referenced a function the scanner
# had not created. This is the SAME omission as the EXPR_STRUCT_INIT arm directly above
# it, which carries a comment describing the identical failure; every other walker in
# codegen_lambda.c (collect_idents, veto_scan_expr, veto_all_idents_in) already handled
# the node.
#
# Assigning the pipeline to a variable first always worked, which is why this survived
# so long: it reads as a rule about interpolation rather than as a missing traversal.
# `${}` is the natural place to put a one-line pipeline, so this blocked the most
# idiomatic spelling of the language's best feature.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# ---- the exact shape that failed to compile --------------------------------

cat > "$TMP/a.wyn" <<'EOF'
fn main() {
    var ns = [5, 3, 9, 1, 7]
    print("map ${ns.map((n) => n * 2)}")
    print("filter ${ns.filter((n) => n > 4)}")
    print("reduce ${ns.reduce((a, b) => a + b, 0)}")
    print("chain ${ns.filter((n) => n > 2).map((n) => n * 10)}")
}
EOF

out=$(cd "$TMP" && "$WYN_ABS" build a.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then
  ok "a lambda inside an interpolation BUILDS"
else
  bad "build failed"; echo "$out" | grep -E "error:|internal" | head -4 | sed 's/^/        /'
fi
if echo "$out" | grep -q '__lambda_'; then
  bad "an undeclared __lambda_N leaked into the generated C again"
else
  ok "no undeclared __lambda_N"
fi

out=$(cd "$TMP" && "$WYN_ABS" run a.wyn 2>&1); code=$?
want='map [10, 6, 18, 2, 14]
filter [5, 9, 7]
reduce 25
chain [50, 30, 90, 70]'
got=$(printf '%s' "$out" | grep -vE 'Compiled|Warning')
if [ $code -eq 0 ] && [ "$got" = "$want" ]; then
  ok "and every pipeline evaluates correctly"
else
  bad "wrong output"; printf '%s\n' "$got" | head -5 | sed 's/^/        /'
fi

# ---- more than one lambda per string, and per program ----------------------
# Ids are handed out by the scanner; two lambdas in ONE interpolation, and a second
# interpolation later, must each get their own function rather than collide on an id.
cat > "$TMP/b.wyn" <<'EOF'
fn main() {
    var ns = [1, 2, 3, 4, 5, 6]
    print("two ${ns.filter((n) => n % 2 == 0)} and ${ns.filter((n) => n % 2 == 1)}")
    print("again ${ns.map((n) => n + 100)}")
    var evens = ns.filter((n) => n % 2 == 0)
    print("mixed ${evens} vs ${ns.map((n) => n * 3)}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run b.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^two \[2, 4, 6\] and \[1, 3, 5\]$' &&
   printf '%s' "$out" | grep -q '^again \[101, 102, 103, 104, 105, 106\]$' &&
   printf '%s' "$out" | grep -q '^mixed \[2, 4, 6\] vs \[3, 6, 9, 12, 15, 18\]$'; then
  ok "several lambdas across and within interpolations each get their own function"
else
  bad "multi-lambda case failed"; printf '%s\n' "$out" | head -5 | sed 's/^/        /'
fi

# A lambda that CAPTURES, inside an interpolation - the capturing path emits a closure
# rather than a bare function pointer, so it is a distinct lowering.
cat > "$TMP/c.wyn" <<'EOF'
fn main() {
    var ns = [1, 2, 3]
    var k = 10
    print("cap ${ns.map((n) => n * k)}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run c.wyn 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q '^cap \[10, 20, 30\]$'; then
  ok "a capturing lambda inside an interpolation works"
else
  bad "capturing case failed"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

# ---- and the spellings that already worked are unchanged -------------------
# The fix only ADDS a traversal arm. A lambda assigned to a variable, passed to a
# function, or held in a struct field must behave exactly as before.
cat > "$TMP/d.wyn" <<'EOF'
struct Handler { name: string }

fn main() {
    var ns = [4, 5, 6]
    // the workaround spelling: pipeline into a variable, then interpolate
    var doubled = ns.map((n) => n * 2)
    print("var ${doubled}")
    // lambda in a plain (non-interpolated) statement
    var big = ns.filter((n) => n > 4)
    print("filtered")
    print("${big}")
    // an interpolation with NO lambda in it at all
    print("plain ${ns.len()} ${ns.sum()}")
    // a struct init nearby, whose own lambda arm this change sits next to
    var h = Handler { name: "ok" }
    print("struct ${h.name}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run d.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^var \[8, 10, 12\]$' &&
   printf '%s' "$out" | grep -q '^\[5, 6\]$' &&
   printf '%s' "$out" | grep -q '^plain 3 15$' &&
   printf '%s' "$out" | grep -q '^struct ok$'; then
  ok "variable, statement and non-lambda spellings all unaffected"
else
  bad "a previously-working spelling broke"; printf '%s\n' "$out" | head -6 | sed 's/^/        /'
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "lambda-interp: $PASS pass, 0 fail"
  exit 0
fi
echo "lambda-interp: $PASS pass, $FAIL fail"
exit 1
