#!/bin/bash
# Collecting `spawn` futures in a LOOP must compile - for every spelling of the
# declaration, and for both ways of consuming the list.
#
# THE DEFECT THIS GATES (reproduced on the published v1.21.0 AND v1.22.0-rc1
# artifacts, in both `wyn build` and `wyn build --release`). `wyn check` reported NO
# errors in all four combinations; two of them then emitted invalid C:
#
#   declaration            for t in ts { await t }     await_all(ts)
#   var ts: [int] = []     builds                      FAILS
#   var ts = []            FAILS                       builds
#
#   var ts: [int] = []  ->  error: initializing 'WynArray' with an expression of
#                           incompatible type 'WynIntArray'
#                             WynArray ts = ({ WynIntArray __arr_6 = int_array_new(); ... });
#   var ts = []         ->  the SAME error, moved to the iteration site:
#                             WynArray __iter_array = ts;
#
# ROOT CAUSE: three independent name tables each owned a piece of the answer to
# "does this array use the packed WynIntArray representation?" - the `[int]` opt-in,
# the int-array veto, and the spawn-future table - and no function joined them. The
# declaration, the initializer expression, the element accessors and the for-in
# lowering each asked a DIFFERENT subset. The annotation only chose which consumer
# broke. They now all go through one authority (wyn_array_is_packed / the
# wyn_array_decl_c_type that records its answer), so they cannot disagree.
#
# WHY IT MATTERS beyond the compile error: building a task list in a loop is the
# only way to write N concurrent tasks for a NON-CONSTANT N. The documented literal
# form (`tasks = [spawn f(1), spawn f(2)]`) works and was the only shape covered.
#
# Both modes are run because they use different runtime headers (wyn_runtime.h vs
# wyn_runtime_slim.h), and the original defect reproduced in both.
#
# Every cell checks the VALUES, not just that it links: each task returns its own
# distinct argument, so the sum pins down that all N futures ran AND that each
# result was read back from the right slot. A cell that only asserted "it builds"
# would pass on a lowering that dropped three of four tasks. The elapsed time is
# asserted against a measured single-task baseline for the same reason the parallel
# gate does it - a loop of spawns that runs sequentially is still a defect.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

NAP=120          # ms per task: overlap ~120ms, sequential ~480ms for four
NTASK=4
WANT_SUM=$(( NAP + (NAP+1) + (NAP+2) + (NAP+3) ))
# Bound = 2.5x of the one-task baseline, in tenths. FOUR tasks, so SEQUENTIAL is 4.0x
# and real overlap measures ~1.0x - 2.5x sits between them with room on both sides.
#
# It was 2.0x, and that failed twice on macos-15-intel at 2.02x and 2.08x (250ms vs
# 120ms, 267ms vs 132ms) on a 3-vCPU runner under load. Those were not a regression:
# measured on an idle box, the default coroutine executor is FLAT in the number of
# awaited sleeps - 2 tasks 105ms, 4 tasks 103ms, 8 tasks 107ms, 16 tasks 118ms against
# a 100ms one-task baseline - so nothing here is core-limited. (The legacy thread pool
# behind WYN_ASYNC_POOL=1 *is*: 16 tasks take 222ms there, two rounds.) What the CI
# failures measured was the scheduler itself being starved of CPU, which inflates the
# elapsed reading without telling us anything about the lowering.
#
# 2.5x still catches the defect this gate exists for. The failure mode is a loop of
# spawns that does not overlap AT ALL, which lands at 4.0x.
RATIO_NUM=25

# $1 = cell name, $2 = declaration line, $3 = consumer block
emit_cell() {
    cat > "$TMP/$1.wyn" <<WYN
// Time.sleep is an external side-effecting call, so neither CSE nor dead-code
// elimination can remove or collapse these tasks; each returns its own argument so
// the sum proves every one of them ran and landed in the right slot.
fn nap(ms: int) -> int { Time.sleep(ms); return ms }

fn main() -> int {
    // Baseline: ONE task, printed so it cannot be dead-store eliminated. THREE
    // samples, and the shell takes the minimum: this reading is the denominator of
    // every ratio below, and contention can only inflate it. A single sample that
    // came in high once failed a CORRECT measurement on a hosted runner.
    for r in 0..3 {
        var b0 = DateTime.micros()
        var base = nap($NAP)
        var b1 = DateTime.micros()
        print("BASE \${(b1 - b0) / 1000} \${base}")
    }

    // N is a VARIABLE. That is the whole point of the shape: with a constant you
    // could write the literal list the docs already cover.
    var n = $NTASK
    var t0 = DateTime.micros()
    $2
    for i in 0..n { ts.push(spawn nap($NAP + i)) }
$3
    var t1 = DateTime.micros()
    print("ELAPSED \${(t1 - t0) / 1000} 0")
    return 0
}
WYN
}

