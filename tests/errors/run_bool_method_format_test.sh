#!/bin/bash
# A bool-returning array method PRINTS as true/false, not 1/0 (2026-08).
#
#     var ns = [5, 3, 9]
#     print(ns.contains(3))              // printed `1`
#     print("${ns.any((n) => n > 8)}")   // printed `1`
#
# while the SAME value routed through a variable printed `true`:
#
#     var c = ns.contains(3)
#     print("${c}")                      // true
#
# print()/to_string() dispatch on the C type via _Generic. The runtime helpers
# (`arr_contains`, `array_contains_str`, `wyn_arr_any`, `wyn_arr_all`) are declared
# `int`/`long long`, so a direct call took the INTEGER branch. Assigning to a variable
# worked only because the variable was independently declared `bool`.
#
# `contains` is one of the most-reached-for methods in the language, and a predicate is
# most naturally written inline - so the wrong spelling was the shorter, more obvious
# one, and it printed a C-ism into user output. Same class of store/load disagreement
# as the codegen type-selection defects: two sites decide a type and disagree.
#
# The fix casts at the emit site - `(bool)arr_contains(...)` - which is exactly what the
# comparison operators already do (see _is_bool_op in codegen_expr.c). It changes the
# TYPE the C expression has, not the value, so truthiness in conditions is unaffected.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# ---- the exact shapes that printed 1/0 -------------------------------------

cat > "$TMP/a.wyn" <<'EOF'
fn main() {
    var ns = [5, 3, 9]
    var ss = ["a", "b"]
    // bare print of the call
    print(ns.contains(3))
    print(ns.any((n) => n > 8))
    print(ns.all((n) => n > 100))
    // and inside an interpolation
    print("contains ${ns.contains(3)}")
    print("missing ${ns.contains(42)}")
    print("any ${ns.any((n) => n > 8)}")
    print("all ${ns.all((n) => n > 100)}")
    // a string-element array uses a DIFFERENT helper (array_contains_str)
    print("str ${ss.contains("b")}")
    print("strmissing ${ss.contains("z")}")
}
EOF

out=$(cd "$TMP" && "$WYN_ABS" run a.wyn 2>&1); code=$?
want='true
true
false
contains true
missing false
any true
all false
str true
strmissing false'
got=$(printf '%s' "$out" | grep -vE 'Compiled|Warning')
if [ $code -eq 0 ] && [ "$got" = "$want" ]; then
  ok "contains/any/all print true/false, bare and interpolated, int and string arrays"
else
  bad "wrong output"; printf '%s\n' "$got" | head -10 | sed 's/^/        /'
fi

# The failure mode named exactly: a bare 1 or 0 on its own line is the C-ism leaking.
if printf '%s' "$out" | grep -qE '^(1|0)$|[a-z] (1|0)$'; then
  bad "a 1/0 leaked into user output again"
else
  ok "no 1/0 in the output"
fi

# ---- the variable spelling, which always worked, must still agree ----------
# The point of the fix is that the two spellings AGREE. Assert them side by side so a
# future change that regresses either one is caught.
cat > "$TMP/b.wyn" <<'EOF'
fn main() {
    var ns = [5, 3, 9]
    var viaVar = ns.contains(3)
    print("var ${viaVar} direct ${ns.contains(3)}")
    var anyVar = ns.any((n) => n > 8)
    print("var ${anyVar} direct ${ns.any((n) => n > 8)}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run b.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   [ "$(printf '%s' "$out" | grep -c '^var true direct true$')" = "2" ]; then
  ok "the variable and direct spellings now agree"
else
  bad "the two spellings still disagree"; printf '%s\n' "$out" | head -4 | sed 's/^/        /'
fi

# ---- truthiness in conditions is UNAFFECTED --------------------------------
# The cast changes the C type, not the value. Every control-flow use of these methods
# must behave exactly as before - this is the control group for the whole change.
cat > "$TMP/c.wyn" <<'EOF'
fn main() {
    var ns = [5, 3, 9]
    var ss = ["a", "b"]
    if ns.contains(3) { print("if-contains") }
    if not ns.contains(42) { print("if-not-missing") }
    if ns.any((n) => n > 8) { print("if-any") }
    if not ns.all((n) => n > 100) { print("if-not-all") }
    if ss.contains("b") { print("if-str") }
    if ns.contains(3) and ns.any((n) => n > 8) { print("if-and") }
    if ns.contains(42) or ns.contains(3) { print("if-or") }
    // in a while, and as a value assigned then branched on
    var i = 0
    while ns.contains(3) and i < 2 { i += 1 }
    print("while ${i}")
    var flag = ns.contains(9)
    if flag { print("via-var") }
    // arithmetic on the result must still work (a bool is still 0/1 in C)
    var n = 0
    if ns.contains(3) { n += 1 }
    if ns.contains(42) { n += 10 }
    print("count ${n}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run c.wyn 2>&1); code=$?
for tag in if-contains if-not-missing if-any if-not-all if-str if-and if-or via-var; do
  if ! printf '%s' "$out" | grep -q "^${tag}\$"; then
    bad "condition '$tag' did not fire"; FAILED_COND=1
  fi
done
if [ $code -eq 0 ] && [ -z "${FAILED_COND:-}" ] &&
   printf '%s' "$out" | grep -q '^while 2$' &&
   printf '%s' "$out" | grep -q '^count 1$'; then
  ok "every condition, while-loop and arithmetic use behaves exactly as before"
else
  bad "a control-flow use changed"; printf '%s\n' "$out" | head -12 | sed 's/^/        /'
fi

# ---- and bools from OTHER sources are untouched ----------------------------
# These already printed true/false; the fix must not have disturbed them.
cat > "$TMP/d.wyn" <<'EOF'
fn big(n: int) -> bool { return n > 8 }
fn main() {
    print("cmp ${3 > 2}")
    print("fn ${big(9)}")
    print("lit ${true}")
    var s = "hello"
    print("strmethod ${s.contains("ell")}")
    print("and ${true and false}")
    print("not ${not true}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run d.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^cmp true$' &&
   printf '%s' "$out" | grep -q '^fn true$' &&
   printf '%s' "$out" | grep -q '^lit true$' &&
   printf '%s' "$out" | grep -q '^strmethod true$' &&
   printf '%s' "$out" | grep -q '^and false$' &&
   printf '%s' "$out" | grep -q '^not false$'; then
  ok "comparisons, fn returns, literals and string methods unaffected"
else
  bad "an already-correct bool source changed"; printf '%s\n' "$out" | head -7 | sed 's/^/        /'
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "bool-method-format: $PASS pass, 0 fail"
  exit 0
fi
echo "bool-method-format: $PASS pass, $FAIL fail"
exit 1
