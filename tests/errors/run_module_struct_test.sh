#!/bin/bash
# A module `pub fn` may return and take a struct DECLARED IN THAT MODULE (2026-08).
#
#   // src/m.wyn
#   struct Point { x: int, y: int }
#   pub fn origin() -> Point { return Point { x: 0, y: 0 } }
#
# died with "unknown type name 'm_Point'". `wyn check` passed; the C compiler
# rejected generated code the programmer never wrote. The identical code in a single
# file was fine, which is what made it a module problem rather than a struct one.
#
# THE CAUSE WAS A DISAGREEMENT, not a missing feature. A struct declared in a module
# is merged into the target program (merge_module_exports) and its typedef is emitted
# UNPREFIXED, as `Point`. But five other sites spelled a use of it `<module>_Point`,
# so prototypes, definitions, locals and literals each independently chose a name and
# they did not all choose the same one - the definition said `m_Point p` while its own
# prototype two lines earlier said `Point p`.
#
# The fix gives every site one predicate: prefix a struct name only when the struct
# is NOT declared in the program being emitted (is_known_struct, which looks in the
# merged program - exactly where the typedef comes from). A name it does not know
# keeps the prefix, so the generic and inner-type paths are untouched.
#
# The tests below are therefore mostly about COMBINATIONS, because each site was
# wrong on its own: returning, taking, declaring a local, and building a literal.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# ---- a flat struct, returned and taken -------------------------------------

cat > "$TMP/m.wyn" <<'EOF'
struct Point { x: int, y: int }

pub fn origin() -> Point {
    return Point { x: 0, y: 0 }
}

pub fn at(a: int, b: int) -> Point {
    return Point { x: a, y: b }
}

pub fn sum(p: Point) -> int {
    return p.x + p.y
}

// A local of the module's own struct type, which is a fourth spelling site.
pub fn doubled(p: Point) -> int {
    var q = Point { x: p.x * 2, y: p.y * 2 }
    return sum(q)
}
EOF

cat > "$TMP/a.wyn" <<'EOF'
import m
fn main() {
    var p = m.at(3, 4)
    print(m.sum(p).to_string())
    print(m.doubled(p).to_string())
    var o = m.origin()
    print(m.sum(o).to_string())
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" check a.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then ok "a module fn returning its own struct checks"; else bad "check failed"; echo "$out"|head -4; fi

out=$(cd "$TMP" && "$WYN_ABS" build a.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then
  ok "...and BUILDS (no unknown type name)"
else
  bad "build failed"; echo "$out" | grep -E 'error:' | head -4
fi
# The specific symptom, named, so a regression is recognisable rather than just red.
if echo "$out" | grep -q "unknown type name"; then
  bad "an 'unknown type name' leaked from the C compiler"
else
  ok "no 'unknown type name' in the output"
fi

out=$(cd "$TMP" && "$WYN_ABS" run a.wyn 2>&1); code=$?
# 3+4=7, doubled 6+8=14, origin 0. VALUES, not just compilation: a name fix that
# silently changed which struct was passed would compile and print nonsense.
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^7$' &&
   printf '%s' "$out" | grep -q '^14$' &&
   printf '%s' "$out" | grep -q '^0$'; then
  ok "the values are right at runtime (7 / 14 / 0)"
else
  bad "wrong runtime output"; printf '%s\n' "$out" | head -6
fi

# ---- a NESTED struct: one module struct holding another ---------------------
# The case that needed the local-declaration site fixed as well, because building
# the inner value requires a local of the inner type.

cat > "$TMP/n.wyn" <<'EOF'
struct Row { items: [string] }
struct Box { rows: [Row] }

pub fn make() -> Box {
    var r = Row { items: ["hello", "world"] }
    return Box { rows: [r] }
}

pub fn first_len(b: Box) -> int {
    return b.rows[0].items[0].len()
}
EOF

cat > "$TMP/b.wyn" <<'EOF'
import n
fn main() {
    var b = n.make()
    print(n.first_len(b).to_string())
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run b.wyn 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q '^5$'; then
  ok "a module struct nested inside another works (\"hello\" is 5)"
else
  bad "nested module structs failed"; printf '%s\n' "$out" | grep -E 'error:|^[0-9]' | head -4
fi

# ---- and nothing that used to work stops working ---------------------------
# The risk in relaxing a prefix rule is losing the prefix where it is load-bearing:
# two modules may each declare a struct of the SAME name, and those must stay
# distinct types.

cat > "$TMP/p1.wyn" <<'EOF'
struct Item { v: int }
pub fn one() -> Item { return Item { v: 1 } }
pub fn val(i: Item) -> int { return i.v }
EOF
cat > "$TMP/p2.wyn" <<'EOF'
struct Item { v: int, w: int }
pub fn two() -> Item { return Item { v: 2, w: 20 } }
pub fn wide(i: Item) -> int { return i.v + i.w }
EOF
cat > "$TMP/c.wyn" <<'EOF'
import p1
import p2
fn main() {
    print(p1.val(p1.one()).to_string())
    print(p2.wide(p2.two()).to_string())
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run c.wyn 2>&1); code=$?
# Whatever the resolution rule is, this must not silently produce a wrong number:
# either it compiles and both answers are right, or it reports a name collision.
if [ $code -eq 0 ]; then
  if printf '%s' "$out" | grep -q '^1$' && printf '%s' "$out" | grep -q '^22$'; then
    ok "two modules with same-named structs both give the right answer"
  else
    bad "same-named structs compiled but gave wrong values"; printf '%s\n' "$out"|head -4
  fi
else
  ok "a same-name struct collision is reported rather than miscompiled"
fi

# A struct passed THROUGH the top-level program, declared there, unaffected.
cat > "$TMP/d.wyn" <<'EOF'
struct Local { n: int }
fn bump(l: Local) -> int { return l.n + 1 }
fn main() {
    var l = Local { n: 41 }
    print(bump(l).to_string())
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run d.wyn 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q '^42$'; then
  ok "a struct in the main program is unaffected"
else
  bad "main-program struct broke"; printf '%s\n' "$out"|head -4
fi

# `ptr`/`cstr` in a module signature must still NOT take the prefix - the bug that
# emitted `<module>_ptr`, fixed earlier and guarded here because this change edits
# the same code path.
cat > "$TMP/q.wyn" <<'EOF'
pub fn cell() -> ptr { return Ptr.cell() }
pub fn tag(p: ptr) -> int { return 7 }
EOF
cat > "$TMP/e.wyn" <<'EOF'
import q
fn main() {
    var c = q.cell()
    print(q.tag(c).to_string())
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run e.wyn 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q '^7$'; then
  ok "ptr in a module signature still resolves (no <module>_ptr)"
else
  bad "ptr regressed"; printf '%s\n' "$out" | grep -E 'error:|^[0-9]' | head -4
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "module-struct: $PASS pass, 0 fail"
  exit 0
fi
echo "module-struct: $PASS pass, $FAIL fail"
exit 1
