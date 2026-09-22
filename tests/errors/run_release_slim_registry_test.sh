#!/bin/bash
# V-8: `wyn run --release` must compile every call the checker's builtin registry
# blesses - and when it cannot, it must not blame the user.
#
# THE DEFECT. `Time.now_millis()` is a real function. It is in the registry, it is
# declared in src/wyn_runtime.h, and it compiled and ran under `wyn check`,
# `wyn run` and `wyn build --release`. Under `wyn run --release` - the ONE path
# that emits src/wyn_runtime_slim.h - it failed, because that header never declared
# it. main.c then caught clang's "call to undeclared function 'Time_now_millis'"
# and printed
#
#   'now_millis' is not a function Wyn knows about. Check the spelling
#
# i.e. the compiler blaming the user for its own missing header entry. A wrong
# diagnosis costs more than none: it sends the reader back to re-read a correct
# line. Time_now_millis was not alone - there were 100+ others.
#
# WHY THE ENUMERATION COMES FROM THE COMPILER. The registry is not one table: it is
# registered through `add_symbol` directly plus reg_fn / reg_math_fns / reg_ptr_fns
# / reg_task_fns / reg_gui_fns / reg_path_fns / reg_db_fns. A grep for the
# table-literal shape alone MISSED Url_encode and Url_decode - two of the very
# symbols this gate has to find - so it would have passed while the bug was live.
# `wyn dump-builtins` (internal, src/main.c) walks the checker's own global scope
# after init_checker(), so the list is the registry rather than a guess at it.
# Arm 1 fails the run if that enumeration ever goes thin, because every later arm
# is vacuous without it.
#
# WHY A C PROBE. "Is this symbol declared in that header" is a question only the C
# compiler can answer without re-implementing C. The probe takes the address of
# every registry symbol with the header included; an undeclared one is a hard
# compile error naming it. That covers 100% of the registry in two cc runs, with no
# need to invent a well-typed Wyn call per entry.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }
skip(){ echo "  skip  $1"; }

if ! command -v python3 >/dev/null 2>&1; then
    echo "release-slim-registry: SKIP (python3 needed to drive the symbol probe)"; exit 0
fi
CC_BIN="${WYN_CC:-cc}"
command -v "$CC_BIN" >/dev/null 2>&1 || CC_BIN=gcc
command -v "$CC_BIN" >/dev/null 2>&1 || { echo "release-slim-registry: SKIP (no C compiler)"; exit 0; }

# --- arm 1: the enumeration itself -------------------------------------------
# TWO sections, and both are needed. The global-scope symbols alone are NOT the
# whole reachable surface: `Base64.encode` is not a global symbol, so a gate built
# on that half alone reported green while `wyn run --release Base64.encode(...)`
# still failed. A namespace call is legal whenever src/wyn_runtime.h declares
# <Ns>_<method> - that is the rule the checker enforces (wyn_namespace_method_
# unknown, src/types.c) - so every such symbol is in scope for this gate too.
"$WYN" dump-builtins > "$TMP/dump.txt" 2>"$TMP/dump.err" || true
sed -n 's/^SYMBOL //p' "$TMP/dump.txt" | sort -u > "$TMP/symbols.txt"
sed -n 's/^NS //p'     "$TMP/dump.txt" | sort -u > "$TMP/namespaces.txt"
# Expand each namespace into every <Ns>_* the FULL runtime header mentions.
: > "$TMP/nssyms.txt"
while IFS= read -r ns; do
    [ -n "$ns" ] || continue
    grep -oE "\b${ns}_[A-Za-z0-9_]+[[:space:]]*\(" "$ROOT/src/wyn_runtime.h" "$ROOT/src/gui.h" 2>/dev/null \
      | sed 's/.*:\([A-Za-z0-9_]*\)[[:space:]]*($/\1/;s/[[:space:]]*($//' \
      | grep -oE "${ns}_[A-Za-z0-9_]+" >> "$TMP/nssyms.txt"
