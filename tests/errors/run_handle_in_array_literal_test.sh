#!/bin/bash
# A HashMap (or any handle) inside an ARRAY LITERAL compiles on Linux too (2026-08).
#
#     var m = HashMap.new()
#     var xs = [m]
#
# emitted `array_push_int(&__arr_1, m)` with NO CAST. array_push_int takes a long long
# and a HashMap is a pointer in C, so gcc rejects it outright:
#
#     error: passing argument 2 of 'array_push_int' makes integer from pointer
#            without a cast [-Wint-conversion]
#
# clang only WARNS, so this shipped: macOS was green while `make test` on Linux failed
# two regression tests (test_hashmap_in_array, test_function_arrays) - 252 pass / 2 fail
# there versus 254 / 0 on macOS. Found while verifying an unrelated deletion in a Linux
# container, which is the only reason it surfaced at all.
#
# The `.push()` spelling ALREADY cast correctly:
#
#     maps.push(m1)   ->  array_push(&(maps), (long long)(m1))     // fine
#     var xs = [m1]   ->  array_push_int(&__arr_0, m1)             // broken
#
# so the two spellings of the same operation disagreed, and only one of them was
# portable. The catch-all arm of the array-literal element dispatch now casts, matching
# what every other path does.
#
# This test asserts the CAST IS PRESENT IN THE GENERATED C rather than only that the
# program runs, because on macOS it ran fine either way - a behavioural assertion here
# would have passed while Linux stayed broken.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# ---- the generated C must cast ---------------------------------------------

cat > "$TMP/a.wyn" <<'EOF'
fn main() {
    var m = HashMap.new()
    m.set("k", "v")
    var xs = [m]
    print("len ${xs.len()}")
}
EOF

out=$(cd "$TMP" && "$WYN_ABS" build a.wyn --debug 2>&1); code=$?
if [ $code -eq 0 ]; then ok "a HashMap in an array literal builds"; else
  bad "build failed"; printf '%s\n' "$out" | grep -E 'error:' | head -3 | sed 's/^/        /'; fi

if [ -f "$TMP/a.wyn.c" ]; then
  # The push of the element must be cast. An UNCAST array_push_int of a bare
  # identifier is the exact shape gcc rejects.
  if grep -qE 'array_push_int\(&__arr_[0-9]+, \(long long\)\(' "$TMP/a.wyn.c"; then
    ok "the element push is cast to (long long)"
  else
    bad "no cast in the generated C - gcc will reject this"
    grep -oE 'array_push_int\([^;]*' "$TMP/a.wyn.c" | head -2 | sed 's/^/        /'
  fi
  # And specifically: no uncast bare-identifier push survives.
  if grep -qE 'array_push_int\(&__arr_[0-9]+, [A-Za-z_][A-Za-z0-9_]*\)' "$TMP/a.wyn.c"; then
    bad "an UNCAST array_push_int of an identifier is still emitted"
  else
    ok "no uncast identifier push remains"
  fi
else
  bad "--debug produced no .c file"
fi

out=$(cd "$TMP" && "$WYN_ABS" run a.wyn 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q '^len 1$'; then
  ok "and it runs correctly"
else
  bad "wrong output"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

# ---- the shapes that must agree with each other ----------------------------
# The bug was that `.push()` and a literal disagreed. Assert both, together.
cat > "$TMP/b.wyn" <<'EOF'
fn main() {
    var m1 = HashMap.new()
    m1.set("name", "Alice")
    var m2 = HashMap.new()
    m2.set("name", "Bob")

    // via push
    var viaPush = []
    viaPush.push(m1)
    viaPush.push(m2)

    // via a literal - the spelling that did not compile on Linux
    var viaLit = [m1, m2]

    print("push ${viaPush.len()} lit ${viaLit.len()}")

    // a set handle too, and a multi-element literal read back
    var s = HashSet.new()
    var sets = [s]
    print("sets ${sets.len()}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run b.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^push 2 lit 2$' &&
   printf '%s' "$out" | grep -q '^sets 1$'; then
  ok "the push and array-literal spellings agree, for HashMap and HashSet"
else
  bad "the two spellings still disagree"; printf '%s\n' "$out" | head -4 | sed 's/^/        /'
fi

# ---- and every element type that already worked is unchanged ---------------
# The fix touches the CATCH-ALL arm only, so the typed arms are the control group.
cat > "$TMP/c.wyn" <<'EOF'
fn main() {
    var ints = [1, 2, 3]
    var strs = ["a", "b"]
    var floats = [1.5, 2.5]
    var bools = [true, false]
    var si = 0
    for i in ints { si += i }
    var ss = ""
    for s in strs { ss = ss + s }
    var sf = 0.0
    for f in floats { sf = sf + f }
    print("ints ${si} strs ${ss} floats ${sf} nbools ${bools.len()}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run c.wyn 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -qE '^ints 6 strs ab floats 4(\.0)? nbools 2$'; then
  ok "int, string, float and bool literals are unaffected"
else
  bad "a typed element arm regressed"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "handle-in-array-literal: $PASS pass, 0 fail"
  exit 0
fi
echo "handle-in-array-literal: $PASS pass, $FAIL fail"
exit 1
