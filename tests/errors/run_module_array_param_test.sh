#!/bin/bash
# A module function may take an ARRAY parameter (2026-08).
#
#     // lib.wyn
#     pub fn total(xs: [int]) -> int { ... }
#
#     // main.wyn
#     import { total } from lib
#     total([1, 2, 3])
#
# failed with "compilation failed (internal codegen error)". The parameter was emitted as
# `long long`, not `WynArray`:
#
#     long long lib_total(long long xs);
#     ... error: passing 'WynArray' to parameter of incompatible type 'long long'
#
# An array annotation (`xs: [int]`) is an EXPR_ARRAY node; the module function emitter only
# handled EXPR_IDENT type annotations and fell through to the `long long` default for
# everything else. The bare token "array" WAS handled - but nothing spells the type that
# way, so it looked covered and was not. Both the prototype and the definition emitter had
# it, so they had to be fixed together or they would disagree (which is itself a C error).
#
# Found writing a game whose engine module takes the map (`grid: [int]`) as a parameter -
# the first non-trivial thing a module does.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

cat > "$TMP/lib.wyn" <<'EOF'
pub fn total(xs: [int]) -> int {
    var s = 0
    for x in xs { s = s + x }
    return s
}

pub fn first_or(xs: [int], d: int) -> int {
    if xs.len() == 0 { return d }
    return xs[0]
}

pub fn longest(rows: [string]) -> int {
    var best = 0
    for r in rows { if r.len() > best { best = r.len() } }
    return best
}

// An array both taken AND returned, so both directions are exercised.
pub fn doubled(xs: [int]) -> [int] {
    var out = []
    for x in xs { out.push(x * 2) }
    return out
}
EOF

cat > "$TMP/main.wyn" <<'EOF'
import { total, first_or, longest, doubled } from lib

fn main() {
    var xs = [3, 4, 5]
    print("sum ${total(xs)}")
    print("first ${first_or(xs, 9)}")
    var empty = []
    print("empty ${first_or(empty, 42)}")
    var words = ["a", "abcd", "ab"]
    print("longest ${longest(words)}")
    var d = doubled(xs)
    print("doubled ${total(d)}")
}
EOF

out=$(cd "$TMP" && "$WYN_ABS" build main.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then
  ok "a module function taking [int] BUILDS"
else
  bad "build failed"; printf '%s\n' "$out" | grep -E 'error:|internal' | head -4 | sed 's/^/        /'
fi
if printf '%s' "$out" | grep -q "incompatible type"; then
  bad "the array param was emitted as long long again"
else
  ok "no incompatible-type error"
fi

out=$(cd "$TMP" && "$WYN_ABS" run main.wyn 2>&1); code=$?
want='sum 12
first 3
empty 42
longest 4
doubled 24'
got=$(printf '%s' "$out" | grep -E '^(sum|first|empty|longest|doubled) ')
if [ $code -eq 0 ] && [ "$got" = "$want" ]; then
  ok "arrays cross the module boundary in both directions and compute correctly"
else
  bad "wrong output"; printf '%s\n' "$got" | head -6 | sed 's/^/        /'
fi

# The prototype and definition must AGREE on the type - checked in the generated C, since
# a disagreement is what the C compiler rejects and the source symptom is only "internal
# codegen error".
out=$(cd "$TMP" && "$WYN_ABS" build main.wyn --debug 2>&1)
if [ -f "$TMP/main.wyn.c" ]; then
  if grep -qE 'lib_total\(long long' "$TMP/main.wyn.c"; then
    bad "the array param is still 'long long' in the C"
    grep -nE 'lib_total\(' "$TMP/main.wyn.c" | head -2 | sed 's/^/        /'
  else
    ok "the array param is WynArray in both prototype and definition"
  fi
else
  bad "--debug produced no .c"
fi

# ---- controls: the parameter types that already worked --------------------
cat > "$TMP/ctl.wyn" <<'EOF'
pub fn addi(a: int, b: int) -> int => a + b
pub fn cat(a: string, b: string) -> string => a + b
pub fn half(x: float) -> float => x / 2.0
pub fn flip(b: bool) -> bool => not b
EOF
cat > "$TMP/mctl.wyn" <<'EOF'
import { addi, cat, half, flip } from ctl
fn main() {
    print("i ${addi(2, 3)}")
    print("s ${cat("ab", "cd")}")
    print("f ${half(5.0)}")
    print("b ${flip(false)}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run mctl.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^i 5$' &&
   printf '%s' "$out" | grep -q '^s abcd$' &&
   printf '%s' "$out" | grep -qE '^f 2\.5$' &&
   printf '%s' "$out" | grep -q '^b true$'; then
  ok "int/string/float/bool module params are unaffected"
else
  bad "a scalar param type regressed"; printf '%s\n' "$out" | head -5 | sed 's/^/        /'
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "module-array-param: $PASS pass, 0 fail"
  exit 0
fi
echo "module-array-param: $PASS pass, $FAIL fail"
exit 1
