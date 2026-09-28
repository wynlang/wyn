#!/bin/bash
# string .len() must be O(1), for EVERY string - not just the ones whose
# constructor remembered to fill the length cache.
#
# THE DEFECT THIS GATES (measured on dev @ 11637e24, macOS arm64, 200k calls):
#
#     "y".repeat(100000).len()      ->    5 ns/call   (cached in the RC header)
#     sb_of(100000).len()           -> 2560 ns/call   (O(n) strlen, ~500x slower)
#     sb_of(10000).len()            ->  270 ns/call
#     sb_of(1000).len()             ->   30 ns/call   ... i.e. linear in length
#
# and it was never only StringBuilder. Same program, n=100000:
#
#     repeat 5ns   upper 5ns   substring 5ns   join 5ns      <- cached
#     trim 2470ns  replace 2500ns  capitalize 2475ns
#     interpolation 2465ns                                   <- O(n) scan
#
# Root cause: the RC header caches the length, string_length() uses it when it is
# non-zero and falls back to strlen() when it is not - and 36 of the runtime's
# string constructors never set it. Whoever writes the 37th will not either. So
# the cache is filled in string_length() itself on a miss: one place, every
# constructor, present and future.
#
# WHY THE ARMS BELOW ARE SHAPED THIS WAY
#  - The cost arm asserts a RATIO across three lengths, not an absolute time: a
#    shared CI runner's absolute numbers are worthless (documented at the idle-CPU
#    budget in scripts/integration_gates.sh), but contention scales all three
#    readings together. An O(n) scan shows ~85x between n=1000 and n=100000; a
#    cached read shows ~1x. The bound is 8x - generous enough not to flake,
#    nowhere near enough to let the defect back in.
#  - TRIM is measured as well as SB *on purpose*. The ticket blamed
#    StringBuilder.to_string(); patching that one site leaves the other 35
#    constructors broken, and this arm is what says so.
#  - The UTF-8 arm cannot fail from the defect (a strlen IS the right answer, just
#    slow). It is here to catch the FIX caching a wrong number, which is the only
#    way this change could produce a silent wrong answer.
#  - The LITERAL arm is correctness-only, deliberately. A literal lives in rodata
#    and can never hold a cached length, so it always takes the strlen path and
#    cannot regress in the O(n) direction this file's ratio arms watch for. What it
#    CAN regress is constant factor, and that happened: filling the cache on a miss
#    (the fix described above) originally called wyn_rc_get_length then
#    wyn_rc_set_length, validating the RC header twice, and a literal misses every
#    call - so literals paid for a cache they can never use. Measured, macOS arm64,
#    1M calls on a 44-char literal, shipped -O2 artifacts:
#
#        v1.21.0 (no memoization)          1.54ms
#        v1.22.0-rc1 (memoized, 2 checks)  2.69ms   <- 1.75x slower
#        one-validation probe              1.55ms   <- restored
#
#    That is NOT gated by a ratio arm here on purpose: the signal is 1.75x, so the
#    bound would have to sit near 1.4x to catch it, which is far too tight to run on
#    a shared runner without flaking. A gate that is permanently amber teaches people
#    to ignore this file. The property is covered by the committed benchmark harness
#    instead; this arm only asserts the literal receiver still returns the right
#    number through the new probe path, including the WYN_RC_NOT_CACHEABLE sentinel.
#
# NOTE FOR ANYONE RE-RUNNING THIS: string_length lives in the RUNTIME, precompiled
# into runtime/libwyn_rt.a. After editing wyn_runtime.h you must
# `rm -f runtime/obj/*.o && make runtime`, or compiled programs keep the old
# behaviour and this gate appears to fail against a correct source tree.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

ITERS=1000000
RATIO_BOUND=8

cat > "$TMP/lenbench.wyn" <<WYN
fn sb_of(n: int) -> string {
    sb = StringBuilder.new()
    i = 0
    while i < n {
        sb.append("y")
        i = i + 1
    }
    return sb.to_string()
}

fn trim_of(n: int) -> string {
    return ("  " + "y".repeat(n) + "  ").trim()
}

