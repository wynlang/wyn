#!/bin/bash
# ONE serialized `make test` run for a lane, timestamped.
#
# WHY TIMESTAMPS ARE THE POINT OF THIS SCRIPT
# -------------------------------------------
# The earlier version of this harness lived outside any git repo and emitted no
# timestamps at all. The direct consequence: three documents gave three different
# durations for the same `make test` - "~9 minutes" (Makefile), "~25 min" (the team
# playbook), and a real median of ~38 min, which could only be recovered after the
# fact from the birth->mtime delta of the log files. A measurement harness that does
# not stamp its own output cannot settle an argument about how long anything takes,
# so every say() line now carries an ISO-8601 UTC stamp plus elapsed seconds, and the
# run prints a total at the end.
#
# It also keeps the guard the original was written for: a previous session's
# validate_lane.sh was STILL running `make test` in the same clone, writing the same
# log path. Two concurrent suites in one tree make both results worthless (and
# parser-stability flakes at rc=142 under load, which reads exactly like a
# regression).
#
# usage: suite.sh [lane-dir] [tag]
#   lane-dir  the wyn repo/worktree to build and test   (default: this script's repo)
#   tag       label for the log and scratch dir         (default: short HEAD sha)
# env:
#   WYN_SUITE_OUT        where logs and scratch go (default: $TMPDIR/wyn-suite)
#   WYN_SUITE_TEST_CMD   the gate to run (default: "make test"). This seam exists so
#                        the script itself can be exercised end to end - including the
#                        timestamps and the TOTAL ELAPSED line - against a cheap real
#                        target such as "make check-fast" (12-14s, timed twice here)
#                        instead of the 38-minute suite. A harness nobody can afford to
#                        run is a harness nobody verifies.
#   WYN_SUITE_MIN_PASS   floor on the number of passes counted across all tally lines
#                        (default 300; `make test` reports 311+68 from its two summary
#                        lines alone). Set it to 30 for a `make check-fast` seam run.
#
# EXIT STATUS IS PART OF THE CONTRACT: 0 only for a green suite, 1 for not-green,
# 2 for a bad lane dir, 3 when another suite is already live. Until 2026-10-05 this
# script could not exit nonzero at all - the verdict was
# `[ … ] && say GREEN || say "NOT GREEN"`, and `say` returns 0 and was the last
# command in the file, so a red suite was labelled correctly and still reported
# success to its caller. Three of the four verdicts it could emit were wrong: it also
# called a watchdog-KILLED suite green (the signal count was printed and then
# excluded from the verdict) and called a run in which ZERO tests executed green
# (the verdict rested on the absence of a failure string, not the presence of a
# result). Fixing those is why the floor and the per-arm FAIL lines below exist.
# Nothing here hardcodes an absolute path; the defaults are derived from the
# script's own location, the same way scripts/flagship_apps_gate.sh does it.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_LANE="$(dirname "$SCRIPT_DIR")"

usage() {
    # Print the whole leading comment block, however long it grows: a hardcoded line
    # range silently truncates the help the moment someone adds a paragraph.
    awk 'NR>1 { if ($0 !~ /^#/) exit; sub(/^# ?/, ""); print }' "${BASH_SOURCE[0]}"
    echo
    echo "resolved defaults on this machine:"
    echo "  lane-dir : $DEFAULT_LANE"
    echo "  out dir  : ${WYN_SUITE_OUT:-${TMPDIR:-/tmp}/wyn-suite}"
    echo "  stamp    : $(date -u +%FT%TZ)   <- every log line carries one of these"
}

case "${1:-}" in
    -h|--help|help) usage; exit 0 ;;
esac

LANE="$(cd "${1:-$DEFAULT_LANE}" 2>/dev/null && pwd)" || {
    echo "suite.sh: lane dir does not exist: ${1:-$DEFAULT_LANE}" >&2; exit 2; }
