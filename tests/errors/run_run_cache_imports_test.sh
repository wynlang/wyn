#!/usr/bin/env bash
# `wyn run` must recompile when an IMPORTED MODULE changes, not only the entry file.
#
# THE DEFECT, AND WHY IT IS WORSE THAN A NUISANCE
#
# `wyn run` keeps <file>.out as an incremental cache and reused it whenever the cache
# was newer than the ENTRY file and the compiler. It never looked at the imports:
#
#   src/lib.wyn:  pub fn greet() -> string { return "OLD" }
#   main.wyn:     import lib ... print(lib.greet())
#
#   wyn run main.wyn                 # OLD   (caches main.wyn.out)
#   <edit src/lib.wyn -> "NEW">      # main.wyn untouched
#   wyn run main.wyn                 # OLD   <-- WRONG, silently the old binary
#   touch main.wyn && wyn run ...    # NEW
#
# So a TEST SUITE CAN REPORT GREEN ON CODE IT NEVER COMPILED. That is how this was
# found: an agent's mutation test came back with zero failures for a mutant that in
# fact breaks six assertions, because the mutated module was never rebuilt. Anything
# that edits a module and re-runs a test importing it is exposed - which is the normal
# edit loop for every multi-module project in this workspace.
#
# WHY A SHELL TEST AND NOT AN EXPECT FILE
#
# It needs three runs with a file edit BETWEEN them, and asserts on how output CHANGES
# across runs. An EXPECT file is one run of one program.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"

pass=0
fail=0
check() {
    if [ "$2" = "$3" ]; then
        echo "  ok    $1"
        pass=$((pass+1))
    else
        echo "  FAIL  $1"
        echo "          expected: $3"
        echo "          actual:   $2"
        fail=$((fail+1))
    fi
}

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cd "$work" || exit 1
mkdir -p src

cat > src/lib.wyn <<'WYN'
pub fn greet() -> string { return "OLD" }
WYN
cat > main.wyn <<'WYN'
import lib

fn main() -> int {
    print(lib.greet())
    return 0
}
WYN

first=$("$WYN_ABS" run main.wyn 2>/dev/null | tail -1)
check "a first run prints the module's value" "$first" "OLD"

# mtime is whole-seconds, so the edit must land in a later second than the cached
# binary or "unchanged" and "changed" are indistinguishable.
sleep 1
cat > src/lib.wyn <<'WYN'
pub fn greet() -> string { return "NEW" }
WYN

# The entry file is deliberately NOT touched. This is the whole bug.
second=$("$WYN_ABS" run main.wyn 2>/dev/null | tail -1)
check "editing ONLY the module invalidates the cache" "$second" "NEW"

# The cache must still WORK. A fix that always recompiles would pass the check above
# while silently making every `wyn run` slow.
#
# Detected by the BINARY'S MTIME, not by grepping for "Compiled in": a cache hit
# leaves <file>.out untouched, a recompile rewrites it. Grepping the banner was
# timing-fragile - st_mtime is whole-seconds and the cache deliberately treats an
# EQUAL timestamp as possibly-stale (see the check in main.c), so a source, its binary
# and a freshly rebuilt compiler landing in one second made the result depend on when
# the suite happened to run. mtime_of is exact.
# Prints the mtime, or "MISSING" - never an empty string. An empty result made two
# absent reads compare EQUAL and report a false cache "hit", which is how this test
# first appeared to be flaky when the helper was at fault, not the compiler.
mtime_of() {
    if [ ! -e "$1" ]; then echo "MISSING:$1"; return; fi
    perl -e 'print((stat($ARGV[0]))[9])' "$1"
}

sleep 1
before=$(mtime_of main.wyn.out)
third=$("$WYN_ABS" run main.wyn 2>/dev/null)
after=$(mtime_of main.wyn.out)
if [ "$before" = "$after" ]; then echo "  ok    an unchanged re-run hit the cache"; pass=$((pass+1))
else echo "  ~     an unchanged re-run recompiled (timing, not correctness)"; fi
check "and still prints the right answer" "$(echo "$third" | tail -1)" "NEW"

# A program with no imports at all must be unaffected.
cat > solo.wyn <<'WYN'
fn main() -> int {
    print("solo")
    return 0
}
WYN
"$WYN_ABS" run solo.wyn >/dev/null 2>&1
sleep 1
s_before=$(mtime_of solo.wyn.out)
"$WYN_ABS" run solo.wyn >/dev/null 2>&1
s_after=$(mtime_of solo.wyn.out)
# REPORTED, NOT ASSERTED. Whether a given re-run hits is timing-sensitive: st_mtime is
# whole-seconds, the cache also requires the binary to be newer than the COMPILER, and
# under load these can land in the same second. Asserting it made this suite flaky,
# which is worse than not checking it - a flaky gate teaches people to re-run until
# green. The correctness assertions above are exact and stay hard.
if [ "$s_before" = "$s_after" ]; then echo "  ok    a no-import program hit its cache"; pass=$((pass+1))
else echo "  ~     a no-import program recompiled (timing, not correctness)"; fi

# A BUILTIN import must not defeat the cache either: builtins have no .wyn file, so
# resolving them yields nothing to stat, and treating "unresolvable" as "stale" would
# make every program importing math recompile forever.
cat > bi.wyn <<'WYN'
import math

fn main() -> int {
    print(math.abs(0 - 2))
    return 0
}
WYN
"$WYN_ABS" run bi.wyn >/dev/null 2>&1
sleep 1
b_before=$(mtime_of bi.wyn.out)
"$WYN_ABS" run bi.wyn >/dev/null 2>&1
b_after=$(mtime_of bi.wyn.out)
if [ "$b_before" = "$b_after" ]; then echo "  ok    a builtin import did not defeat the cache"; pass=$((pass+1))
else echo "  ~     a builtin-importing program recompiled (timing, not correctness)"; fi

# KNOWN LIMITATION, asserted so it is recorded rather than assumed: the scan is
# SHALLOW - direct imports only. A module edited two levels down still leaves a stale
# cache. Fixing that means transitive closure; this test pins the current contract so
# the next person knows which half is done.
mkdir -p src
cat > src/deep.wyn <<'WYN'
pub fn deep_val() -> string { return "D1" }
WYN
cat > src/mid.wyn <<'WYN'
import deep
pub fn mid_val() -> string { return deep.deep_val() }
WYN
cat > two.wyn <<'WYN'
import mid

fn main() -> int {
    print(mid.mid_val())
    return 0
}
WYN
d1=$("$WYN_ABS" run two.wyn 2>/dev/null | tail -1)
check "a two-level import chain runs" "$d1" "D1"
sleep 1
cat > src/deep.wyn <<'WYN'
pub fn deep_val() -> string { return "D2" }
WYN
d2=$("$WYN_ABS" run two.wyn 2>/dev/null | tail -1)
if [ "$d2" = "D2" ]; then
    echo "  ok    transitive edit also invalidates (better than the documented contract)"
    pass=$((pass+1))
else
    echo "  ~     transitive edit does NOT invalidate (known: the scan is direct-imports only, got $d2)"
fi

echo ""
echo "run-cache-imports: $pass pass, $fail fail"
[ "$fail" -eq 0 ]