fn timed_ms(s: string, iters: int) -> int {
    t0 = Time.now_millis()
    total = 0
    i = 0
    while i < iters {
        total = total + s.len()
        i = i + 1
    }
    t1 = Time.now_millis()
    if total < 0 { print("unreachable") }
    return t1 - t0
}

// Three reps, first discarded, MIN of the rest. min is the robust estimator for a
// microbenchmark: noise only ever adds time, so the minimum is the closest reading
// to the real cost. Discarding run 1 also pays first-touch page faults once.
fn best_ms(s: string, iters: int) -> int {
    warm = timed_ms(s, iters)
    if warm < 0 { print("unreachable") }
    a = timed_ms(s, iters)
    b = timed_ms(s, iters)
    if a < b { return a }
    return b
}

fn main() {
    iters = $ITERS

    print("SB 1000 \${best_ms(sb_of(1000), iters)}")
    print("SB 10000 \${best_ms(sb_of(10000), iters)}")
    print("SB 100000 \${best_ms(sb_of(100000), iters)}")
    print("TRIM 1000 \${best_ms(trim_of(1000), iters)}")
    print("TRIM 10000 \${best_ms(trim_of(10000), iters)}")
    print("TRIM 100000 \${best_ms(trim_of(100000), iters)}")

    // --- correctness of the cached value, multi-byte UTF-8 ------------------
    // unit is 17 BYTES and 13 code points, so a length taken in the wrong unit,
    // or truncated at a continuation byte, cannot coincide with the right answer.
    unit = "héllo wörld ✓"
    k = 400
    sb = StringBuilder.new()
    i = 0
    while i < k {
        sb.append(unit)
        i = i + 1
    }
    built = sb.to_string()          // a constructor that did NOT cache
    spun = unit.repeat(k)           // a constructor that DID cache
    print("UTF8 \${built.len()} \${spun.len()} \${unit.len() * k} \${built.len()}")
    print("UTF8TAIL \${built.substring(unit.len() * k - 3, unit.len() * k)}")

    // --- literal receiver: never cacheable, must still be correct -----------
    // Goes through the same probe as an RC string but hits the
    // WYN_RC_NOT_CACHEABLE sentinel, so a sentinel confused with a real length
    // would show up here as a wrong number or an empty-string verdict.
    lit = "héllo wörld ✓"
    empty = ""
    print("LIT \${lit.len()} \${lit.len()} \${empty.len()} \${"abc".len()}")
    // Same bytes, one cacheable and one not: they must agree.
    print("LITVSRC \${lit.len()} \${lit.repeat(1).len()}")
}
WYN

out="$TMP/bench.out"
if ! (cd "$TMP" && perl -e 'alarm(300); exec @ARGV' -- "$WYN" run "$TMP/lenbench.wyn") \
        > "$out" 2>"$TMP/bench.err"; then
    bad "benchmark program runs"
    head -5 "$TMP/bench.err" | sed 's/^/        /'
    echo ""; echo "len-cache: $PASS pass, $FAIL fail"; exit 1
fi

read_ms() { awk -v k="$1" -v n="$2" '$1==k && $2==n {print $3}' "$out"; }

check_flat() {   # $1=KIND  $2=human label
    local small mid large total
    small=$(read_ms "$1" 1000); mid=$(read_ms "$1" 10000); large=$(read_ms "$1" 100000)
    if [ -z "$small" ] || [ -z "$mid" ] || [ -z "$large" ]; then
        bad "$2 (no reading: small=$small mid=$mid large=$large)"; return
    fi
    total=$((small + mid + large))
    # A gate whose every reading is 0ms cannot fail. Say so instead of passing.
    if [ "$total" -lt 3 ]; then
        bad "$2 (readings too small to mean anything: ${small}/${mid}/${large}ms for $ITERS calls - raise ITERS)"
        return
    fi
    local floor=$small
    [ "$floor" -lt 1 ] && floor=1
    if [ "$large" -le $((RATIO_BOUND * floor)) ] && [ "$mid" -le $((RATIO_BOUND * floor)) ]; then
        ok "$2 (n=1000 ${small}ms, n=10000 ${mid}ms, n=100000 ${large}ms per $ITERS calls)"
    else
        bad "$2 - cost GROWS with length: n=1000 ${small}ms, n=10000 ${mid}ms, n=100000 ${large}ms per $ITERS calls (bound ${RATIO_BOUND}x)"
    fi
}

