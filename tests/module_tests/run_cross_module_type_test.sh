#!/bin/bash
# A `pub struct` or `pub enum` exported from a module must be usable from the
# importing file: passed, returned, and have its fields read.
#
# TWO bugs, one root cause. An imported module's public types are spliced into the
# main program as STMT_EXPORT and emitted, UNPREFIXED, by the struct/enum pass in
# codegen_program.c. The module pass in codegen_stmt.c then emitted them a SECOND
# time with current_module_prefix set. The two symptoms looked unrelated only
# because structs honour that prefix and enums do not:
#
#   enum   -> both copies named `Color`; C rejected it outright with
#             "redefinition of enumerator 'Red'". toString was emitted twice too.
#   struct -> copies named `Point` and `shapes_Point`, so it compiled past that
#             point and failed at the typedef alias instead:
#             "typedef redefinition with different types
#              ('Point' vs 'struct shapes_Point')".
#
# And separately, on the caller's side: `var p = shapes.make(3, 4)` fell through
# every branch to the `long long` default, because lookup_module_fn_return_type
# is a hardcoded table of stdlib builtins (types.c) and knows nothing about a
# user module's functions. The struct was returned correctly and assigned into an
# integer -> "initializing 'long long' with an expression of incompatible type",
# then "member reference base type 'long long' is not a structure" on `p.x`.
#
# So the enum case must be asserted too, not just the struct one. Testing only
# structs would have left the enum crash in place - the two paths differ.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
ROOT="$(dirname "$WYN")"
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

run_case() {  # $1=name $2=expected-stdout-line
    local name="$1" want="$2" out rc
    out=$(cd "$TMP" && WYN_ROOT="$ROOT" perl -e 'alarm(60); exec @ARGV' -- "$WYN" run src/main.wyn 2>&1); rc=$?
    if [ $rc -ne 0 ]; then
        bad "$name (rc=$rc)"
        echo "$out" | grep -E 'error:|^Error' | head -2 | sed 's/^/        /'
        return
    fi
    if echo "$out" | grep -qxF "$want"; then ok "$name"
    else bad "$name (want '$want', got: $(echo "$out" | grep -vE 'Compiled in|^$|Warning:' | tr '\n' '|'))"; fi
}

# --- a struct returned across a module boundary, then a field read -------
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/src"
printf '[project]\nname = "x"\nversion = "0.1.0"\n' > "$TMP/wyn.toml"
cat > "$TMP/src/shapes.wyn" <<'WYN'
pub struct Point { x: int, y: int }
pub fn make(x: int, y: int) -> Point {
    return Point { x: x, y: y }
}
WYN
cat > "$TMP/src/main.wyn" <<'WYN'
import shapes
fn main() {
    var p = shapes.make(3, 4)
    println(p.x)
}
WYN
run_case "struct returned from a module, field read" "3"

# --- the y field too, so a wrong-offset read cannot pass -----------------
cat > "$TMP/src/main.wyn" <<'WYN'
import shapes
fn main() {
    var p = shapes.make(3, 4)
    println(p.y)
}
WYN
run_case "second field reads the right offset" "4"

# --- an enum crossing the boundary --------------------------------------
TMP=$(mktemp -d)
mkdir -p "$TMP/src"
printf '[project]\nname = "y"\nversion = "0.1.0"\n' > "$TMP/wyn.toml"
cat > "$TMP/src/col.wyn" <<'WYN'
pub enum Color { Red, Green, Blue }
pub fn pick() -> Color { return Color::Red }
WYN
cat > "$TMP/src/main.wyn" <<'WYN'
import col
fn main() {
    var c = col.pick()
    println("got")
}
WYN
run_case "enum crossing a module boundary compiles" "got"

# --- a struct PASSED INTO a module fn, not only returned ----------------
TMP=$(mktemp -d)
mkdir -p "$TMP/src"
printf '[project]\nname = "z"\nversion = "0.1.0"\n' > "$TMP/wyn.toml"
cat > "$TMP/src/geo.wyn" <<'WYN'
pub struct Vec { x: int, y: int }
pub fn build(x: int, y: int) -> Vec { return Vec { x: x, y: y } }
pub fn total(v: Vec) -> int { return v.x + v.y }
WYN
cat > "$TMP/src/main.wyn" <<'WYN'
import geo
fn main() {
    var v = geo.build(20, 22)
    println(geo.total(v))
}
WYN
run_case "struct passed back into a module fn" "42"

# --- the generated C, checked directly ----------------------------------
# A value assertion alone could pass while a stray duplicate definition sat in
# the output, so check the shape as well.
TMP=$(mktemp -d)
mkdir -p "$TMP/src"
printf '[project]\nname = "g"\nversion = "0.1.0"\n' > "$TMP/wyn.toml"
cat > "$TMP/src/shapes.wyn" <<'WYN'
pub struct Point { x: int, y: int }
pub fn make(x: int, y: int) -> Point { return Point { x: x, y: y } }
WYN
cat > "$TMP/src/main.wyn" <<'WYN'
import shapes
fn main() {
    var p = shapes.make(1, 2)
    println(p.x)
}
WYN
(cd "$TMP" && WYN_ROOT="$ROOT" perl -e 'alarm(60); exec @ARGV' -- "$WYN" build src/main.wyn --debug >/dev/null 2>&1) || true
gen="$TMP/src/main.wyn.c"
if [ -f "$gen" ]; then
    n=$(grep -c '} Point;\|} shapes_Point;' "$gen" || true)
    if [ "$n" -eq 1 ]; then ok "the struct is defined exactly once"
    else bad "the struct is defined $n times (expected 1)"; fi
    # The alias is what lets a caller name it `shapes_Point`; it must survive.
    if grep -q 'typedef Point shapes_Point;' "$gen"; then ok "the module alias is still emitted"
    else bad "the module alias went missing (callers cannot name the type)"; fi
    # And the variable must NOT be an integer.
    if grep -qE '^\s*long long p = shapes_make' "$gen"; then
        bad "the variable is still typed long long"
    else ok "the variable is typed as the struct, not long long"; fi
else
    bad "could not read the generated C"
fi
rm -rf "$TMP"

echo ""; echo "cross-module-type: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
