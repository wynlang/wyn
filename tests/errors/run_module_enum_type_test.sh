#!/bin/bash
# A module's ENUM may appear in an exported function's signature (2026-08).
#
#     // lib.wyn
#     enum Kind { A, B }
#     pub fn kname(k: Kind) -> string => match k { Kind.A => "a", Kind.B => "b" }
#
#     // main.wyn
#     import { kname } from lib3
#
# failed with "compilation failed (internal codegen error)". The generated C declared the
# prototype with a module-prefixed TYPE name that nothing defines:
#
#     static const char* lib_kname(lib_Kind k);   // error: unknown type name 'lib_Kind'
#     static const char* lib_kname(Kind k) { ... }  // the definition said Kind
#
# Types are emitted with their BARE name (`typedef enum {...} Kind;`) while a function
# inside an imported module is emitted with the module prefix. That is right for the
# FUNCTION name and wrong for a TYPE name, so the prefixing sites consult a predicate to
# exempt types - and that predicate was is_known_struct(), which does not know about
# enums. Structs were therefore fine and enums were not.
#
# A `pub` enum happened to survive by accident: a separate pass emits
# `typedef Kind lib_Kind;` for EXPORTED enums, which rescued the mismatch. A non-pub enum
# used in a pub signature got no such alias and did not compile at all - so the bug was
# invisible exactly until you wrote a module that kept its enum private, which is the
# normal thing to do.
#
# Found writing a game whose engine module exposes `enum Cell` / `enum Kind` in its API.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# ---- the exact shape that did not compile ----------------------------------
# A PRIVATE enum used in an EXPORTED function's parameter and return type.

cat > "$TMP/eng.wyn" <<'EOF'
enum Kind { Grunt, Brute }

struct Mob { kind: Kind, hp: int }

pub fn kname(k: Kind) -> string => match k {
    Kind.Grunt => "grunt",
    Kind.Brute => "brute"
}

pub fn make_mob(k: Kind) -> Mob => Mob { kind: k, hp: 10 }

pub fn kind_of(m: Mob) -> Kind => m.kind

pub fn tougher(k: Kind) -> Kind => match k {
    Kind.Grunt => Kind.Brute,
    Kind.Brute => Kind.Brute
}
EOF

cat > "$TMP/main.wyn" <<'EOF'
import { kname, make_mob, kind_of, tougher } from eng

fn main() {
    // enum as a PARAMETER
    print("a ${kname(Kind.Grunt)}")
    // enum inside a returned STRUCT, read back out
    var m = make_mob(Kind.Brute)
    print("b ${kname(kind_of(m))} hp ${m.hp}")
    // enum as a RETURN type
    print("c ${kname(tougher(Kind.Grunt))}")
}
EOF

out=$(cd "$TMP" && "$WYN_ABS" build main.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then
  ok "a private enum in an exported signature BUILDS"
else
  bad "build failed"; printf '%s\n' "$out" | grep -E 'error:|internal' | head -4 | sed 's/^/        /'
fi
# The exact symptom, named so a regression is unmistakable.
if printf '%s' "$out" | grep -q "unknown type name"; then
  bad "an unknown module-prefixed TYPE name leaked into the C again"
else
  ok "no unknown-type-name error"
fi

out=$(cd "$TMP" && "$WYN_ABS" run main.wyn 2>&1); code=$?
want='a grunt
b brute hp 10
c brute'
got=$(printf '%s' "$out" | grep -E '^[abc] ')
if [ $code -eq 0 ] && [ "$got" = "$want" ]; then
  ok "and every enum crossing the module boundary evaluates correctly"
else
  bad "wrong output"; printf '%s\n' "$got" | head -4 | sed 's/^/        /'
fi

# The prototype and the definition must AGREE. Checked in the generated C because a
# mismatch is what the C compiler rejects, and the source-level symptom is only
# "internal codegen error".
out=$(cd "$TMP" && "$WYN_ABS" build main.wyn --debug 2>&1)
if [ -f "$TMP/main.wyn.c" ]; then
  if grep -qE '\beng_Kind\b' "$TMP/main.wyn.c"; then
    bad "the C still names eng_Kind, a type nothing declares"
    grep -nE '\beng_Kind\b' "$TMP/main.wyn.c" | head -2 | sed 's/^/        /'
  else
    ok "the generated C names the enum type consistently"
  fi
else
  bad "--debug produced no .c"
fi

# ---- a pub enum still works ------------------------------------------------
# This spelling worked BEFORE the fix (via a typedef alias emitted for exported enums),
# so it is the control: the fix must not disturb it.
cat > "$TMP/pub.wyn" <<'EOF'
pub enum Colour { Red, Blue }
pub fn cname(c: Colour) -> string => match c { Colour.Red => "red", Colour.Blue => "blue" }
EOF
cat > "$TMP/mpub.wyn" <<'EOF'
import { cname, Colour } from pub
fn main() { print("p ${cname(Colour.Blue)}") }
EOF
out=$(cd "$TMP" && "$WYN_ABS" run mpub.wyn 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q '^p blue$'; then
  ok "an exported (pub) enum is unaffected"
else
  bad "pub-enum case regressed"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

# ---- a module STRUCT is unaffected ----------------------------------------
# Structs were always exempted correctly; the fix widened that exemption, so assert the
# original half still holds.
cat > "$TMP/st.wyn" <<'EOF'
struct Point { x: int, y: int }
pub fn origin() -> Point => Point { x: 0, y: 0 }
pub fn sum(p: Point) -> int => p.x + p.y
pub fn shift(p: Point, d: int) -> Point => Point { x: p.x + d, y: p.y + d }
EOF
cat > "$TMP/mst.wyn" <<'EOF'
import { origin, sum, shift } from st
fn main() {
    var p = shift(origin(), 4)
    print("s ${sum(p)}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run mst.wyn 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q '^s 8$'; then
  ok "a module struct in a signature still works"
else
  bad "struct case regressed"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

# ---- an enum in the MAIN program is unaffected ----------------------------
cat > "$TMP/plain.wyn" <<'EOF'
enum E { X, Y }
fn nm(e: E) -> string => match e { E.X => "x", E.Y => "y" }
fn main() { print("m ${nm(E.Y)}") }
EOF
out=$(cd "$TMP" && "$WYN_ABS" run plain.wyn 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q '^m y$'; then
  ok "an enum in the main program is unaffected"
else
  bad "plain enum regressed"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "module-enum-type: $PASS pass, 0 fail"
  exit 0
fi
echo "module-enum-type: $PASS pass, $FAIL fail"
exit 1