# Cell 1/2: the `for t in ts { await t }` consumer.
FORIN='    var sum = 0
    var cnt = 0
    for t in ts {
        sum = sum + await t
        cnt = cnt + 1
    }
    print("SUM ${sum} ${cnt}")'
# Cell 3/4: the await_all(ts) consumer.
AWAITALL='    var r = await_all(ts)
    var sum = 0
    for v in r { sum = sum + v }
    print("SUM ${sum} ${r.len()}")'

emit_cell annot_forin   'var ts: [int] = []' "$FORIN"
emit_cell annot_awaitall 'var ts: [int] = []' "$AWAITALL"
emit_cell infer_forin   'var ts = []'        "$FORIN"
emit_cell infer_awaitall 'var ts = []'       "$AWAITALL"

run_cell() {  # $1 = cell, $2 = flags, $3 = mode label, $4 = human description
    local cell="$1" flags="$2" mode="$3" desc="$4"
    local dir="$TMP/$cell-$mode"
    mkdir -p "$dir"; cp "$TMP/$cell.wyn" "$dir/c.wyn"

    # `wyn check` passing is not in doubt - it always did. It is asserted anyway,
    # because the headline of this defect is that check and build DISAGREED, and a
    # gate for that has to pin down both halves.
    if ! perl -e 'alarm(300); exec @ARGV' -- "$WYN" check "$dir/c.wyn" > "$dir/check.log" 2>&1; then
        bad "[$mode] $desc: wyn check"
        grep -m2 -iE "error" "$dir/check.log" | sed 's/^/        /'
        return
    fi
    if ! (cd "$dir" && perl -e 'alarm(600); exec @ARGV' -- "$WYN" build $flags "$dir/c.wyn") \
            > "$dir/build.log" 2>&1; then
        bad "[$mode] $desc: builds (wyn check passed, so this is check/build disagreement)"
        grep -m2 -iE "error" "$dir/build.log" | sed 's/^/        /'
        return
    fi
    [ -x "$dir/c" ] || { bad "[$mode] $desc: binary produced"; return; }

    # Discard the first run: macOS scans a freshly built binary on first exec.
    "$dir/c" >/dev/null 2>&1
    if ! perl -e 'alarm(300); exec @ARGV' -- "$dir/c" > "$dir/out" 2>&1; then
        bad "[$mode] $desc: runs"; sed 's/^/        /' "$dir/out"; return
    fi

    local sum len base elapsed
    sum=$(awk '$1=="SUM"{print $2; exit}'     "$dir/out")
    len=$(awk '$1=="SUM"{print $3; exit}'     "$dir/out")
    base=$(awk '$1=="BASE"{ if (m=="" || $2+0 < m+0) m=$2 } END{print m}' "$dir/out")
    elapsed=$(awk '$1=="ELAPSED"{print $2; exit}' "$dir/out")

    if [ "${sum:-x}" = "$WANT_SUM" ]; then
        ok "[$mode] $desc: all $NTASK results, each from its own task (sum=$sum)"
    else
        bad "[$mode] $desc: wrong results - want sum $WANT_SUM, got '${sum:-}'"
        sed 's/^/        /' "$dir/out"
        return
    fi
    if [ "${len:-x}" = "$NTASK" ]; then
        ok "[$mode] $desc: the list holds all $NTASK futures"
    else
        bad "[$mode] $desc: list length - want $NTASK, got '${len:-}'"
    fi

    # A loop of spawns that does not overlap is still a defect; but a baseline too
    # small to measure makes the ratio meaningless, so say so rather than pass on
    # noise.
    if [ -z "${base:-}" ] || [ -z "${elapsed:-}" ]; then
        bad "[$mode] $desc: timing readings present"
    elif [ "$base" -lt 50 ]; then
        bad "[$mode] $desc: baseline too small to compare against (${base}ms - raise NAP)"
    elif [ $((elapsed * 10)) -le $((base * RATIO_NUM)) ]; then
        ok "[$mode] $desc: the $NTASK tasks overlap (${elapsed}ms vs ${base}ms for one)"
    else
        bad "[$mode] $desc: the $NTASK tasks did NOT overlap: ${elapsed}ms vs ${base}ms for one (bound 2.5x; 4x means sequential)"
    fi
}

