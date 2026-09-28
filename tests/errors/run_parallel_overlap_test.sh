#!/bin/bash
# `parallel { }` must actually overlap its branches - for EVERY branch shape it
# accepts, not just the one the lowering happened to special-case first.
#
# THE DEFECT THIS GATES (measured on the published v1.21.0 AND v1.22.0-rc1
# artifacts, macOS arm64, against a 30ms single-branch baseline):
#
#     parallel { var a = work(1)  ... }   30ms   overlapped
#     parallel { spawn work(1)    ... }   30ms   overlapped
#     parallel { a = work(1)      ... }   61ms   SEQUENTIAL   <- the documented shape
#     parallel { work(1)          ... }        SEQUENTIAL
#
# Only a STMT_VAR bound to a call, or a bare `spawn`, became a real spawn. An
# assignment to a variable declared ABOVE the block - which is the shape the
# language's own `parallel { }` example uses - and a bare call statement both fell
# through to a plain sequential emit under a `/* parallel */` comment. The block
# named after parallelism was, for those shapes, a no-op.
#
# WHY THE BRANCHES SLEEP INSTEAD OF COMPUTING. Two traps, both hit while writing
# this file, and both of which make a CPU-bound fixture report success on a broken
# compiler:
#
#  1. CSE. A parallelism benchmark whose branches call a PURE function with the SAME
#     argument measures common-subexpression elimination. At -O2 the C compiler
#     collapses N identical calls into one, so the block appears to overlap
#     perfectly no matter what the lowering did. That is how the old "two branches
#     of fib(35) finish in the time of one (34ms)" figure was produced on a build
#     where parallel{} did not overlap at all; with four identical branches the
#     collapsed call was hoisted clear out of the timing window and reported 0ms
#     elapsed with a correct sum.
#  2. Dead-code elimination, which is worse because it makes a WRONG result LOOK
#     fast. A bare `work(3)` statement whose result is discarded, where `work` is
#     pure, is simply deleted. An earlier draft of this gate measured 0ms for the
#     bare-call arm with the fix REMOVED and passed - a vacuous arm. Mutation
#     testing is the only reason that was caught.
#
# Sleeping fixes both: Time.sleep is an external side-effecting call, so it can be
# neither collapsed nor deleted, and the wall-clock signal is a clean 2x (two
# overlapping 150ms sleeps take 150ms; sequential takes 300ms) that does not depend
# on the machine's speed or core count at all. Arguments still differ per branch as
# a matter of discipline, so a future purity inference cannot reintroduce trap 1.
#
# This gate tests DISPATCH - whether a branch runs concurrently at all. CPU-bound
# scaling is a separate property bounded by core count and is not measured here.
#
# The arms assert a RATIO against a measured single-branch baseline rather than an
# absolute time, because a shared runner's absolute numbers are worthless while
# contention scales both readings together. The bound is 1.6x: overlap lands at
# ~1.0x, sequential at ~2.0x.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

RATIO_NUM=16   # bound = 1.6x of the single-branch baseline, in tenths
NAP=150        # ms per branch; overlap ~150ms, sequential ~300ms

cat > "$TMP/par.wyn" <<WYN
// External sleep, so neither CSE nor dead-code elimination can touch a call to it.
fn nap(ms: int) -> int { Time.sleep(ms); return ms }

