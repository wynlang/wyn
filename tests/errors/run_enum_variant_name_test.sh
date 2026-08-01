#!/bin/bash
# An enum variant may be named anything, including a C type or keyword (2026-08).
#
#     enum Level { Info, Warn, Error }
#
# failed to BUILD with "redefinition of 'Error'": the C enum emitted its members BARE
# (`Info, Warn, Error`), and `Error` collided with the runtime type `WynError`. wyn check
# passed; the C compiler rejected the whole program. Any variant whose name is a C
# identifier already in scope - Error, and plausibly others - was a landmine, and enum
# variant names are the programmer's to choose.
#
# The fix emits members PREFIXED as `EnumName_Variant`, which is the same symbol
# `match` and `EnumName.Variant` already used via a #define - so the #define block was
# dropped as redundant and the toString switch updated to the prefixed member. This test
# guards both the collision AND that ordinary enums still behave (the change touched
# every enum's codegen).
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# ---- the collision that failed to build ------------------------------------

cat > "$TMP/a.wyn" <<'EOF'
enum Level { Info, Warn, Error }

fn tag(l: Level) -> string => match l {
    Level.Info => "INFO",
    Level.Warn => "WARN",
    Level.Error => "ERROR"
}

fn main() {
    print(tag(Level.Error))
    print(tag(Level.Info))
    var l = Level.Warn
    if l == Level.Warn { print("is warn") }
}
EOF

out=$(cd "$TMP" && "$WYN_ABS" build a.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then
  ok "an enum with a variant named Error BUILDS"
else
  bad "build failed"; echo "$out" | grep -E 'error:|redefinition' | head -4
fi
if echo "$out" | grep -qi "redefinition"; then
  bad "a redefinition leaked from the C compiler"
else
  ok "no redefinition error"
fi

out=$(cd "$TMP" && "$WYN_ABS" run a.wyn 2>&1); code=$?
want='ERROR
INFO
is warn'
got=$(printf '%s' "$out" | grep -vE 'Compiled|Warning')
if [ $code -eq 0 ] && [ "$got" = "$want" ]; then
  ok "match, EnumName.Variant and == all evaluate correctly"
else
  bad "wrong output"; printf '%s\n' "$got" | head -4 | sed 's/^/        /'
fi

# Other names that shadow C types or keywords, to prove it is not a one-off for Error.
cat > "$TMP/b.wyn" <<'EOF'
enum T { Int, Bool, Char, Void, Struct, Union, Return }
fn name(t: T) -> string => match t {
    T.Int => "int", T.Bool => "bool", T.Char => "char", T.Void => "void",
    T.Struct => "struct", T.Union => "union", T.Return => "return"
}
fn main() {
    print(name(T.Struct))
    print(name(T.Return))
    print(name(T.Void))
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run b.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^struct$' &&
   printf '%s' "$out" | grep -q '^return$' &&
   printf '%s' "$out" | grep -q '^void$'; then
  ok "variants named after C types/keywords all work"
else
  bad "a C-name variant broke"; printf '%s\n' "$out" | head -4 | sed 's/^/        /'
fi

# ---- ordinary enums still behave (the change touched every enum's codegen) --

cat > "$TMP/c.wyn" <<'EOF'
enum Dir { North, East, South, West }
fn main() {
    // ordinal via interpolation of the qualified constant
    print("${Dir.North} ${Dir.West}")
    // two enums in one program, and comparison across a variable
    var d = Dir.East
    var moved = false
    if d == Dir.East { moved = true }
    print("moved=${moved}")
    // toString-style name in a match
    var s = "?"
    match d {
        Dir.North => { s = "N" }
        Dir.East => { s = "E" }
        Dir.South => { s = "S" }
        Dir.West => { s = "W" }
    }
    print("dir=${s}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run c.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^0 3$' &&
   printf '%s' "$out" | grep -q '^moved=true$' &&
   printf '%s' "$out" | grep -q '^dir=E$'; then
  ok "ordinals, comparison and match on an ordinary enum unaffected"
else
  bad "ordinary enum behaviour changed"; printf '%s\n' "$out" | head -5 | sed 's/^/        /'
fi

# An enum WITH data (a different codegen path) must be untouched.
cat > "$TMP/d.wyn" <<'EOF'
enum Shape { Circle(int), Rect(int, int) }
fn area(s: Shape) -> int {
    return match s {
        Shape.Circle(r) => 3 * r * r,
        Shape.Rect(w, h) => w * h
    }
}
fn main() {
    print("${area(Shape.Circle(2))} ${area(Shape.Rect(3, 4))}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run d.wyn 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q '^12 12$'; then
  ok "a data-carrying enum still works"
else
  bad "data-carrying enum broke"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "enum-variant-name: $PASS pass, 0 fail"
  exit 0
fi
echo "enum-variant-name: $PASS pass, $FAIL fail"
exit 1
