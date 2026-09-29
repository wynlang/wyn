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
# THE SECOND DEFECT THIS GATES: a call to a NAMESPACED BUILTIN was never dispatched
# in ANY position, in either spelling, because dispatch needed a wrapper named after
# the callee:
#
#   parallel { Time.sleep(200) x8 }     1,618ms - 8x200ms, no overlap at all. This
#                                       is the exact shape the concurrency guide
#                                       offered as the reassuring I/O-wait example.
#   spawn Time.sleep(200)               returned after 204ms: SYNCHRONOUS.
#   var f = spawn Time.sleep(200)       emitted `Future* f = NULL` - the call was
#                                       DROPPED. `await f` returned 0 after 0ms and
#                                       nothing ever slept.
#   spawn Time::sleep(200)              emitted `__spawn_wrapper_Time::sleep_1`,
#                                       which is not a C identifier: did not build.
#   spawn print("hi")                   emitted `wynfn_print(...)`, undeclared: did
#                                       not build.
#
# Both spellings are covered here because they are DIFFERENT AST shapes -
# `Time::sleep(x)` folds into one identifier containing "::" and arrives as a call,
# `Time.sleep(x)` arrives as a method call on the namespace - and they reached
# different dispatch sites. Fixing one silently leaves the other.
#
# NOT covered, because it is not yet dispatched: a method call on a VALUE
# (`obj.run(1)`), whose receiver cannot be copied into a per-site args box without
# losing mutations made through its address. Those arms are deliberately absent
# rather than asserted-sequential, so implementing them does not red this gate.
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

fn battery(rep: int) -> int {
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

    // (5) bare NAMESPACED BUILTIN, dot spelling - the shape the concurrency guide
    //     offers as its I/O-wait example. Four branches, so a regression reads as
    //     4x rather than 2x.
    var t10 = DateTime.micros()
    parallel {
        Time.sleep($NAP)
        Time.sleep($((NAP+1)))
        Time.sleep($((NAP+2)))
        Time.sleep($((NAP+3)))
    }
    var t11 = DateTime.micros()
    print("NSDOT \${(t11 - t10) / 1000} 0")

    // (6) the SAME calls in the :: spelling. A different AST shape reaching a
    //     different dispatch site, so it needs its own arm.
    var t12 = DateTime.micros()
    parallel {
        Time::sleep($NAP)
        Time::sleep($((NAP+1)))
        Time::sleep($((NAP+2)))
        Time::sleep($((NAP+3)))
    }
    var t13 = DateTime.micros()
    print("NSCOLON \${(t13 - t12) / 1000} 0")

    // (7) explicit spawn of a builtin inside the block, both spellings. The ::
    //     form did not even COMPILE before (__spawn_wrapper_Time::sleep_1).
    var t14 = DateTime.micros()
    parallel {
        spawn Time.sleep($NAP)
        spawn Time::sleep($((NAP+1)))
    }
    var t15 = DateTime.micros()
    print("NSSPAWN \${(t15 - t14) / 1000} 0")

    // (8) fire-and-forget spawn of a builtin must RETURN AT ONCE. Before, the
    //     "spawn" was emitted as a plain call and this measured a full branch.
    var t16 = DateTime.micros()
    spawn Time.sleep($NAP)
    var t17 = DateTime.micros()
    print("FIREFORGET \${(t17 - t16) / 1000} 0")

    // (9) spawn EXPRESSION of a builtin. Two separate properties: the spawn must
    //     return at once, AND the await must really wait for it. Before, \`fut\` was
    //     a literal NULL and the call was dropped, so BOTH numbers were 0 - which
    //     is why the await bound below has a FLOOR as well as a ceiling.
    var t18 = DateTime.micros()
    var fut = spawn Time.sleep($NAP)
    var t19 = DateTime.micros()
    var fv = await fut
    var t20 = DateTime.micros()
    print("SPAWNEXPR \${(t19 - t18) / 1000} \${fv}")
    print("SPAWNEXPRAWAIT \${(t20 - t18) / 1000} \${fv}")

    // (10) EVERY dispatched branch must actually RUN. A wall-clock reading of ~1x
    //      cannot tell "four branches overlapped" from "one ran and three were
    //      dropped" - so these four bare builtin calls have an observable effect
    //      each. One is interpolated, which makes its argument a FRESH +1 string:
    //      that is the reference the site box retains and the wrapper releases, so
    //      this arm also covers the string-argument ownership path.
    var tag = "d"
    parallel {
        print("EFFECT-a")
        print("EFFECT-b")
        print("EFFECT-c")
        print("EFFECT-\${tag}")
    }
    return rep
}

