#!/bin/bash
# A payload-free enum VALUE is an int, not a struct - and a match may yield a struct
# (2026-08).
#
# Two defects in how enum/struct VALUES are represented, both found writing one sample
# app (a mobile counter whose core is `apply(state, Action) -> State`).
#
# 1. AN ARRAY OF PAYLOAD-FREE ENUM VALUES COLLAPSED TO ITS FIRST ELEMENT.
#
#        var xs = [A.Up, A.Down, A.Reset]
#        for a in xs { print(name(a)) }      // up, up, up
#
#    The array-literal path pushed ANY TYPE_ENUM element with array_push_struct,
#    contradicting its own comment ("Data-enum value (a tagged-union struct)"). A
#    payload-free variant is a C enum CONSTANT - an int - so the macro's
#    `EnumType __temp_val = (value)` + memcpy read the wrong bytes and every element
#    came back as the first variant. SILENTLY, AT EXIT 0, in both the for-in and the
#    indexed form. A list of plain enum values is how you script a sequence of actions
#    or states, so this quietly corrupted exactly the data it was handed. Fixed with
#    is_data_enum_type(), the predicate the for-in path already uses for this
#    distinction.
#
# 2. A MATCH THAT YIELDS A STRUCT DID NOT COMPILE.
#
#        fn apply(c: C, a: A) -> C => match a {
#            A.Up   => C { v: c.v + 1 },
#            A.Down => C { v: c.v - 1 }
#        }
#
#    The arms correctly emitted `(C){.v = ...}` but the result temporary stayed
#    `long long`, so every arm failed with "assigning to 'long long' from incompatible
#    type 'C'". The result-type chain covered string/float/bool and not struct. `wyn
#    check` passed, so this surfaced only as "internal codegen error".
#
#    Reduce-over-a-sum-type - one total function from state plus an action to new
#    state - is the most useful shape there is for modelling app or UI logic, and it
#    could not be written at all.
#
# They ship together because defect 1 is what makes defect 2 observable: the scripted
# action list is what feeds the struct-returning match.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# ---- 1: an array of payload-free enum values keeps its elements -------------

cat > "$TMP/a.wyn" <<'EOF'
enum A { Up, Down, Reset }

fn name(a: A) -> string => match a {
    A.Up => "up",
    A.Down => "down",
    A.Reset => "reset"
}

fn main() {
    var xs = [A.Up, A.Down, A.Reset, A.Down]
    for a in xs { print("loop ${name(a)}") }
    // the indexed form is a separate read path
    for i in 0..xs.len() { print("idx ${name(xs[i])}") }
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run a.wyn 2>&1); code=$?
want='loop up
loop down
loop reset
loop down
idx up
idx down
idx reset
idx down'
got=$(printf '%s' "$out" | grep -E '^(loop|idx) ')
if [ $code -eq 0 ] && [ "$got" = "$want" ]; then
  ok "an array of payload-free enum values keeps every element distinct"
else
  bad "wrong output"; printf '%s\n' "$got" | head -9 | sed 's/^/        /'
fi

# The failure mode named exactly: all-identical output is the collapse.
if [ "$(printf '%s' "$out" | grep -c '^loop up$')" -gt 1 ]; then
  bad "every element collapsed to the FIRST variant again"
else
  ok "no collapse to the first variant"
fi

# ---- 2: a match may yield a struct -----------------------------------------

cat > "$TMP/b.wyn" <<'EOF'
enum A { Up, Down }
struct C { v: int }

// expression body
fn ex(c: C, a: A) -> C => match a {
    A.Up => C { v: c.v + 1 },
    A.Down => C { v: c.v - 1 }
}

// and an explicit `return match`
fn blk(c: C, a: A) -> C {
    return match a {
        A.Up => C { v: c.v + 10 },
        A.Down => C { v: c.v - 10 }
    }
}

