#!/bin/bash
# Inferred local-variable types do not leak between functions (2026-08).
#
#     // lib.wyn
#     var x = make()        // make() returns a struct/enum; x -> that type
#
#     // main.wyn (importer)
#     fn describe(x: int) -> string => "${x.to_string()}"   // a DIFFERENT x, an int
#
# lowered `x.to_string()` to `Ui_toString` and failed to compile - or, for a float,
# SILENTLY PRODUCED A WRONG VALUE. Codegen tracked "which struct/enum type does the
# variable named X hold" in a map keyed by NAME across the whole compile, with no per-
# function scope. So a `var x = ...` in one function set the type of every later `x`,
# including an unrelated parameter in another file.
#
# This forced a naming discipline across the gui, wyncanvas and wynjs codebases that no
# error message pointed at (gui/src/widgets.wyn:1636 records that it "cost a working
# designer.wyn to find"). The float variant was worse than the struct one: it compiled
# and returned the wrong number.
#
# The struct-var and enum-var maps are now saved at each function's entry and truncated at
# its exit - the same scoping already done for method/impl bodies - in both the module
# function emitter and the top-level one.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# ---- struct type does not leak across the module boundary ------------------

cat > "$TMP/w.wyn" <<'EOF'
pub struct Ui { n: int }
pub fn make() -> Ui => Ui { n: 5 }
pub fn build() -> int {
    var x = make()      // x is a Ui INSIDE this module
    return x.n
}
EOF
cat > "$TMP/m.wyn" <<'EOF'
import { build } from w
fn describe(x: int) -> string {
    // This x is an int PARAMETER, unrelated to the module's x. Its .to_string()
    // must be the int method, not Ui_toString.
    return "value=${x.to_string()}"
}
fn main() {
    print("build ${build()}")
    print(describe(42))
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" build m.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then
  ok "a same-named var in an imported module does not retype the importer's param"
else
  bad "build failed"; printf '%s\n' "$out" | grep -E 'error:|toString|unknown method' | head -3 | sed 's/^/        /'
fi
if printf '%s' "$out" | grep -qi 'Ui_toString\|Ui.toString'; then
  bad "the int param's method still lowered to Ui_toString"
else
  ok "no Ui_toString leak"
fi
out=$(cd "$TMP" && "$WYN_ABS" run m.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^build 5$' &&
   printf '%s' "$out" | grep -q '^value=42$'; then
  ok "and both evaluate correctly"
else
  bad "wrong output"; printf '%s\n' "$out" | grep -E '^(build|value)' | head -3 | sed 's/^/        /'
fi

# ---- the FLOAT variant, which was silently WRONG ---------------------------
# This is the dangerous case: it compiled either way, so only the VALUE reveals the bug.
cat > "$TMP/vlib.wyn" <<'EOF'
pub struct Vec { x: float }
pub fn mk() -> Vec => Vec { x: 1.5 }
pub fn use_it() -> float {
    var d = mk()
    return d.x
}
EOF
cat > "$TMP/vm.wyn" <<'EOF'
import { use_it } from vlib
fn scale(d: float) -> float { return d * 2.0 }
fn main() {
    print("lib ${use_it()}")
    print("scale ${scale(3.0)}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run vm.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -qE '^lib 1\.5$' &&
   printf '%s' "$out" | grep -qE '^scale 6(\.0)?$'; then
  ok "a struct var named d in a module does not corrupt a float param d elsewhere"
else
  bad "the float variant is wrong"; printf '%s\n' "$out" | grep -E '^(lib|scale)' | head -3 | sed 's/^/        /'
fi

# ---- same hazard WITHIN one file, across two functions ---------------------
# Not module-specific: two functions in the same file, one with `var p = Struct{}` and a
# later one with an int parameter p.
cat > "$TMP/one.wyn" <<'EOF'
struct Point { v: int }
fn first() -> int {
    var p = Point { v: 7 }
    return p.v
}
fn second(p: int) -> string {
    return "p=${p.to_string()}"
}
fn main() {
    print("first ${first()}")
    print(second(99))
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run one.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^first 7$' &&
   printf '%s' "$out" | grep -q '^p=99$'; then
  ok "a struct var in one function does not retype a param of the same name in another"
else
  bad "in-file leak"; printf '%s\n' "$out" | grep -E '^(first|p=)' | head -3 | sed 's/^/        /'
fi

# ---- and a real enum var still resolves its own methods --------------------
# The fix scopes the map; it must not stop a genuine enum var from finding its toString.
cat > "$TMP/en.wyn" <<'EOF'
enum Dir { North, South }
fn name(d: Dir) -> string => match d { Dir.North => "N", Dir.South => "S" }
fn main() {
    var d = Dir.North
    print("d ${name(d)}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run en.wyn 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q '^d N$'; then
  ok "a genuine enum variable still works after scoping"
else
  bad "enum var regressed"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "var-type-scope: $PASS pass, 0 fail"
  exit 0
fi
echo "var-type-scope: $PASS pass, $FAIL fail"
exit 1
