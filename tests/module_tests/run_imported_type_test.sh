#!/bin/bash
# When a module fn is declared `-> SomeStruct`, the CHECKER must record that struct
# as the return type - not fall back to int.
#
# imported_type_from_expr() (checker.c ~:7891) recognised only the six builtin type
# NAMES and returned builtin_int for anything else, so `pub fn make() -> Box`
# registered as returning int. `var b = lib.make()` then typed `b` as int, and every
# field access on it degraded to int too.
#
# #229 taught CODEGEN to emit the right C type for such a return
# (get_module_fn_struct_return), which is why the generated C declaration looked
# correct while the checker's own type stayed wrong. This fixes the checker, so both
# agree.
#
# NOTE ON SCOPE, measured rather than assumed: this does NOT fix indexing an ARRAY
# field of an imported struct (`b.items[0].label`), which has a second, independent
# cause further downstream and still fails. The test asserts only what actually
# works, so it cannot quietly start "passing" for the wrong reason later.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
ROOT="$(dirname "$WYN")"
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/src"
printf '[project]\nname = "t"\nversion = "0.1.0"\n' > "$TMP/wyn.toml"

cat > "$TMP/src/lib.wyn" <<'WYN'
pub struct Cfg { name: string, n: int }
pub enum Mode { Fast, Slow }
pub fn load() -> Cfg { return Cfg { name: "hello", n: 3 } }
pub fn mode() -> Mode { return Mode::Fast }
// A module's own extern fn returning a string. libm's strerror(0) is a real
// symbol, so this proves the LINK as well as the naming.
// getenv is real libc, always linked, and returns char* - the exact shape that
// broke. The test sets WYN_TEST_MARKER before running.
extern fn getenv(name: string) -> string;
pub fn marker() -> string { return getenv("WYN_TEST_MARKER") }
pub fn tag() -> string { return "wyn" }
WYN

check() {  # $1=name $2=body $3=expected
    printf 'import lib\nfn main() {\n%s\n}\n' "$2" > "$TMP/src/main.wyn"
    local out rc
    out=$(cd "$TMP" && WYN_TEST_MARKER=marker-ok WYN_ROOT="$ROOT" perl -e 'alarm(60); exec @ARGV' -- "$WYN" run src/main.wyn 2>&1); rc=$?
    if [ $rc -ne 0 ]; then
        bad "$1 (rc=$rc)"; echo "$out" | grep -E 'error:|^Error' | head -2 | sed 's/^/        /'; return
    fi
    if echo "$out" | grep -qxF "$3"; then ok "$1"
    else bad "$1 (want '$3', got: $(echo "$out" | grep -vE 'Compiled in|^$|Warning:' | tr '\n' '|'))"; fi
}

# THE DISCRIMINATING CASE. Binding the imported struct to a second variable is
# what forces the checker's own type to be right: with the fix reverted this is
# the ONLY behavioural case that fails, because the other spellings are rescued
# downstream by #229's codegen-side type. Mutation-tested - do not "simplify"
# this into c.name, which passes either way.
check "imported struct bound to another variable" '    var c = lib.load()
    var d = c
    println(d.name)' "hello"

# CONTROLS - these pass with the fix reverted too, because codegen recovers the
# type for a DIRECT field read. They are here to catch a regression in that path,
# not as evidence for this fix. Labelled so the distinction is not lost.
check "control: direct string field read"   '    var c = lib.load()
    println(c.name)' "hello"

check "control: direct int field read"      '    var c = lib.load()
    println(c.n)' "3"

check "control: imported enum return"       '    var m = lib.mode()
    println("ok")' "ok"

# The generated C must declare the variable as the struct, not an integer.
printf 'import lib\nfn main() {\n    var c = lib.load()\n    println(c.name)\n}\n' > "$TMP/src/main.wyn"
(cd "$TMP" && WYN_ROOT="$ROOT" perl -e 'alarm(60); exec @ARGV' -- "$WYN" build src/main.wyn --debug >/dev/null 2>&1) || true
if [ -f "$TMP/src/main.wyn.c" ]; then
    if grep -qE '^\s*long long c = lib_load' "$TMP/src/main.wyn.c"; then
        bad "the variable is still typed long long"
    else ok "the variable is typed as the struct, not long long"; fi
else
    bad "could not read the generated C"
fi

# A string-returning module fn BOUND TO A VARIABLE. This was a silent wrong
# answer: the direct call printed correctly because the call site uses the C
# function's own return type, but the VARIABLE fell back to long long, so a
# `const char*` was stored in an integer and printed as a decimal pointer
# (e.g. 4370151551) at exit 0. Found while building the gui package.
# THE DISCRIMINATING CASE: calling a module's `extern fn` DIRECTLY
# (`lib.getenv(...)`, not via a pub fn wrapper) and binding the result.
#
# Mutation-tested, and the shape matters precisely: with the fix reverted, a
# `pub fn` wrapper around the same extern still types correctly (another path
# handles it) - only the direct extern call falls to the `long long` default and
# prints the pointer as a decimal. That is exactly `gui.Win_backend_name()`,
# which is how this was found.
check "string-returning module EXTERN called DIRECTLY, bound to a variable" '    var e = lib.getenv("WYN_TEST_MARKER")
    println(e)' "marker-ok"

check "control: pub fn returning a string"             '    var t = lib.tag()
    println(t)' "wyn"

echo ""; echo "imported-type: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