TAG="${2:-$(git -C "$LANE" rev-parse --short HEAD 2>/dev/null || echo adhoc)}"

OUT="${WYN_SUITE_OUT:-${TMPDIR:-/tmp}/wyn-suite}"
TD="$OUT/tmp/suite-$TAG"
LOG="$OUT/suite-$TAG.log"
mkdir -p "$OUT"
rm -rf "$TD"; mkdir -p "$TD"
: > "$LOG"

START=$(date +%s)
# ISO-8601 UTC + elapsed seconds on every line. Without this the log cannot answer
# "how long did the suite take", which is the only question it is run to answer.
say() { printf '%s +%04ds %s\n' "$(date -u +%FT%TZ)" "$(( $(date +%s) - START ))" "$*" | tee -a "$LOG"; }

say "=== suite $TAG  lane=$LANE"
say "    log=$LOG  scratch=$TD"
say "    head=$(git -C "$LANE" log --oneline -1 2>/dev/null || echo 'not a git checkout')"

# --- guard: refuse to start if anything else is running a suite
STRAY=$(pgrep -fl 'make test|validate_lane|run_bdd\.sh' | grep -v "suite.sh" | head -5)
if [ -n "$STRAY" ]; then
    say "REFUSING TO START - another suite is live:"
    say "$STRAY"
    exit 3
fi

export TMPDIR="$TD" WYN_ROOT="$LANE"
cd "$LANE" || exit 2

