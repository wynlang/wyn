#!/bin/bash
# `wyn run --release prog.wyn` must LINK and produce the SAME answers as the
# default path.
#
# It could not link at all. --release is the only mode that emits
# `#include "wyn_runtime_slim.h"` instead of the full wyn_runtime.h, and the slim
# header only DECLARES the runtime; the definitions have to come from
# runtime/libwyn_rt.a. src/runtime_exports.c is the single translation unit that
# includes wyn_runtime.h, so it is where every function defined in that header
# becomes a linkable symbol - and it was listed only in the Makefile's
# TCC_RT_SRCS, never in RT_SRCS. So the archive --release links contained none of
# them, and EVERY --release build died with
#
#   Undefined symbols: _Math_pow, _System_args, ___wyn_argc, _print_float_no_nl, ...
#
# Two more classes of defect hid behind that link failure, which is why this file
# asserts OUTPUT and not merely exit status:
#
#  1. MISSING DECLARATIONS. The slim header had drifted from wyn_runtime.h - no
#     nested-array getters for float/bool/string, no HashMap/HashSet/Json, no
#     spawn/await, no Time/Shared, no Test module. Each one is a hard compile
#     error under --release only.
#  2. DIVERGENT LOCAL COPIES, which are worse, because they COMPILE. The slim
#     header defined its own int_to_string returning a `static char buf[32]`:
#     every to_string in one expression handed back the same pointer, so
#     `print("${a} + ${b} = ${a+b}")` with a=10,b=20 printed "30 + 30 = 30"
#     under --release and "10 + 20 = 30" on the default path. Its private
#     string_replace never copied the replacement and never advanced past the
#     needle, so "Hello World".replace("World","Wyn") segfaulted. Both now
#     resolve to the archive's real definitions.
#
# THE INVARIANT THIS FILE ENCODES: --release is an optimization level, not a
# dialect. Anything the default path prints, --release must print byte for byte.
# So every case below runs the SAME program both ways and diffs the two outputs
# against each other AND against a hand-written expectation - a shared wrong
# answer would satisfy the diff alone.
#
# CACHING GOTCHA: `wyn run` reuses <file>.wyn.out when it is newer than the
# source, and the two modes write to the same path. Every case removes it before
# each run, or the second mode silently re-runs the first mode's binary and the
# test passes no matter what the compiler does.
#
# FLAG-ORDER GOTCHA: it is `wyn run --release f.wyn`. Wyn's own flags come BEFORE
# the file; `wyn build f.wyn --release` only changes -O level and still uses the
# full header, so it cannot exercise the slim runtime at all.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

n=0
# $1=name  $2=expected stdout (exact)  stdin=the program source
# Runs it default-path and --release, then requires: release links, release
# matches the expectation, and the two paths agree with each other.
check() {
    local name="$1" want="$2"
    n=$((n+1))
    local f="$TMP/p$n.wyn"
    cat > "$f"
    local base rel brc rrc
    rm -f "$TMP/p$n.wyn.out"
    base=$(cd "$TMP" && perl -e 'alarm(90); exec @ARGV' -- "$WYN" run "$f" 2>/dev/null); brc=$?
    rm -f "$TMP/p$n.wyn.out"
    rel=$(cd "$TMP" && perl -e 'alarm(90); exec @ARGV' -- "$WYN" run --release "$f" 2>/dev/null); rrc=$?
    rm -f "$TMP/p$n.wyn.out"
    if [ "$brc" -ne 0 ]; then
        bad "$name (BASELINE broke, rc=$brc - fix that first, not --release)"; return
    fi
    if [ "$rrc" -ne 0 ]; then
        bad "$name (--release failed, rc=$rrc; usually a link error - rerun with WYN_DEBUG=1)"; return
    fi
    if [ "$rel" != "$want" ]; then
        bad "$name (--release printed '$rel', want '$want')"; return
    fi
    if [ "$rel" != "$base" ]; then
        bad "$name (paths disagree: default '$base' vs --release '$rel')"; return
    fi
    ok "$name"
}

