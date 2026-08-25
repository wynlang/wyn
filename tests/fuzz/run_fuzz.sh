#!/bin/bash
# Fuzz harness (T1.4): generate grammar-based + mutated Wyn programs and feed
# them to `wyn check` and `wyn build`. Invariants:
#   1. Neither command may CRASH (die on a signal) on ANY input.
#   2. Neither command may HANG (>TIMEOUT s) on ANY input.
#   3. A program that PASSES `wyn check` must BUILD without an internal
#      codegen error or a C-compiler error leaking through.
# Usage: run_fuzz.sh [seed] [count]   (defaults: seed=1 count=120)
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
SEED="${1:-1}"
COUNT="${2:-120}"
# Per-input wall-clock budget for `wyn check`/`wyn build`. This exists to catch a
# genuine INFINITE loop (parser/checker non-termination), where the gap between
# "hung forever" and "finished" is enormous - not to police a few hundred ms. A
# real repro here checks/builds in well under a second; anything near the budget
# is the shared CI runner being contended, not the compiler looping. It was 12s,
# which flaked one input to a false HANG on a busy macos-15-intel runner (the
# input parse-errors in 0.7s locally). 30s keeps the infinite-loop signal while
# giving the slowest borrowed core plenty of head-room. Override with FUZZ_TIMEOUT.
TIMEOUT="${FUZZ_TIMEOUT:-30}"
HERE="$(cd "$(dirname "$0")" && pwd)"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT

python3 "$HERE/gen_fuzz.py" "$SEED" "$COUNT" "$TMP/corpus" >/dev/null

# Portable timeout via perl alarm (macOS has no `timeout`); 124 = timed out.
run_to() { perl -e 'alarm shift; exec @ARGV or exit 127' "$TIMEOUT" "$@"; }

CRASH=0; HANG=0; ICE=0; CHECKED=0; BUILT=0; REJECTED=0
FAILED_FILES=""

cd "$TMP"
for f in corpus/*.wyn; do
    run_to "$WYN" check "$f" > check.log 2>&1
    rc=$?
    if [ $rc -ge 128 ]; then
        sig=$((rc - 128))
        if [ "$sig" = 14 ]; then HANG=$((HANG+1)); else CRASH=$((CRASH+1)); fi
        FAILED_FILES="$FAILED_FILES check:$f(rc=$rc)"
        cp "$f" "$TMP/$(basename "$f").checkfail" 2>/dev/null
        continue
    fi
    if [ $rc -ne 0 ]; then REJECTED=$((REJECTED+1)); continue; fi
    CHECKED=$((CHECKED+1))
    # check passed -> must build cleanly
    run_to "$WYN" build "$f" > build.log 2>&1
    rc=$?
    if [ $rc -ge 128 ]; then
        sig=$((rc - 128))
        if [ "$sig" = 14 ]; then HANG=$((HANG+1)); else CRASH=$((CRASH+1)); fi
        FAILED_FILES="$FAILED_FILES build:$f(rc=$rc)"
    elif [ $rc -ne 0 ]; then
        # ANY build failure after `wyn check` passed is a soundness violation.
        #
        # This used to flag one only when build.log matched an ALLOWLIST of four
        # C-error phrasings ("internal codegen error", "error: incompatible",
        # "error: use of undeclared", "ld: symbol") and counted everything else as
        # a "clean rejection at build stage". There is no such thing: the invariant
        # in this file's own header is "a program that PASSES check must BUILD", so
        # the allowlist was silently excusing exactly the class it exists to catch.
        # It had been excusing a real one - `-71.to_string()` fails with "invalid
        # argument type 'char *' to unary expression", which is not in the list, so
        # every run reported 0 violations while the defect sat in the corpus.
        # Fixed in the preceding commit; the allowlist is why nobody knew.
        #
        # A new C-error phrasing must never be able to buy silence, so the test is
        # now the rc, not the wording.
        ICE=$((ICE+1))
        FAILED_FILES="$FAILED_FILES buildfail:$f"
    else
        BUILT=$((BUILT+1))
    fi
    rm -f "${f%.wyn}" "$f.out" "${f%.wyn}.wyn.c" 2>/dev/null
done

echo ""
echo "fuzz(seed=$SEED,n=$COUNT): $CHECKED check-pass, $BUILT built, $REJECTED cleanly rejected"
echo "fuzz invariants: $CRASH crash, $HANG hang, $ICE check-passed-but-build-failed"
if [ -n "$FAILED_FILES" ]; then
    echo "FAILING INPUTS:$FAILED_FILES"
    # Preserve repros for debugging.
    KEEP="${FUZZ_KEEP_DIR:-/tmp/wyn_fuzz_failures}"
    mkdir -p "$KEEP"
    for spec in $FAILED_FILES; do
        f="${spec#*:}"; f="${f%%(*}"
        cp "$f" "$KEEP/" 2>/dev/null
    done
    echo "repros copied to $KEEP"
fi
[ $((CRASH + HANG + ICE)) -eq 0 ]
