#!/bin/bash
# THE v1.21 ACCEPTANCE GATE - PLAN_v1.21 §10.
#
# §10 defines ONE test as the measure of the release, not a checklist, and states
# why: "failures concentrate at composition boundaries, so twenty working 25-line
# programs do not compose into one working 500-line program." Every unit suite was
# green while the #43 cluster shipped, and again while the seven
# check-passes/build-fails defects of 2026-08-25 were live.
#
# So this gate builds and RUNS one realistic CLI tool that does all of the
# following at once, in one translation unit:
#
#   1. reads stdin            4. propagates errors across DIFFERENT Result families
#   2. parses JSON            5. passes structs across function boundaries
#   3. formats numbers        6. uses a HashMap from a spawned handler
#
# IT HAS ALREADY EARNED ITS PLACE. Its first ~200 lines found three defects that
# every other gate missed - HashMap's typed setters corrupting the heap (#310),
# Ok/Err/Some/None unusable as enum variant names (#311), and Json's writer methods
# emitting nothing (#312). It did not even BUILD before those landed.
#
# Keep it written in ordinary idiomatic Wyn. If it ever needs a workaround, that
# workaround IS a finding - that property is the entire mechanism, and rewriting the
# tool to dodge a defect would silently retire the gate.
#
# The expected output lives in the `// EXPECT:` lines of cli_tool.wyn, so the tool
# and its contract stay in one file.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$HERE/cli_tool.wyn"
IN="$HERE/input.txt"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# 1. It must type-check.
out=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" check "$SRC" 2>&1)
if [ $? -eq 0 ]; then ok "acceptance tool type-checks"
else bad "acceptance tool type-checks [$(echo "$out" | head -3 | tr '\n' ' ')]"; fi

# 2. It must BUILD. This is the check-passes-must-build contract on a program big
#    enough to cross every boundary at once - `wyn check` alone would have passed
#    while three of the defects above were live.
if perl -e 'alarm(180); exec @ARGV' -- "$WYN" build "$SRC" -o "$TMP/cli_tool" >"$TMP/build.log" 2>&1; then
    ok "acceptance tool builds"
else
    bad "acceptance tool builds [$(grep -m2 'error:' "$TMP/build.log" | tr '\n' ' ')]"
    echo ""; echo "acceptance: $PASS pass, $FAIL fail"; exit 1
fi

# 3. It must produce the RIGHT ANSWERS. A composition test that only builds proves
#    very little: the HashMap defect it found built fine and returned zeros.
expected=$(grep '^// EXPECT: ' "$SRC" | sed 's|^// EXPECT: ||')
actual=$(perl -e 'alarm(120); exec @ARGV' -- "$TMP/cli_tool" < "$IN" 2>&1)
rc=$?
if [ $rc -ne 0 ]; then
    bad "acceptance tool runs (rc=$rc) [$(echo "$actual" | tail -2 | tr '\n' ' ')]"
elif [ "$actual" = "$expected" ]; then
    ok "acceptance tool output matches all $(echo "$expected" | wc -l | tr -d ' ') expected lines"
else
    bad "acceptance tool output differs"
    diff <(printf '%s\n' "$expected") <(printf '%s\n' "$actual") | head -12
fi

echo ""
echo "acceptance: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
