#!/bin/bash
# Regression shell test for two HIGH-severity runtime bugs (// EXPECT can't
# assert non-zero exit codes, so these live here):
#
#   BUG 1  Shared.* had no handle bounds-checking → OOB read/write on the global
#          atomic slab (ASan: global-buffer-overflow). An out-of-range handle now
#          panics with a clear message and exits non-zero (unless lenient).
#   BUG 2  Task.channel(0) built a capacity-0 channel that could never deliver
#          (size < capacity is never true) → permanent hang. It now rejects
#          capacity < 1 at the constructor with a clean panic instead of hanging.
#
# Each program is wrapped in a wall-clock alarm so a REGRESSION (a return of the
# hang) fails loudly instead of blocking the suite forever.
set -uo pipefail

WYN="${WYN:-./wyn}"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
FAILS=0

# Bounded runner: alarm N seconds, capture stdout+stderr, report exit code.
run_bounded() {  # $1=seconds $2=binary...
    local secs="$1"; shift
    perl -e 'alarm shift; exec @ARGV or exit 127' "$secs" "$@" 2>&1
    return $?
}

check() {  # $1=label $2=src $3=expected-message-substring $4=alarm-secs
    local label="$1" src="$2" needle="$3" secs="$4"
    local bin="$TMP/$(basename "${src%.wyn}").bin"
    if ! "$WYN" build "$src" -o "$bin" >/dev/null 2>&1; then
        echo "  FAIL [$label]: build failed"; FAILS=$((FAILS+1)); return
    fi
    local out rc
    out=$(run_bounded "$secs" "$bin"); rc=$?
    if [ "$rc" -eq 142 ] || [ "$rc" -eq 137 ]; then
        echo "  FAIL [$label]: HUNG (alarm fired, rc=$rc) — the bug is back"; FAILS=$((FAILS+1)); return
    fi
    if [ "$rc" -eq 0 ]; then
        echo "  FAIL [$label]: exited 0, expected non-zero panic"; FAILS=$((FAILS+1)); return
    fi
    if ! echo "$out" | grep -qF "$needle"; then
        echo "  FAIL [$label]: message missing '$needle' (got: $out)"; FAILS=$((FAILS+1)); return
    fi
    echo "  ok   [$label]: exit $rc, message matched"
}

# --- BUG 1a: negative Shared handle write ---
cat > "$TMP/shared_neg.wyn" <<'EOF'
fn main() {
    var s = Shared.new(0)
    Shared.set(-1, 123)
    println(Shared.get(s))
}
EOF
check "Shared.set(-1)" "$TMP/shared_neg.wyn" "Shared handle out of range" 8

# --- BUG 1b: pool exhaustion (>WYN_SHARED_MAX allocations) ---
# WYN_SHARED_MAX is 65536; allocate past it and confirm a clean panic, not an
# OOB. (Kept modest at exactly-over via a loop; alarm guards a runaway.)
cat > "$TMP/shared_exhaust.wyn" <<'EOF'
fn main() {
    var i = 0
    while i < 70000 {
        var s = Shared.new(0)
        i = i + 1
    }
    println("should not reach here")
}
EOF
check "Shared pool exhaustion" "$TMP/shared_exhaust.wyn" "Shared value pool exhausted" 20

# --- BUG 2: capacity-0 channel ---
cat > "$TMP/chan0.wyn" <<'EOF'
fn produce(ch: int) {
    Task.send(ch, 42)
}
fn main() {
    var ch = Task.channel(0)
    spawn produce(ch)
    var v = Task.recv(ch)
    println(v)
}
EOF
check "Task.channel(0)" "$TMP/chan0.wyn" "capacity must be >= 1" 8

if [ "$FAILS" -eq 0 ]; then
    echo "PASS: all runtime-safety panic checks"
    exit 0
fi
echo "FAIL: $FAILS check(s) failed"
exit 1