fn main() -> int {
    // WARM-UP, discarded. The coroutine scheduler's worker pool spins up lazily, so
    // the FIRST parallel block in a process pays for that startup. All four blocks
    // below lower to byte-identical C - the same wyn_spawn_async_traced pair and the
    // same future_get_consume pair - so without this the arm that fails is simply
    // whichever one happens to run first. That is exactly how this gate failed on a
    // macOS CI runner: the first block measured 298ms against a 170ms baseline while
    // the other three overlapped, and nothing about the lowering differed.
    parallel {
        spawn nap(10)
        spawn nap(10)
    }
    // FOUR reps; the shell takes the MIN per key. Contention only ever adds time, so
    // the minimum is the closest reading to the real cost on a shared runner.
    //
    // It was two, and two was not enough: EVERY reading here, baseline included, is a
    // sample that contention can only inflate, and with two samples a hosted runner
    // inflated BOTH copies of one key. Two real flakes, in opposite directions, neither
    // caused by the change under test:
    //
    //   ceiling, macos-15-intel: 252ms against a 154ms baseline (1.64x, bound 1.6x) -
    //                            the MEASUREMENT was inflated
    //   floor,   macos-15:       160ms against a 216ms baseline (0.74x, floor 0.8x) -
    //                            the BASELINE was inflated, and 160ms was the correct
    //                            answer for a 150ms sleep
    //
    // The gate's premise is that contention scales both readings together, which is only
    // true in aggregate: at one sample per key it is false, and either side can move
    // alone. Four reps cost a few seconds and make an uncontended sample much likelier
    // for every key. On an idle box this arm set reads 151-157ms against 151-154ms, so
    // the bound is nowhere near marginal when the box is quiet - the flakes were the
    // runner, not the margin.
    var r1 = battery(1)
    var r2 = battery(2)
    var r3 = battery(3)
    var r4 = battery(4)
    if r1 + r2 + r3 + r4 != 10 { print("battery did not run four times") }
    return 0
}
WYN