done < "$TMP/namespaces.txt"
sort -u "$TMP/symbols.txt" "$TMP/nssyms.txt" | grep . > "$TMP/registry.txt"
count=$(grep -c . "$TMP/registry.txt" 2>/dev/null || echo 0)
nsn=$(grep -c . "$TMP/namespaces.txt" 2>/dev/null || echo 0)
missing_known=""
# Time_now_millis    the V-8 repro
# Url_encode         registered via reg_fn(), which a table-literal grep missed
# Base64_encode      reachable ONLY through the namespace half of the enumeration
# print_str          codegen emits it for every print()
for n in Time_now_millis File_read Url_encode Base64_encode print_str; do
    grep -qx "$n" "$TMP/registry.txt" || missing_known="$missing_known $n"
done
if [ "$count" -ge 500 ] && [ "$nsn" -ge 20 ] && [ -z "$missing_known" ]; then
    ok "wyn dump-builtins enumerates the reachable surface ($count names over $nsn namespaces)"
else
    bad "wyn dump-builtins is not enumerating the reachable surface (count=$count namespaces=$nsn missing:$missing_known)"
    sed -n '1,5p' "$TMP/dump.err"
    echo ""
    echo "release-slim-registry: $PASS pass, $((FAIL)) fail"
    exit 1
fi

# --- arms 2+3: declared in the full header => must be declared in the slim one
python3 - "$ROOT" "$TMP" "$CC_BIN" <<'PY' > "$TMP/probe.out" 2>&1
import re, subprocess, sys
root, tmp, cc = sys.argv[1], sys.argv[2], sys.argv[3]
names = [l.strip() for l in open(tmp + "/registry.txt") if l.strip()]

def probe(header, ns):
    src = '#include "%s"\nvoid* __wyn_probe[] = {\n' % header
    src += "".join("  (void*)&%s,\n" % n for n in ns) + "};\n"
    p = tmp + "/probe.c"
    open(p, "w").write(src)
    r = subprocess.run([cc, "-std=c11", "-w", "-ferror-limit=0", "-I", root + "/src",
                        "-c", p, "-o", tmp + "/probe.o"], capture_output=True, text=True)
    und = set(re.findall(r"use of undeclared identifier '([A-Za-z_][A-Za-z0-9_]*)'", r.stderr))
    und |= set(re.findall(r"'([A-Za-z_][A-Za-z0-9_]*)' undeclared", r.stderr))
    # Anything else that errors on a probe line is not a function we can take the
    # address of (a type name, a macro): map the line back to its symbol and drop it.
    lines = src.split("\n"); other = set()
    for m in re.finditer(r'probe\.c:(\d+):\d+: error:', r.stderr):
        i = int(m.group(1)) - 1
        if 0 <= i < len(lines):
            mm = re.search(r'&([A-Za-z_][A-Za-z0-9_]*)', lines[i])
            if mm and mm.group(1) not in und:
                other.add(mm.group(1))
    return und, other

und_f, other_f = probe("wyn_runtime.h", names)
real = [n for n in names if n not in und_f and n not in other_f]
und_s, other_s = probe("wyn_runtime_slim.h", real)
gap = sorted(und_s | other_s)
print("REAL=%d" % len(real))
print("NOTFUNC=%d" % len(und_f | other_f))
print("GAP=%d" % len(gap))
for g in gap:
    print("GAPNAME %s" % g)
PY
real=$(sed -n 's/^REAL=//p' "$TMP/probe.out" | head -1)
gapn=$(sed -n 's/^GAP=//p' "$TMP/probe.out" | head -1)
if [ -z "${real:-}" ] || [ -z "${gapn:-}" ]; then
    bad "symbol probe did not run"
    sed -n '1,15p' "$TMP/probe.out"
elif [ "$real" -lt 400 ]; then
    bad "symbol probe found only $real real runtime functions in the registry - the probe is broken, not the header"
    sed -n '1,15p' "$TMP/probe.out"
