#!/bin/bash
# A method may be called on an INTERPOLATED string literal (2026-08).
#
#     print("  ${a}${b}".pad_right(20, " "))
#
# type-checked clean and then died in codegen with
#
#     Error: Unknown method 'pad_right' (no type info)
#
# The method-call path special-cases every receiver shape it knows - an identifier, a
# chained method, a call returning an array, a tuple index - and an interpolated literal
# was simply not among them, so the receiver type stayed NULL and dispatch fell through
# to the error. An interpolation produces a string whatever its pieces are, so there is
# nothing to infer.
#
# The same call on a PLAIN literal, or on a variable holding the interpolation, worked -
# so the effective rule was "you may not call a method on an interpolated literal",
# which is not a rule anyone can learn. Found writing a sample app that aligns a table
# column built by interpolation.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

cat > "$TMP/a.wyn" <<'EOF'
fn main() {
    var a = "x"
    var b = "y"
    var n = 7

    // The shape that failed: a method directly on an interpolated literal.
    print("[" + "${a}${b}".pad_right(6, ".") + "]")

    // With a number interpolated in, and chained with a concatenation after.
    print("[" + "n=${n}".pad_right(8, ".") + "]")

    // Other string methods on the same receiver, since the fix is about the receiver
    // rather than about pad_right.
    print("[" + "${a}${b}".upper() + "]")
    print("[" + "${a}-${b}".len().to_string() + "]")
    print("[" + "  ${a}  ".trim() + "]")
    print("[" + "${a}${b}".replace("y", "z") + "]")

    // And nested one level: a method on an interpolation that itself contains a call.
    print("[" + "v=${n.to_string()}".pad_right(8, ".") + "]")
}
EOF

out=$(cd "$TMP" && "$WYN_ABS" check a.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then ok "it type-checks"; else bad "check failed"; echo "$out"|head -4; fi

out=$(cd "$TMP" && "$WYN_ABS" build a.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then
  ok "...and BUILDS"
else
  bad "build failed"; echo "$out" | grep -E 'Error|error:' | head -4
fi
# The specific symptom, named, so a regression is recognisable rather than merely red.
if echo "$out" | grep -q "no type info"; then
  bad "'no type info' came back"
else
  ok "no 'no type info' error"
fi

out=$(cd "$TMP" && "$WYN_ABS" run a.wyn 2>&1); code=$?
want='[xy....]
[n=7.....]
[XY]
[3]
[x]
[xz]
[v=7.....]'
got=$(printf '%s' "$out" | grep '^\[')
if [ $code -eq 0 ] && [ "$got" = "$want" ]; then
  ok "every value is correct at runtime"
else
  bad "wrong output"; printf '%s\n' "$got" | head -8 | sed 's/^/        /'
fi

# The forms that already worked must keep working - the fix adds a case, it must not
# steal one.
cat > "$TMP/b.wyn" <<'EOF'
fn main() {
    var a = "x"
    // A plain literal receiver.
    print("[" + "ab".pad_right(5, ".") + "]")
    // An interpolation stored in a variable first.
    var s = "${a}y"
    print("[" + s.pad_right(5, ".") + "]")
    // A method on a call's result.
    print("[" + "z".upper().pad_right(4, ".") + "]")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run b.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^\[ab\.\.\.\]$' &&
   printf '%s' "$out" | grep -q '^\[xy\.\.\.\]$' &&
   printf '%s' "$out" | grep -q '^\[Z\.\.\.\]$'; then
  ok "plain literals, stored interpolations and call results still work"
else
  bad "a previously-working form broke"; printf '%s\n' "$out" | head -5 | sed 's/^/        /'
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "interp-method: $PASS pass, 0 fail"
  exit 0
fi
echo "interp-method: $PASS pass, $FAIL fail"
exit 1