# The symbols named in the original link failure, in one program: Math_pow +
# System_args + __wyn_argc/__wyn_argv (System.args() reads them, and they are
# referenced by libwyn_rt.a's own _main) + print_float_no_nl.
check "Math.pow, System.args and a float array link and compute" "8.0
1.5
1" <<'WYN'
fn main() {
    var xs = [1.5, 2.5]
    print(Math.pow(2.0, 3.0))
    print(xs[0])
    print(System.args().len())
}
WYN

# Nested-array reads. The float/bool/string nested getters existed in
# wyn_runtime.h but had no slim declaration, so the implicit-int return
# TRUNCATED a float and reinterpreted a char* as a number.
check "nested array reads keep their element type" "f=2.5
s=b
i=20
b=true
f3=7.5
s3=z" <<'WYN'
fn main() {
    var fr: [float] = [1.5, 2.5]
    var fm: [[float]] = [fr]
    print("f=${fm[0][1]}")
    var sr: [string] = ["a", "b"]
    var sm: [[string]] = [sr]
    print("s=${sm[0][1]}")
    var ir: [int] = [10, 20]
    var im: [[int]] = [ir]
    print("i=${im[0][1]}")
    var br: [bool] = [false, true]
    var bm: [[bool]] = [br]
    print("b=${bm[0][1]}")
    var f3: [[[float]]] = [fm]
    print("f3=${f3[0][0][0] * 5.0}")
    var s3: [[[string]]] = [[["y", "z"]]]
    print("s3=${s3[0][0][1]}")
}
WYN

# argv actually reaching the program, not just System.args() linking. `--` marks
# the program's own arguments; args[0] is the program name.
n=$((n+1))
f="$TMP/argv.wyn"
cat > "$f" <<'WYN'
fn main() {
    var a = System.args()
    print("n=${a.len()} one=${a[1]} two=${a[2]}")
}
WYN
rm -f "$TMP/argv.wyn.out"
argv_base=$(cd "$TMP" && perl -e 'alarm(90); exec @ARGV' -- "$WYN" run "$f" -- alpha beta 2>/dev/null)
rm -f "$TMP/argv.wyn.out"
argv_rel=$(cd "$TMP" && perl -e 'alarm(90); exec @ARGV' -- "$WYN" run --release "$f" -- alpha beta 2>/dev/null)
rm -f "$TMP/argv.wyn.out"
if [ "$argv_rel" = "n=3 one=alpha two=beta" ] && [ "$argv_rel" = "$argv_base" ]; then
    ok "argv reaches the program under --release"
else
    bad "argv under --release (want 'n=3 one=alpha two=beta', got '$argv_rel'; default gave '$argv_base')"
fi

# The static-buffer int_to_string bug. Two or more interpolations of DIFFERENT
# values in one string is the whole test: with a shared buffer every slot showed
# the last value computed.
check "multiple interpolations in one string keep distinct values" "10 + 20 = 30
1.5 2.5 4.0
a-b-c" <<'WYN'
fn main() {
    var a = 10
    var b = 20
    print("${a} + ${b} = ${a + b}")
    var x = 1.5
    var y = 2.5
    print("${x} ${y} ${x + y}")
    print("${"a"}-${"b"}-${"c"}")
}
WYN

# The string_replace buffer overrun (segfault under --release only), plus the
# other string helpers the slim header used to define privately.
check "string replace/upper/lower/contains agree with the default path" "Hello Wyn
a,b,c
HELLO
hello
true" <<'WYN'
fn main() {
    var s = "Hello World"
    print(s.replace("World", "Wyn"))
    print("a b c".replace(" ", ","))
    print("Hello".upper())
    print("HeLLo".lower())
    print("${s.contains("World")}")
}
WYN

