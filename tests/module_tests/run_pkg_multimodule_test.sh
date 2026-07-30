#!/bin/bash
# A git-URL dependency may export MORE THAN ONE module. `import <name>` must find
# any module inside a declared dependency, not only the one whose name happens to
# match the package.
#
# wyn_dep_resolve() (package.c) matched the import name against dependency NAMES
# only, so the `gui` package - which ships both `gui` (bindings) and `widgets` (a
# toolkit built on them) - could export just `gui`. `import widgets` resolved to
# nothing, load_module returned NULL, and the checker therefore registered NONE of
# that module's functions.
#
# The visible symptom was nowhere near the cause, which is why this test asserts
# the SYMPTOM as well as the resolution:
#   - every call into the module defaulted to int, so a `-> string` function
#     compared against a string gave "Cannot compare int with string" on a
#     perfectly correct comparison;
#   - and codegen emitted `long long s = ;` - wrong type AND the initialiser
#     missing entirely, i.e. malformed C rather than a wrong value.
#
# Splitting one library per repo was the alternative and is worse: `widgets` is
# useless without `gui`, they version together, and it would make every
# multi-module package a multi-repo package.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
ROOT="$(dirname "$WYN")"
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT

# A local "package" with TWO modules, wired in as a dependency via a file:// git
# repo - a real clone through the real cache path, not a hand-placed directory,
# so the resolution path under test is the one that actually runs.
PKG="$TMP/pkgsrc"
mkdir -p "$PKG/src"
printf '[project]\nname = "twomod"\nversion = "0.1.0"\n' > "$PKG/wyn.toml"
cat > "$PKG/src/twomod.wyn" <<'WYN'
pub fn base() -> int => 7
WYN
# The SECOND module: named differently from the package, which is the whole point.
cat > "$PKG/src/helper.wyn" <<'WYN'
pub fn greet() -> string => "hello"
pub fn answer() -> int => 42
WYN
git -C "$PKG" init -q 2>/dev/null
git -C "$PKG" add -A 2>/dev/null
git -C "$PKG" -c user.email=t@t -c user.name=t commit -qm init 2>/dev/null

APP="$TMP/app"
mkdir -p "$APP/src"
cat > "$APP/wyn.toml" <<TOML
[project]
name = "app"
version = "0.1.0"

[dependencies]
twomod = "file://$PKG"
TOML

cat > "$APP/src/main.wyn" <<'WYN'
import twomod
import helper
fn main() {
    print("${twomod.base()}")
    print("${helper.answer()}")
    s = helper.greet()
    if s == "hello" { print("cmp-ok") } else { print("cmp-BAD") }
}
WYN

# Install the dependency the normal way.
inst=$(cd "$APP" && WYN_ROOT="$ROOT" perl -e 'alarm(120); exec @ARGV' -- "$WYN" pkg install 2>&1)
if echo "$inst" | grep -qi 'installed'; then ok "the dependency installs"
else bad "the dependency did not install"; echo "$inst" | head -3 | sed 's/^/        /'; fi

out=$(cd "$APP" && WYN_ROOT="$ROOT" perl -e 'alarm(120); exec @ARGV' -- "$WYN" run src/main.wyn 2>&1)
rc=$?

if [ $rc -ne 0 ]; then
    bad "an app importing BOTH package modules builds (rc=$rc)"
    echo "$out" | grep -E 'error:|^Error' | head -3 | sed 's/^/        /'
else
    ok "an app importing BOTH package modules builds"
fi

want() {
    if echo "$out" | grep -qxF "$2"; then ok "$1"
    else bad "$1 (want '$2', got: $(echo "$out" | grep -vE 'Compiled in|^$|Warning:' | tr '\n' '|'))"; fi
}
# The package-named module: worked before this fix, so it is the control.
want "control: the module named after the package resolves" "7"
# THE DISCRIMINATING CASES - a second module inside the same package.
want "a SECOND module in the same package resolves"         "42"
want "and its string return keeps its type"                 "cmp-ok"

# The malformed-C symptom, checked directly: `long long s = ;` had no initialiser,
# so a value assertion alone could pass while the emitted C was still wrong.
(cd "$APP" && WYN_ROOT="$ROOT" perl -e 'alarm(120); exec @ARGV' -- "$WYN" build src/main.wyn --debug -o "$TMP/app.out" >/dev/null 2>&1) || true
gen="$APP/src/main.wyn.c"
if [ -f "$gen" ]; then
    if grep -qE '= *;' "$gen"; then
        bad "generated C has an assignment with no initialiser"
        grep -nE '= *;' "$gen" | head -2 | sed 's/^/        /'
    else ok "no empty initialisers in the generated C"; fi
    if grep -qE '^\s*long long s = ' "$gen"; then
        bad "the string variable is still typed long long"
    else ok "the string variable is not typed long long"; fi
else
    bad "could not read the generated C"
fi

# A module that genuinely does not exist must STILL report not-found, rather than
# being silently attributed to some dependency by the new search.
cat > "$APP/src/main.wyn" <<'WYN'
import twomod
import nosuchmodule
fn main() { print("${nosuchmodule.thing()}") }
WYN
bad_out=$(cd "$APP" && WYN_ROOT="$ROOT" perl -e 'alarm(120); exec @ARGV' -- "$WYN" run src/main.wyn 2>&1)
if [ $? -ne 0 ]; then ok "a genuinely missing module still fails"
else bad "a missing module was silently accepted"; fi

echo ""; echo "pkg-multimodule: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