fn main() -> int {
    // Baseline: one branch. Printed so it cannot be dead-store eliminated.
    var t0 = DateTime.micros()
    var base = nap($NAP)
    var t1 = DateTime.micros()
    print("BASE \${(t1 - t0) / 1000} \${base}")

    // (1) assignment to variables declared ABOVE the block
    var a1 = 0
    var a2 = 0
    var t2 = DateTime.micros()
    parallel {
        a1 = nap($NAP)
        a2 = nap($((NAP+1)))
    }
    var t3 = DateTime.micros()
    print("ASSIGN \${(t3 - t2) / 1000} \${a1 + a2}")

    // (2) bare call statements, results discarded
    var t4 = DateTime.micros()
    parallel {
        nap($NAP)
        nap($((NAP+1)))
    }
    var t5 = DateTime.micros()
    print("BARE \${(t5 - t4) / 1000} 0")

    // (3) new declarations inside the block
    var t6 = DateTime.micros()
    parallel {
        var d1 = nap($NAP)
        var d2 = nap($((NAP+1)))
    }
    var t7 = DateTime.micros()
    print("VARDECL \${(t7 - t6) / 1000} 0")

    // (4) explicit spawn
    var t8 = DateTime.micros()
    parallel {
        spawn nap($NAP)
        spawn nap($((NAP+1)))
    }
    var t9 = DateTime.micros()
    print("SPAWN \${(t9 - t8) / 1000} 0")
    return 0
}
WYN

# Built with --release on purpose: -O2 is where CSE lives, so a fixture that only
# looks right unoptimised would hide the very artifact described above.
if ! (cd "$TMP" && perl -e 'alarm(300); exec @ARGV' -- "$WYN" build --release "$TMP/par.wyn") \
        > "$TMP/build.log" 2>&1; then
    bad "fixture builds --release"
    grep -m3 -iE "error" "$TMP/build.log" | sed 's/^/        /'
    echo ""; echo "parallel-overlap: $PASS pass, $FAIL fail"; exit 1
fi
[ -x "$TMP/par" ] || { bad "fixture binary produced"; echo "parallel-overlap: $PASS pass, $FAIL fail"; exit 1; }

# Discard the first run: macOS scans a freshly built binary on first exec and that
# alone has produced multi-second readings on a 30ms program.
"$TMP/par" >/dev/null 2>&1
out="$TMP/par.out"
perl -e 'alarm(300); exec @ARGV' -- "$TMP/par" > "$out" 2>&1 || { bad "fixture runs"; echo "parallel-overlap: $PASS pass, $FAIL fail"; exit 1; }

read_ms(){ awk -v k="$1" '$1==k {print $2}' "$out"; }
read_val(){ awk -v k="$1" '$1==k {print $3}' "$out"; }

BASE=$(read_ms BASE)
if [ -z "$BASE" ]; then
    bad "baseline reading present"; cat "$out" | sed 's/^/        /'
    echo ""; echo "parallel-overlap: $PASS pass, $FAIL fail"; exit 1
fi
# A baseline too small to measure makes every ratio meaningless - say so rather
# than passing four arms on noise.
if [ "$BASE" -lt 50 ]; then
    bad "baseline too small to compare against (${BASE}ms - raise NAP)"
    echo ""; echo "parallel-overlap: $PASS pass, $FAIL fail"; exit 1
fi

check(){  # $1=key  $2=label
    local ms; ms=$(read_ms "$1")
    if [ -z "$ms" ]; then bad "$2 (no reading)"; return; fi
    # ms*10 <= BASE*RATIO_NUM  i.e.  ms <= 1.6 * BASE
    if [ $((ms * 10)) -le $((BASE * RATIO_NUM)) ]; then
        ok "$2 overlaps (${ms}ms vs ${BASE}ms for one branch)"
    else
        bad "$2 did NOT overlap: ${ms}ms vs ${BASE}ms for one branch (bound 1.6x; 2x means sequential)"
    fi
}

check ASSIGN  "assignment to a variable declared outside the block"
check BARE    "a bare call statement"
check VARDECL "a new declaration inside the block"
check SPAWN   "an explicit spawn"

# Overlap is worthless if the answers change. Both branches must still have run and
# produced the values the sequential version would.
# Overlap is worthless if the answers change: both branches must have run and the
# join must have written their results back into the OUTER variables. nap returns
# its argument, so the sum is exactly NAP + NAP+1 and nothing else.
want=$(( NAP + NAP + 1 ))
asum=$(read_val ASSIGN)
if [ "${asum:-x}" = "$want" ]; then
    ok "the join writes both results back into the outer variables (sum=$asum)"
else
    bad "the join writes both results back: want $want, got '${asum:-}'"
fi

echo ""; echo "parallel-overlap: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
