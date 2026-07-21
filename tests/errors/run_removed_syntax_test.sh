#!/bin/bash
# Negative-syntax regression: removed operators must produce a clean, fast error -
# NOT an infinite parser loop (regression for the &&/|| condition hang, 2026-07).
# Each case: `wyn check` must (a) finish within a timeout, (b) exit non-zero,
# (c) print the expected "has been removed" guidance.
set -uo pipefail

WYN="${WYN:-./wyn}"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
PASS=0
FAIL=0

# Portable N-second guard (this dev box + CI runners have no `timeout` binary).
run_guarded() {
    local secs="$1"; shift
    ( "$@" ) & local pid=$!
    ( sleep "$secs"; kill -9 "$pid" 2>/dev/null ) & local killer=$!
    wait "$pid" 2>/dev/null; local code=$?
    kill "$killer" 2>/dev/null; wait "$killer" 2>/dev/null
    return "$code"
}

check_case() {
    local name="$1" src="$2" needle="$3"
    local f="$TMP/$name.wyn"
    printf '%s\n' "$src" > "$f"
    local out
    out=$(run_guarded 8 "$WYN" check "$f" 2>&1)
    local code=$?
    if [ "$code" -eq 137 ]; then
        echo "  FAIL  $name - HUNG (killed after 8s)"; FAIL=$((FAIL+1)); return
    fi
    if [ "$code" -eq 0 ]; then
        echo "  FAIL  $name - accepted removed syntax (exit 0)"; FAIL=$((FAIL+1)); return
    fi
    if ! echo "$out" | grep -q "$needle"; then
        echo "  FAIL  $name - missing guidance '$needle'"; FAIL=$((FAIL+1)); return
    fi
    echo "  ok    $name"; PASS=$((PASS+1))
}

# && / || in an if-condition were the exact hang trigger.
check_case "ampamp_if"  'fn main() {
  var a = 1
  if a > 0 && a < 5 { print("x") }
}' "the operator '&&' has been removed"

check_case "pipepipe_if" 'fn main() {
  var a = 1
  if a > 0 || a < 5 { print("x") }
}' "the operator '||' has been removed"

# Chained form must drain (report) rather than loop.
check_case "or_chain" 'fn main() {
  var a = 1
  if a > 0 || a < 5 || a == 3 { print("x") }
}' "the operator '||' has been removed"

# The already-removed |> stays covered here too.
check_case "pipe_op" 'fn main() {
  var x = [1, 2, 3]
  var y = x |> len
  print(y)
}' "the pipe operator '|>' has been removed"

echo ""
echo "removed-syntax: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ]