check_flat SB   ".len() on a StringBuilder.to_string() result does not grow with length"
# The anti-per-site arm: .trim() is a DIFFERENT constructor. A patch to
# StringBuilder_to_string alone leaves this one red.
check_flat TRIM ".len() on a .trim() result does not grow with length"

# --- the cached length must be the RIGHT length -----------------------------
utf8=$(awk '$1=="UTF8" {print $2, $3, $4, $5}' "$out")
set -- $utf8
if [ $# -ne 4 ]; then
    bad "cached length equals the scanned length for multi-byte UTF-8 (no reading)"
elif [ "$1" = "$2" ] && [ "$1" = "$3" ] && [ "$1" = "$4" ]; then
    ok "cached length equals the scanned length for multi-byte UTF-8 ($1 bytes, stable across calls)"
else
    bad "cached length for multi-byte UTF-8: sb=$1 repeat=$2 expected=$3 second-call=$4"
fi

# A byte length that was truncated at a UTF-8 continuation boundary would slice
# the last character apart; assert the final 3 bytes are still the whole "✓".
tail3=$(awk '$1=="UTF8TAIL" {print $2}' "$out")
if [ "$tail3" = "✓" ]; then ok "the cached byte length still addresses whole characters"
else bad "the cached byte length still addresses whole characters (got '$tail3')"; fi

# --- wyn build must agree with wyn run --------------------------------------
# Different paths through the runtime header (slim vs full); a stale
# runtime/libwyn_rt.a shows up as exactly one of the two being right.
build_test() {
    local f="$TMP/b.wyn"
    cat > "$f" <<'WYN'
fn main() {
    sb = StringBuilder.new()
    i = 0
    while i < 5000 {
        sb.append("héllo ")
        i = i + 1
    }
    s = sb.to_string()
    print("${s.len()} ${s.len()}")
}
WYN
    local out2 rc
    out2=$(cd "$TMP" && perl -e 'alarm(120); exec @ARGV' -- "$WYN" build "$f" 2>&1); rc=$?
    if [ $rc -ne 0 ]; then bad "wyn build agrees with wyn run (build rc=$rc)"; return; fi
    local bin="${f%.wyn}"
    if [ ! -x "$bin" ]; then bad "wyn build agrees with wyn run (no binary at $bin)"; return; fi
    out2=$(perl -e 'alarm(60); exec @ARGV' -- "$bin" 2>&1)
    if [ "$out2" = "35000 35000" ]; then ok "wyn build agrees with wyn run"
    else bad "wyn build agrees with wyn run (want '35000 35000', got '$out2')"; fi
}
build_test

# --- a literal receiver returns the right length through the probe ------------
lit=$(awk '$1=="LIT" {print $2, $3, $4, $5}' "$out")
set -- $lit
if [ $# -ne 4 ]; then
    bad "literal receiver .len() (no reading)"
elif [ "$1" = "17" ] && [ "$2" = "17" ] && [ "$3" = "0" ] && [ "$4" = "3" ]; then
    ok "literal receiver .len() is correct and stable ($1 bytes, empty=0, abc=3)"
else
    bad "literal receiver .len(): got '$1' '$2' empty='$3' abc='$4' (want 17 17 0 3)"
fi

# A cacheable and a non-cacheable string with identical bytes must agree - this is
# what a sentinel leaking into the returned value would break.
lvr=$(awk '$1=="LITVSRC" {print $2, $3}' "$out")
set -- $lvr
if [ $# -eq 2 ] && [ "$1" = "$2" ]; then
    ok "literal and RC-managed copies of the same bytes agree ($1)"
else
    bad "literal vs RC-managed length disagree: literal='${1:-}' rc='${2:-}'"
fi

echo ""; echo "len-cache: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