for mode_spec in "::debug" "--release::release"; do
    flags="${mode_spec%%::*}"; mode="${mode_spec##*::}"
    run_cell annot_forin    "$flags" "$mode" "var ts: [int] = []  +  for t in ts { await t }"
    run_cell annot_awaitall "$flags" "$mode" "var ts: [int] = []  +  await_all(ts)"
    run_cell infer_forin    "$flags" "$mode" "var ts = []         +  for t in ts { await t }"
    run_cell infer_awaitall "$flags" "$mode" "var ts = []         +  await_all(ts)"
done

# --- The packed representation must not escape the variable that opted into it ---
#
# The three tables above are keyed on the variable NAME alone, and the spawn-future
# one is program-wide with no per-function reset, so a same-named array in an
# UNRELATED function inherited the packed representation. Before the four cells
# agreed, this program did not compile at all, which hid it; once they agreed it
# compiled and printed a raw pointer:
#
#     first=4378051595        instead of        first=p
#
# A packed array is a long long*, so it can only hold ints. The declaration refuses
# it for a known non-int element type AND records that refusal, so the accessors in
# that function agree - a declaration that refused while the accessors still read
# the program-wide table is the same store/load disagreement one layer down, and is
# how this arm failed on the first attempt.
cat > "$TMP/leak.wyn" <<WYN
fn nap(ms: int) -> int { Time.sleep(ms); return ms }

fn collect() -> int {
    var xs = []
    for i in 0..2 { xs.push(spawn nap(5)) }
    var s = 0
    for t in xs { s = s + await t }
    return s
}

// Same NAME, unrelated function, strings. Every use here (index, len) is one the
// packed form CAN express, so the int-array veto does not rescue it - the element
// type is the only thing that can.
fn strings() -> int {
    var xs = ["p", "q"]
    print("FIRST \${xs[0]}")
    print("LAST \${xs[1]}")
    return xs.len()
}

fn main() -> int {
    print("COLLECT \${collect()}")
    print("LEN \${strings()}")
    return 0
}
WYN

run_leak() {  # $1 = flags, $2 = mode label
    local flags="$1" mode="$2" dir="$TMP/leak-$2"
    mkdir -p "$dir"; cp "$TMP/leak.wyn" "$dir/leak.wyn"
    if ! (cd "$dir" && perl -e 'alarm(600); exec @ARGV' -- "$WYN" build $flags "$dir/leak.wyn") \
            > "$dir/build.log" 2>&1; then
        bad "[$mode] a same-named string array in another function: builds"
        grep -m2 -iE "error" "$dir/build.log" | sed 's/^/        /'; return
    fi
    "$dir/leak" >/dev/null 2>&1
    if ! perl -e 'alarm(300); exec @ARGV' -- "$dir/leak" > "$dir/out" 2>&1; then
        bad "[$mode] a same-named string array in another function: runs"; return
    fi
    local first last len
    first=$(awk '$1=="FIRST"{print $2; exit}' "$dir/out")
    last=$(awk '$1=="LAST"{print $2; exit}'   "$dir/out")
    len=$(awk '$1=="LEN"{print $2; exit}'     "$dir/out")
    if [ "${first:-}" = "p" ] && [ "${last:-}" = "q" ] && [ "${len:-}" = "2" ]; then
        ok "[$mode] a same-named string array in another function keeps its elements (p, q, 2)"
    else
        bad "[$mode] a same-named string array inherited the packed representation: first='${first:-}' last='${last:-}' len='${len:-}' (want p q 2)"
        sed 's/^/        /' "$dir/out"
    fi
}
run_leak ""          debug
run_leak "--release" release

echo ""; echo "future-array-typing: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