fn main() {
    var c = C { v: 5 }
    var d = ex(c, A.Up)
    var e = ex(c, A.Down)
    var f = blk(c, A.Up)
    print("ex ${d.v} ${e.v} blk ${f.v}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" build b.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then
  ok "a struct-yielding match BUILDS"
else
  bad "build failed"; echo "$out" | grep -E 'error:' | head -3 | sed 's/^/        /'
fi
if echo "$out" | grep -q "incompatible type"; then
  bad "the result temporary was long long again"
else
  ok "no incompatible-type error"
fi

out=$(cd "$TMP" && "$WYN_ABS" run b.wyn 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q '^ex 6 4 blk 15$'; then
  ok "and every arm returns the right struct"
else
  bad "wrong struct value"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

# ---- the two together: reduce over a sum type ------------------------------
# The shape the whole fix exists for - fold a scripted action list through a
# struct-returning match. This is what the sample app does.
cat > "$TMP/c.wyn" <<'EOF'
enum Action { Inc, Dec, Reset }
struct S { v: int, taps: int }

fn apply(s: S, a: Action) -> S => match a {
    Action.Inc => S { v: s.v + 1, taps: s.taps + 1 },
    Action.Dec => S { v: s.v - 1, taps: s.taps + 1 },
    Action.Reset => S { v: 0, taps: s.taps + 1 }
}

fn main() {
    var script = [Action.Inc, Action.Inc, Action.Inc, Action.Dec, Action.Reset, Action.Inc]
    var s = S { v: 0, taps: 0 }
    for a in script { s = apply(s, a) }
    print("v=${s.v} taps=${s.taps}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run c.wyn 2>&1); code=$?
# Inc,Inc,Inc,Dec -> 2 ; Reset -> 0 ; Inc -> 1. Six taps.
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q '^v=1 taps=6$'; then
  ok "folding a scripted action list through a struct-returning match is correct"
else
  bad "the fold gave the wrong answer"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

# ---- and DATA-carrying enums are untouched ---------------------------------
# Defect 1's fix narrows a guard, so a data enum - which genuinely IS a
# tagged-union struct and must still be pushed by value - is the control group.
cat > "$TMP/d.wyn" <<'EOF'
enum Shape { Circle(int), Rect(int, int) }

fn area(s: Shape) -> int => match s {
    Shape.Circle(r) => 3 * r * r,
    Shape.Rect(w, h) => w * h
}

fn main() {
    var shapes = [Shape.Circle(2), Shape.Rect(3, 4), Shape.Circle(1)]
    for s in shapes { print("area ${area(s)}") }
    // a data enum stored in a variable, then in an array
    var one = Shape.Rect(2, 5)
    var more = [one, Shape.Circle(3)]
    for s in more { print("more ${area(s)}") }
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run d.wyn 2>&1); code=$?
want='area 12
area 12
area 3
more 10
more 27'
got=$(printf '%s' "$out" | grep -E '^(area|more) ')
if [ $code -eq 0 ] && [ "$got" = "$want" ]; then
  ok "data-carrying enums still push by value and keep their payloads"
else
  bad "a data enum regressed"; printf '%s\n' "$got" | head -6 | sed 's/^/        /'
fi

# And a match yielding each already-supported type must be unchanged - the
# result-type chain is what defect 2 touched.
cat > "$TMP/e.wyn" <<'EOF'
enum K { A, B }
fn s(k: K) -> string => match k { K.A => "sa", K.B => "sb" }
fn i(k: K) -> int => match k { K.A => 1, K.B => 2 }
fn f(k: K) -> float => match k { K.A => 1.5, K.B => 2.5 }
fn b(k: K) -> bool => match k { K.A => true, K.B => false }
fn main() {
    print("s ${s(K.B)}")
    print("i ${i(K.B)}")
    print("f ${f(K.B)}")
    print("b ${b(K.B)}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run e.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^s sb$' &&
   printf '%s' "$out" | grep -q '^i 2$' &&
   printf '%s' "$out" | grep -qE '^f 2\.5$' &&
   printf '%s' "$out" | grep -q '^b false$'; then
  ok "a match yielding string/int/float/bool is unaffected"
else
  bad "an existing match result type changed"; printf '%s\n' "$out" | head -5 | sed 's/^/        /'
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "enum-value-repr: $PASS pass, 0 fail"
  exit 0
fi
echo "enum-value-repr: $PASS pass, $FAIL fail"
exit 1