else
    ok "$real registry entries are real functions declared by src/wyn_runtime.h"
    if [ "$gapn" -eq 0 ]; then
        ok "every one of them is also declared by src/wyn_runtime_slim.h (--release can compile them)"
    else
        bad "$gapn registry symbols are MISSING from src/wyn_runtime_slim.h - each one is a"
        echo "          \`wyn run --release\` failure waiting for a user, reported to them as a typo:"
        sed -n 's/^GAPNAME /          /p' "$TMP/probe.out"
    fi
fi

# --- arm 4: the V-8 repro, through all four commands -------------------------
# Flag order matters and is the opposite between commands: `wyn run` takes ITS
# flags BEFORE the path (everything after the file belongs to the program), while
# `wyn build --release` deliberately keeps the FULL header - so
# `wyn run --release f.wyn` is the only spelling that exercises the slim header at
# all. Get it wrong and this arm passes without ever compiling what it claims to.
cat > "$TMP/nowms.wyn" <<'WYN'
fn main() -> int {
    var t = Time.now_millis()
    if t > 0 { print("ms-ok") } else { print("ms-zero") }
    return 0
}
WYN
four_ok=1; four_detail=""
# THE CACHE MUST BE GONE BEFORE EVERY INVOCATION, .mode file included. `wyn run`
# reuses <file>.wyn.out, and until the mode became part of that key a `wyn run`
# before a `wyn run --release` on the same path meant the release compile NEVER
# HAPPENED and this arm reported green having tested nothing - the precise way a
# slim-header regression reaches a user. Deleting is belt; the .mode key is braces.
run_one() {  # $@ = the command line; sets `out`/`rc`
    rm -f "$TMP/nowms.wyn.out" "$TMP/nowms.wyn.out.mode" "$TMP/nowms.wyn.c" "$TMP/nowms" 2>/dev/null
    out=$(perl -e 'alarm(120); exec @ARGV' -- "$@" 2>&1); rc=$?
}
run_one "$WYN" check "$TMP/nowms.wyn"
[ $rc -eq 0 ] || { four_ok=0; four_detail="$four_detail check(rc=$rc)"; }
run_one "$WYN" run "$TMP/nowms.wyn"
{ [ $rc -eq 0 ] && echo "$out" | grep -q "ms-ok"; } || { four_ok=0; four_detail="$four_detail run(rc=$rc)"; }
run_one "$WYN" build --release "$TMP/nowms.wyn" -o "$TMP/nowms"
[ $rc -eq 0 ] || { four_ok=0; four_detail="$four_detail build--release(rc=$rc)"; }
run_one "$WYN" run --release "$TMP/nowms.wyn"
{ [ $rc -eq 0 ] && echo "$out" | grep -q "ms-ok"; } || { four_ok=0; four_detail="$four_detail run--release(rc=$rc: $(echo "$out" | head -2 | tr '\n' ' '))"; }
if [ "$four_ok" = 1 ]; then
    ok "Time.now_millis() compiles under check, run, build --release AND run --release"
else
    bad "Time.now_millis() does not compile under every command:$four_detail"
fi

# --- arm 4b: `wyn run --release` must NOT hand back a non-release binary --------
# The run cache is keyed on mtimes, and the MODE used to be absent from that key:
#     wyn run c.wyn            -> the debug binary
#     wyn run --release c.wyn  -> silently re-ran THAT binary, byte for byte
# Measured before the fix: identical .out, 1,071,896 bytes, where a release build of
# the same file in a fresh directory is 1,044,088. Two consequences, both worse than
# a slow compile: benchmarking `--release` in place times the DEBUG build, and the
# slim runtime header - which only `wyn run --release` compiles - can regress without
# any gate noticing, because the gate's release invocation never compiled anything.
#
# Asserted on the ARTIFACT, not on the output: both modes print the same thing, so
# "it printed hi" proves nothing. Deliberately does NOT delete the .out between the
# two runs - that deletion is exactly what this arm must not depend on.
cat > "$TMP/cachekey.wyn" <<'WYN'
fn main() -> int {
    print("hi")
    return 0
}
WYN
#
# Compared by SIZE, not by bytes. Two builds of one file from one compiler are not
# byte-reproducible here (the link embeds absolute paths), so a byte comparison of
# the second debug build against the first fails for a reason that has nothing to do
# with the cache - measured, and it is why this arm reads sizes.
osize() { [ -f "$1" ] && wc -c < "$1" | tr -d ' '; }
rm -f "$TMP/cachekey.wyn.out" "$TMP/cachekey.wyn.out.mode" "$TMP/cachekey.wyn.c"
perl -e 'alarm(180); exec @ARGV' -- "$WYN" run "$TMP/cachekey.wyn" >/dev/null 2>&1
dbg_sz=$(osize "$TMP/cachekey.wyn.out")
perl -e 'alarm(180); exec @ARGV' -- "$WYN" run --release "$TMP/cachekey.wyn" >/dev/null 2>&1
rel_sz=$(osize "$TMP/cachekey.wyn.out")
if [ -z "${dbg_sz:-}" ] || [ -z "${rel_sz:-}" ]; then
    bad "run-cache mode arm: no .out produced (dbg=[${dbg_sz:-}] rel=[${rel_sz:-}])"