say "--- fresh build (two concurrent makes may have left partial objects)"
OLD=$(md5 -q wyn 2>/dev/null || md5sum wyn 2>/dev/null | cut -d' ' -f1 || echo none)
rm -f wyn runtime/obj/*.o 2>/dev/null
if ! make > "$TD/build.log" 2>&1; then
    say "BUILD FAILED"; grep -m8 -iE 'error' "$TD/build.log" | tee -a "$LOG"; exit 1
fi
W=$(grep -ci warning "$TD/build.log")
NEW=$(md5 -q wyn 2>/dev/null || md5sum wyn 2>/dev/null | cut -d' ' -f1)
say "    warnings=$W   md5 $OLD -> $NEW"
[ "$W" -eq 0 ] || { say "FAIL: $W warnings (bar is 0)"; exit 1; }
[ "$OLD" != "$NEW" ] || say "    NOTE: md5 unchanged - build is not reproducible, so this is not proof of anything either way"

TEST_CMD="${WYN_SUITE_TEST_CMD:-make test}"
say "--- full suite (serial): $TEST_CMD"
# Capture the gate's own exit status. Previously this `if` only chose which message
# to print and then threw the status away, so a suite that exited 143 was reported
# with the same confidence as one that exited 0.
$TEST_CMD > "$TD/maketest.log" 2>&1
TRC=$?
say "    $TEST_CMD exited $TRC"
say "    $(grep -oE 'Results: [0-9]+ pass, [0-9]+ fail' "$TD/maketest.log" | tail -1)"
say "    $(grep -oE 'Stdlib results: [0-9]+ pass, [0-9]+ fail[^)]*' "$TD/maketest.log" | tail -1)"
BAD=$(grep -oE '[0-9]+ fail(ed)?' "$TD/maketest.log" | grep -v '^0 fail' | sort -u | tr '\n' ' ')
MK=$(grep -cE 'make(\[[0-9]+\])?: \*\*\*' "$TD/maketest.log")
SIG=$(grep -cE 'Terminated|Killed|rc=1[0-9][0-9]' "$TD/maketest.log")

# EVERY tally line the run emitted, in any of the suite's dialects: "Results: 311
# pass, 0 fail", "Stdlib results: 68 pass, 0 fail", "Golden-C: 30 pass, 0 fail".
# This is the DISCOVERY count that turns "I found no failures" into "I found N
# passes": the verdict used to rest purely on the ABSENCE of a nonzero fail tally,
# so a run that executed nothing - WYN_SUITE_TEST_CMD=true, or a summary whose
# format changed - was indistinguishable from a clean suite. The repo already has
# two standing examples of that shape (run_tests_parallel.sh's green tick over zero
# executed tests; `wyn test` exiting 0 with zero test blocks), which is why the
# floor is asserted here rather than left to the reader.
TALLIES=$(grep -oE '[0-9]+ (pass|passed), *[0-9]+ (fail|failed)' "$TD/maketest.log")
NTALLY=$(printf '%s' "$TALLIES" | grep -c '[0-9]')
PASSES=$(printf '%s\n' "$TALLIES" | grep -oE '^[0-9]+' | awk '{s += $1} END {print s + 0}')
# A FLOOR, not an equality: adding a gate must not red this, removing the whole
# suite must. `make test` has reported 311 + 68 from its two summary lines alone in
# every green run on record, so 300 cannot fire on a correct suite. Override it for
# a cheap seam run - `make check-fast` emits one 30-pass tally, so that needs
# WYN_SUITE_MIN_PASS=30.
MIN_PASS="${WYN_SUITE_MIN_PASS:-300}"
say "    nonzero tallies: ${BAD:-none}   make-level errors: $MK   signal lines: $SIG"
say "    test-cmd rc: $TRC   tally lines found: $NTALLY   passes counted: $PASSES (floor $MIN_PASS)"

ELAPSED=$(( $(date +%s) - START ))
say "=== TOTAL ELAPSED ${ELAPSED}s ($(( ELAPSED / 60 ))m $(( ELAPSED % 60 ))s) for $TAG"

# The verdict. Each arm says WHICH condition failed, and the script EXITS NONZERO so
# a caller that &&-chains it is told the truth: the old one-liner
# (`[ … ] && say GREEN || say "NOT GREEN"`) labelled a red suite correctly and then
# returned 0, because `say` succeeds and was the last command in the file.
GREEN=1
[ "$TRC" -eq 0 ] || { say "FAIL: $TEST_CMD exited $TRC, not 0"; GREEN=0; }
[ -z "$BAD" ]    || { say "FAIL: nonzero fail tallies: $BAD"; GREEN=0; }
[ "$MK" -eq 0 ]  || { say "FAIL: $MK make-level error line(s)"; GREEN=0; }
# A KILLED suite is not a result. The watchdog (ulimit -t / perl alarm) terminates a
# looping test and the run still prints its partial tally, so "Terminated: 15"
# arrives alongside a green-looking "311 pass, 0 fail" - the exact misread recorded
# in this project's notes. SIG was already computed and printed here and then left
# out of the verdict, which is worse than not computing it: the log line made the
# case look covered. Of the 17 archived runs of this harness, the 15 that got far
# enough to print the count all report `signal lines: 0` - including all 11 green ones -
# so this arm cannot fire on a healthy suite. (The other 2 died before the suite step,
# which is exactly why the count is asserted rather than eyeballed.)
[ "$SIG" -eq 0 ] || { say "FAIL: $SIG signal/kill line(s) - the suite was KILLED, so its tally is partial and is NOT a result"; GREEN=0; }
[ "$NTALLY" -ge 1 ] || { say "FAIL: no 'N pass, M fail' tally line anywhere in the log - the suite did not run, or its summary format changed"; GREEN=0; }
[ "$PASSES" -ge "$MIN_PASS" ] || { say "FAIL: counted $PASSES passes over $NTALLY tally line(s), floor is $MIN_PASS (set WYN_SUITE_MIN_PASS to run a cheaper gate deliberately)"; GREEN=0; }

if [ "$GREEN" -eq 1 ]; then
    say "=== $TAG SUITE GREEN - $PASSES passes over $NTALLY tally line(s), rc=0, no signals"
else
    say "=== $TAG SUITE NOT GREEN - read $TD/maketest.log"
    exit 1
fi
