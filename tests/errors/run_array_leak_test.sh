#!/bin/bash
# A PACKED [int] LOCAL MUST NOT LEAK, AND NO CORRECTNESS TEST CAN SEE THAT.
#
# THE BUG (#466). A `[int]` local that kept the packed WynIntArray representation was
# never freed. Measured in the Linux container before the fix:
#
#     50,000 calls  ->  9,700 KB max RSS
#    500,000 calls  -> 71,692 KB max RSS        linear in the call count
#
# while the homepage says memory is "reference counted and freed at scope exit". There
# was no packed free function in the runtime at all; `array_free` covers the generic
# WynArray and is only emitted for INNER blocks, and a function body is not one.
#
# WHY A RATIO AND NOT AN ABSOLUTE BOUND. Absolute RSS depends on the platform, the
# allocator, the page size and what else the process touched, so a fixed KB figure
# would be a per-platform fudge factor that drifts. The invariant is SHAPE: ten times
# the calls must not cost ten times the memory. The leak was ~7.4x, so a 2x ceiling is
# both far from the leak and far from normal jitter.
#
# EACH MEASUREMENT RUNS IN ITS OWN PYTHON PROCESS. getrusage(RUSAGE_CHILDREN).ru_maxrss
# is a HIGH-WATER MARK across every child the process has reaped, so measuring both
# binaries from one parent makes the second reading include the first and the ratio
# collapses towards 1 - i.e. the gate would pass on the leak. Found while taking the
# baseline above. (The unit differs by platform - KB on Linux, bytes on macOS - which
# does not matter to a ratio.)
#
# THE ANSWER IS ASSERTED TOO. Without that, freeing the array too early - the failure
# mode that actually matters here, since a premature free in this area is worse than
# the leak - would show up as a beautifully flat RSS and a wrong number.
#
# Three arms, one per exit path codegen has to get right:
#   1  `return a[3]`     the reported repro: the return expression READS the array it
#                        is about to free, so the release must follow the value
#   2  fall-through      a function that ends without a return
#   3  inner block       declared inside a loop body, released per iteration
set -uo pipefail

WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d)
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

echo "=== packed [int] locals must not leak (#466) ==="

if ! command -v python3 >/dev/null 2>&1; then
    echo "  skip  python3 not available (needed to read max RSS)"
    echo ""
    echo "array-leak: $PASS pass, $FAIL fail"
    exit 0
fi

# One child, one fresh parent, one number.
measure() {
    python3 -c '
import resource, subprocess, sys
r = subprocess.run([sys.argv[1]], capture_output=True)
sys.stdout.write("%d %s" % (
    resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss,
    r.stdout.decode().strip().splitlines()[-1] if r.stdout.strip() else "NO-OUTPUT"))
' "$1"
}

# $1 label  $2 expected-answer  $3 wyn source with __N__ placeholder
check_flat() {
    local label="$1" want="$2" src="$3"
    local small=50000 big=500000
    local f_small="$TMP/${label}_s.wyn" f_big="$TMP/${label}_b.wyn"
    printf '%s' "$src" | sed "s/__N__/$small/" > "$f_small"
    printf '%s' "$src" | sed "s/__N__/$big/"   > "$f_big"

    local b_small="$TMP/${label}_s.out" b_big="$TMP/${label}_b.out"
    if ! TMPDIR="$TMP" perl -e 'alarm(180); exec @ARGV' -- "$WYN" build "$f_small" -o "$b_small" > "$TMP/$label.build" 2>&1 ||
       ! TMPDIR="$TMP" perl -e 'alarm(180); exec @ARGV' -- "$WYN" build "$f_big"   -o "$b_big"  >> "$TMP/$label.build" 2>&1; then
        bad "$label: builds"
        grep -iE 'error' "$TMP/$label.build" | head -3 | sed 's/^/          /'
        return
    fi

    local rs rb rss_s rss_b ans_s ans_b
    rs=$(measure "$b_small"); rb=$(measure "$b_big")
    rss_s=${rs%% *}; ans_s=${rs#* }
    rss_b=${rb%% *}; ans_b=${rb#* }

    # The answer first: a flat RSS with a wrong number is the premature-free failure.
    if [ "$ans_s" = "$want" ] && [ "$ans_b" != "NO-OUTPUT" ]; then
        ok "$label: answer is still correct ($ans_s)"
    else
        bad "$label: answer is still correct (want $want at N=$small, got '$ans_s'; N=$big got '$ans_b')"
        return
    fi

    if [ "$rss_s" -le 0 ] 2>/dev/null; then
        bad "$label: max RSS could be read (got '$rss_s')"
        return
    fi
    # 10x the calls must cost under 2x the memory.
    if python3 -c "import sys; sys.exit(0 if $rss_b < 2 * $rss_s else 1)"; then
        ok "$label: RSS is flat in the call count ($rss_s -> $rss_b, 10x calls)"
    else
        bad "$label: RSS grows with the call count ($rss_s -> $rss_b for 10x calls, ceiling 2x)"
    fi
}

# --- 1. the reported repro: the return expression reads the array ------------
check_flat "repro" "1250125000" 'fn handle(n: int) -> int {
    var a: [int] = []
    var k = 0
    while k < 10 { a.push(n + k); k = k + 1 }
    return a[3]
}
fn main() {
    var i = 0
    var acc = 0
    while i < __N__ { acc = acc + handle(i); i = i + 1 }
    print(acc)
}
'

# --- 2. a function that falls through without a return ----------------------
check_flat "fallthrough" "50000" 'var total = 0
fn fill(n: int) {
    var a: [int] = []
    var k = 0
    while k < 10 { a.push(n + k); k = k + 1 }
    total = total + 1
}
fn main() {
    var i = 0
    while i < __N__ { fill(i); i = i + 1 }
    print(total)
}
'

# --- 3. declared in an inner block, released per iteration ------------------
# Only 500 outer calls x 100 inner iterations, because the point here is that the
# release happens per ITERATION rather than per call - 50,000 block entries either
# way, and the ratio still answers the question.
check_flat "innerblock" "50000" 'fn run(n: int) -> int {
    var seen = 0
    var j = 0
    while j < 100 {
        var a: [int] = []
        var k = 0
        while k < 10 { a.push(j + k); k = k + 1 }
        seen = seen + 1
        j = j + 1
    }
    return seen
}
fn main() {
    var i = 0
    var acc = 0
    while i < (__N__ / 100) { acc = acc + run(i); i = i + 1 }
    print(acc)
}
'

echo ""
echo "array-leak: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