elif [ "$dbg_sz" = "$rel_sz" ]; then
    bad "wyn run --release reused the non-release binary from the cache (both $rel_sz bytes) - the release path is untestable in place"
else
    ok "wyn run --release rebuilds instead of reusing the debug binary ($dbg_sz -> $rel_sz bytes)"
fi
# …and the reverse direction, which is the same bug pointing the other way: after a
# --release run, a plain `wyn run` must not keep running the release binary.
perl -e 'alarm(180); exec @ARGV' -- "$WYN" run "$TMP/cachekey.wyn" >/dev/null 2>&1
back_sz=$(osize "$TMP/cachekey.wyn.out")
if [ "${back_sz:-}" = "$dbg_sz" ]; then
    ok "and going back to the default mode rebuilds too ($back_sz bytes)"
else
    bad "default mode after --release did not rebuild (want $dbg_sz bytes, got ${back_sz:-none})"
fi
# POSITIVE PATH, and it is not optional. Both arms above pass if the mode key is
# simply never written, because the cache then never hits at all - measured, by
# making wyn_run_mode_record() a no-op: both arms stayed green and
# run_run_cache_imports_test.sh only downgraded its cache line to advisory
# ("timing, not correctness"). So nothing in the suite would have caught a fix that
# disabled caching outright. A SAME-MODE re-run must still hit.
#
# The source is backdated 5s first: st_mtime is whole-seconds, so a source written
# and compiled inside one tick fails `out.mtime > src.mtime` and misses the cache for
# a reason that is not the thing being tested. That flake is exactly why the other
# cache gate has to keep its hit checks advisory; backdating removes it.
rm -f "$TMP/cachekey.wyn.out" "$TMP/cachekey.wyn.out.mode"   # the arms above left a
                                                             # warm cache; this arm
                                                             # needs a cold one or the
                                                             # "first" run never
                                                             # compiles and proves
                                                             # nothing.
perl -e 'my $t = time - 5; utime $t, $t, $ARGV[0] or die' "$TMP/cachekey.wyn"
first=$(perl -e 'alarm(180); exec @ARGV' -- "$WYN" run "$TMP/cachekey.wyn" 2>&1)
second=$(perl -e 'alarm(180); exec @ARGV' -- "$WYN" run "$TMP/cachekey.wyn" 2>&1)
if echo "$first" | grep -q "Compiled in" && ! echo "$second" | grep -q "Compiled in"; then
    ok "a same-mode re-run still HITS the cache (the mode key is recorded, not just checked)"
elif ! echo "$first" | grep -q "Compiled in"; then
    bad "the first run did not compile, so the cache-hit arm proves nothing [$first]"
else
    bad "a same-mode re-run recompiled - the mode key is never recorded, so the cache is dead [$second]"
fi
rm -f "$TMP/cachekey.wyn.out" "$TMP/cachekey.wyn.out.mode" "$TMP/cachekey.wyn.c"