# BOTH modes. --release matters because -O2 is where CSE lives, so a fixture that
# only looks right unoptimised would hide the artifact described above; plain mode
# matters because the two use DIFFERENT runtime headers (wyn_runtime.h vs
# wyn_runtime_slim.h), so a runtime symbol added for one and not the other builds in
# exactly one of them.
run_mode() {  # $1 = "" | "--release" ; $2 = label
    local flags="$1" label="$2" dir="$TMP/$2"
    mkdir -p "$dir"; cp "$TMP/par.wyn" "$dir/par.wyn"
    if ! (cd "$dir" && perl -e 'alarm(600); exec @ARGV' -- "$WYN" build $flags "$dir/par.wyn") \
            > "$dir/build.log" 2>&1; then
        bad "[$label] fixture builds"
        grep -m3 -iE "error" "$dir/build.log" | sed 's/^/        /'
        return
    fi
    [ -x "$dir/par" ] || { bad "[$label] fixture binary produced"; return; }

    # Discard the first run: macOS scans a freshly built binary on first exec and
    # that alone has produced multi-second readings on a 30ms program.
    "$dir/par" >/dev/null 2>&1
    local out="$dir/par.out"
    perl -e 'alarm(600); exec @ARGV' -- "$dir/par" > "$out" 2>&1 || { bad "[$label] fixture runs"; return; }

    # MIN across reps - each key is printed once per rep.
    read_ms(){ awk -v k="$1" '$1==k { if (m=="" || $2+0 < m+0) m=$2 } END{print m}' "$out"; }
    read_val(){ awk -v k="$1" '$1==k {print $3; exit}' "$out"; }

    local BASE; BASE=$(read_ms BASE)
    if [ -z "$BASE" ]; then
        bad "[$label] baseline reading present"; sed 's/^/        /' "$out"; return
    fi
    # A baseline too small to measure makes every ratio meaningless - say so rather
    # than passing every arm on noise.
    if [ "$BASE" -lt 50 ]; then
        bad "[$label] baseline too small to compare against (${BASE}ms - raise NAP)"; return
    fi

    # A dispatched-and-joined branch must take ABOUT one branch's time: not two branches'
    # worth (it did not overlap) and not zero (it did not run, or was not joined). Both
    # ends are needed.
    #
    # The FLOOR was missing, and a 0ms reading passed. Found by mutation: with
    # par_branch_classify() forced to refuse every branch, ten of these arms correctly
    # reddened at a sequential 2x - but the two explicit-spawn arms reported
    # "overlaps (0ms vs 150ms)" and PASSED. A refused `spawn` is emitted as a plain
    # statement, a bare `spawn` statement is fire-and-forget, so it returns at once and
    # the sleeps never land inside the window. That is the same ceiling-only hole
    # check_waited() already has a floor for, and 0ms passing a gate is exactly the shape
    # that let `parallel { }` look correct for several releases.
    check(){  # $1=key  $2=label
        local ms; ms=$(read_ms "$1")
        if [ -z "$ms" ]; then bad "[$label] $2 (no reading)"; return; fi
        # BASE*8 <= ms*10 <= BASE*RATIO_NUM   i.e.  0.8 * BASE <= ms <= 1.6 * BASE
        if [ $((ms * 10)) -lt $((BASE * 8)) ]; then
            bad "[$label] $2 did not RUN: ${ms}ms is under 0.8x of ${BASE}ms for one branch (0ms means the call was dropped or never joined)"
        elif [ $((ms * 10)) -le $((BASE * RATIO_NUM)) ]; then
            ok "[$label] $2 overlaps (${ms}ms vs ${BASE}ms for one branch)"
        else
            bad "[$label] $2 did NOT overlap: ${ms}ms vs ${BASE}ms for one branch (bound 1.6x; 2x means sequential)"
        fi
    }
    # A spawn that DISPATCHES returns in microseconds; one emitted as a plain call
    # returns after a full branch. A quarter of a branch separates those by 4x.
    check_immediate(){  # $1=key  $2=label
        local ms; ms=$(read_ms "$1")
        if [ -z "$ms" ]; then bad "[$label] $2 (no reading)"; return; fi
        if [ $((ms * 4)) -le "$BASE" ]; then
            ok "[$label] $2 returns immediately (${ms}ms vs ${BASE}ms for the call)"
        else
            bad "[$label] $2 ran INLINE: returned after ${ms}ms, the call itself is ${BASE}ms"
        fi
    }
    # The await must really have waited. The FLOOR is the load-bearing half: the old
    # `Future* f = NULL` lowering dropped the call and returned 0 instantly, which a
    # ceiling-only bound reads as a pass.
    check_waited(){  # $1=key  $2=label
        local ms; ms=$(read_ms "$1")
        if [ -z "$ms" ]; then bad "[$label] $2 (no reading)"; return; fi
        if [ $((ms * 10)) -ge $((BASE * 8)) ] && [ $((ms * 10)) -le $((BASE * RATIO_NUM)) ]; then
            ok "[$label] $2 waited for the task (${ms}ms vs ${BASE}ms)"
        else
            bad "[$label] $2 did not wait for the task: ${ms}ms, expected 0.8-1.6x of ${BASE}ms"
        fi
    }

    check ASSIGN   "assignment to a variable declared outside the block"
    check BARE     "a bare call statement"
    check VARDECL  "a new declaration inside the block"
    check SPAWN    "an explicit spawn"
    check NSDOT    "bare namespaced builtins, Ns.method() spelling"
    check NSCOLON  "bare namespaced builtins, Ns::method() spelling"
    check NSSPAWN  "explicit spawn of namespaced builtins, both spellings"
    check_immediate FIREFORGET "fire-and-forget spawn of a builtin"
    check_immediate SPAWNEXPR  "spawn EXPRESSION of a builtin"
    check_waited    SPAWNEXPRAWAIT "await of a spawned builtin"

    # Overlap is worthless if the answers change: both branches must have run and the
    # join must have written their results back into the OUTER variables. nap returns
    # its argument, so the sum is exactly NAP + NAP+1 and nothing else.
    local want asum; want=$(( NAP + NAP + 1 )); asum=$(read_val ASSIGN)
    if [ "${asum:-x}" = "$want" ]; then
        ok "[$label] the join writes both results back into the outer variables (sum=$asum)"
    else
        bad "[$label] the join writes both results back: want $want, got '${asum:-}'"
    fi

    # Every dispatched branch RAN. Without this, "one branch ran and three were
    # dropped" is indistinguishable from perfect overlap.
    local missing=""
    for k in a b c d; do grep -q "^EFFECT-$k$" "$out" || missing="$missing $k"; done
    if [ -z "$missing" ]; then
        ok "[$label] all four dispatched builtin branches ran (EFFECT-a..d present)"
    else
        bad "[$label] dispatched branches that never ran:$missing"
    fi
}

run_mode ""          debug
run_mode "--release" release

echo ""; echo "parallel-overlap: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