# HashMap / HashSet: real functions in the archive, but the slim header declared
# none of them, so any program touching a map failed to compile under --release.
# (HashSet.contains returns the runtime's int 1/0, not a bool - that is existing
# behaviour on BOTH paths, so it is pinned here as-is rather than "corrected".)
check "HashMap and HashSet work under --release" "one=1
has=true
len=2
set=1
setlen=2" <<'WYN'
fn main() {
    var m = HashMap.new()
    m.set("one", 1)
    m.set("two", 2)
    print("one=${m.get("one")}")
    print("has=${m.has("two")}")
    print("len=${m.len()}")
    var s = HashSet.new()
    s.add("x")
    s.add("y")
    print("set=${s.contains("x")}")
    print("setlen=${s.len()}")
}
WYN

# Time.now()/Time.sleep() and the divide/modulo guards. wyn_safe_div is a MACRO
# threading __FILE__/__LINE__ - a plain two-argument prototype compiled and then
# failed at link, since only wyn_safe_div_impl is a symbol.
check "Time and the checked div/mod operators link" "div=5
mod=1
positive=true" <<'WYN'
fn main() {
    var a = 11
    print("div=${a / 2}")
    print("mod=${a % 2}")
    Time.sleep(1)
    print("positive=${Time.now() > 0}")
}
WYN

# spawn/await. `spawn f()` lowers to wyn_spawn_inline and await_all to a
# _Generic over eight wyn_await_all* variants - C requires EVERY association in
# a _Generic to name a declared function, even the seven not selected.
check "spawn and await_all link under --release" "sum=30" <<'WYN'
fn work(n: int) -> int => n * 10
fn main() {
    var f1 = spawn work(1)
    var f2 = spawn work(2)
    var rs = await_all([f1, f2])
    print("sum=${rs[0] + rs[1]}")
}
WYN

# The array higher-order functions and array_free/array_each.
check "array map/filter/reduce link under --release" "6
2
6" <<'WYN'
fn dbl(x: int) -> int => x * 2
fn even(x: int) -> bool => x % 2 == 0
fn main() {
    var xs = [1, 2, 3]
    var m = xs.map(dbl)
    print(m[2])
    var fl = xs.filter(even)
    print(fl[0])
    var s = 0
    var i = 0
    while i < xs.len() { s = s + xs[i]; i = i + 1 }
    print(s)
}
WYN

# The archive must actually CONTAIN runtime_exports.o. Asserted directly as well
# as behaviourally: if a future edit drops it from RT_SRCS, this names the cause
# in one line instead of leaving a wall of undefined symbols to decode.
n=$((n+1))
RT_LIB="$(dirname "$WYN")/runtime/libwyn_rt.a"
if [ ! -f "$RT_LIB" ]; then
    bad "runtime/libwyn_rt.a is missing (run 'make runtime')"
elif ar t "$RT_LIB" 2>/dev/null | grep -q '^runtime_exports\.o$'; then
    ok "runtime/libwyn_rt.a contains runtime_exports.o"
else
    bad "runtime/libwyn_rt.a lacks runtime_exports.o - add src/runtime_exports.c to RT_SRCS in the Makefile"
fi

# The default (TCC / full-header) path must keep working. Adding
# runtime_exports.c to the archive is safe ONLY because a linker pulls an archive
# member in just to resolve an undefined symbol, and a default-path program's own
# object already defines all of them. It is NOT safe in the from-source fallback
# list (main.c's wyn_runtime_sources[]): compiling runtime_exports.c ALONGSIDE the
# program's .c gives 805 duplicate symbols, both TUs having included
# wyn_runtime.h. Do not "unify" the two lists.
n=$((n+1))
f="$TMP/dflt.wyn"
cat > "$f" <<'WYN'
fn main() {
    var xs = [1.5, 2.5]
    print("d=${xs[1]} p=${Math.pow(3.0, 2.0)}")
}
WYN
rm -f "$TMP/dflt.wyn.out"
dflt=$(cd "$TMP" && perl -e 'alarm(90); exec @ARGV' -- "$WYN" run "$f" 2>/dev/null)
rm -f "$TMP/dflt.wyn.out"
if [ "$dflt" = "d=2.5 p=9.0" ]; then ok "the default path is unaffected by the archive change"
else bad "the default path regressed (want 'd=2.5 p=9.0', got '$dflt')"; fi

echo ""; echo "release-link: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