# --- arm 5: the WORDING, on a slim header that really is missing a declaration
# Self-contained: build a SHADOW WYN_ROOT (resolve_wyn_root probes $WYN_ROOT first,
# src/main.c) whose src/ is symlinked to the real one except for a copy of
# wyn_runtime_slim.h with ONE declaration deleted. The tree is never touched, and
# the arm therefore exercises a state that cannot otherwise be reached once the gap
# above is closed - without it, the new wording would be dead code no test covers.
SHADOW="$TMP/shadow"
mkdir -p "$SHADOW/src"
for f in "$ROOT"/src/*; do ln -s "$f" "$SHADOW/src/$(basename "$f")" 2>/dev/null; done
rm -f "$SHADOW/src/wyn_runtime_slim.h"
grep -v 'Time_now_millis' "$ROOT/src/wyn_runtime_slim.h" > "$SHADOW/src/wyn_runtime_slim.h"
ln -s "$ROOT/runtime" "$SHADOW/runtime" 2>/dev/null
ln -s "$ROOT/vendor" "$SHADOW/vendor" 2>/dev/null
if grep -q 'Time_now_millis' "$SHADOW/src/wyn_runtime_slim.h"; then
    skip "wording arm (could not remove the declaration from the shadow header)"
elif ! grep -q 'Time_now_millis' "$ROOT/src/wyn_runtime_slim.h"; then
    bad "wording arm cannot run: src/wyn_runtime_slim.h does not declare Time_now_millis, so arm 3 above is what needs fixing"
else
    rm -f "$TMP/nowms.wyn.out" "$TMP/nowms.wyn.c"
    wout=$(WYN_ROOT="$SHADOW" perl -e 'alarm(120); exec @ARGV' -- "$WYN" run --release "$TMP/nowms.wyn" 2>&1); wrc=$?
    if [ "$wrc" -eq 0 ]; then
        skip "wording arm (the shadow root still compiled - WYN_ROOT was not honoured here)"
    elif echo "$wout" | grep -qi "check the spelling"; then
        bad "a method Wyn KNOWS is still reported as a spelling mistake [$(echo "$wout" | head -2 | tr '\n' ' ')]"
    elif echo "$wout" | grep -qi "COMPILER BUG" && echo "$wout" | grep -q "Time_now_millis"; then
        ok "a missing declaration is reported as a compiler bug and names the C symbol"
    else
        bad "a missing declaration is not reported as a compiler bug [$(echo "$wout" | head -3 | tr '\n' ' ')]"
    fi
fi

# ...and the converse: when the compiler CANNOT prove the name is one of its own, it
# must not claim a compiler bug. Same shadow trick, one step further - drop
# wyn_runtime.h as well, so the declaration index is unavailable (the -1 arm of
# wyn_namespace_method_declared). The checker then stays permissive, the call reaches
# codegen, the slim compile fails, and the reporter has nothing to stand on: the old
# wording is the honest answer and a compiler-bug claim would be a guess.
#
# THIS ARM IS WHY THE FIX IS A THREE-WAY TEST AND NOT A TWO-WAY ONE. Hard-coding
# "it's a compiler bug" passes every other arm in this file - measured, by doing it -
# because #357 rejects a genuine namespace typo at CHECK time, so no typo ever
# reaches the reporter to contradict it.
SHADOW2="$TMP/shadow2"
mkdir -p "$SHADOW2/src"
for f in "$ROOT"/src/*; do ln -s "$f" "$SHADOW2/src/$(basename "$f")" 2>/dev/null; done
rm -f "$SHADOW2/src/wyn_runtime_slim.h" "$SHADOW2/src/wyn_runtime.h"
grep -v 'Time_now_millis' "$ROOT/src/wyn_runtime_slim.h" > "$SHADOW2/src/wyn_runtime_slim.h"
# wyn_runtime.h must EXIST or resolve_wyn_root rejects the whole shadow and silently
# falls back to the real tree - which is how the first version of this arm SKIPPED
# instead of testing anything. It is emptied rather than deleted: the declaration
# index reports "unavailable" when it finds no symbols at all (src/types.c,
# wyn_rt_index_load), which is precisely the state being exercised. --release
# compiles against the slim header only, so an empty wyn_runtime.h costs nothing.
printf '// intentionally empty: see tests/errors/run_release_slim_registry_test.sh\n' \
    > "$SHADOW2/src/wyn_runtime.h"
ln -s "$ROOT/runtime" "$SHADOW2/runtime" 2>/dev/null
ln -s "$ROOT/vendor" "$SHADOW2/vendor" 2>/dev/null
rm -f "$TMP/nowms.wyn.out" "$TMP/nowms.wyn.c"
uout=$(WYN_ROOT="$SHADOW2" perl -e 'alarm(120); exec @ARGV' -- "$WYN" run --release "$TMP/nowms.wyn" 2>&1); urc=$?
if [ "$urc" -eq 0 ]; then
    skip "unprovable arm (the shadow root still compiled)"
elif echo "$uout" | grep -qi "COMPILER BUG"; then
    bad "the compiler claims a compiler bug it cannot prove (declaration index unavailable) [$(echo "$uout" | head -2 | tr '\n' ' ')]"
else
    ok "with no declaration index to consult, no compiler-bug claim is made"
fi

# And end to end on the REAL root: a genuine typo must be reported as an unknown
# method, never as a compiler bug. (With #357 this is answered by the CHECKER before
# codegen runs - which is the right place - so this arm guards the user-visible
# behaviour rather than the reporter branch above.)
cat > "$TMP/typo.wyn" <<'WYN'
fn main() -> int {
    var t = Time.now_ms()
    print("${t}")
    return 0
}
WYN
rm -f "$TMP/typo.wyn.out" "$TMP/typo.wyn.c"
tout=$(perl -e 'alarm(120); exec @ARGV' -- "$WYN" run --release "$TMP/typo.wyn" 2>&1); trc=$?
if [ "$trc" -eq 0 ]; then
    bad "a misspelled namespace method compiled (Time.now_ms should not exist)"
elif echo "$tout" | grep -qi "COMPILER BUG"; then
    bad "a genuine typo is blamed on the compiler [$(echo "$tout" | head -2 | tr '\n' ' ')]"
elif echo "$tout" | grep -qi "unknown method 'Time.now_ms'"; then
    ok "a genuine typo is still reported as an unknown method, not a compiler bug"
else
    bad "a genuine typo gets neither diagnosis [$(echo "$tout" | head -3 | tr '\n' ' ')]"
fi

# --- arm 6: the archive `wyn build-runtime` produces must satisfy the slim path
# Same family as the gap above, and the same root cause: TWO LISTS OF THE SAME
# THING. src/runtime_exports.c is the only TU that includes wyn_runtime.h, so it is
# where the 800+ functions DEFINED in that header become linkable symbols. The
# Makefile's RT_SRCS has it; main.c's wyn_runtime_sources[] does not, and
# `wyn build-runtime` built its source list from the latter - so an archive built
# that way could not satisfy a single --release link. Checked by reading the
# command's own source list rather than by running it, because running it
# OVERWRITES runtime/libwyn_rt.a and would sabotage every later suite in `make test`.
# Match the COMPILE LOOP itself, not the file anywhere in the block: the first
# version of this arm grepped the whole build-runtime block and was satisfied by the
# COMMENT that explains why runtime_exports.c is appended - so deleting it from the
# actual `for f in ...` line left the arm green. Caught by mutation, and it is the
# textbook version of the trap: a test that matches prose passes when the code is
# wrong and the prose is right.
if sed -n '/strcmp(command, "build-runtime")/,/return rt_result/p' "$ROOT/src/main.c" \
     | grep -E '^[[:space:]]*"for f in ' | grep -q 'src/runtime_exports\.c'; then
    ok "wyn build-runtime's compile loop includes src/runtime_exports.c (so its archive can satisfy --release)"
else
    bad "wyn build-runtime omits src/runtime_exports.c from its compile loop - its archive cannot satisfy a --release link"
    sed -n '/strcmp(command, "build-runtime")/,/return rt_result/p' "$ROOT/src/main.c" \
      | grep -E '^[[:space:]]*"for f in ' | sed 's/^/          /'
fi

echo ""
echo "release-slim-registry: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
