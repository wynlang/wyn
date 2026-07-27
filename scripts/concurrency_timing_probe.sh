#!/bin/bash
# S0 await-epic timing probe (NON-deterministic; run manually to regenerate the
# numbers in docs/CONCURRENCY_BASELINE.md). Measures whether awaited Time::sleep
# overlaps cooperatively (wall ~= one sleep) or serializes.
#
# Distinguishes the two executors by N > CPU count:
#   - coroutine scheduler (default): all N sleeps park cooperatively -> wall ~= one sleep
#   - legacy thread pool (WYN_ASYNC_POOL=1): only ~NCPU run at once -> wall ~= ceil(N/NCPU)*sleep
#
# Usage: scripts/concurrency_timing_probe.sh
set -uo pipefail
cd "$(dirname "$0")/.."
WYN="${WYN:-./wyn}"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

echo "host CPUs: $( (sysctl -n hw.ncpu 2>/dev/null) || (nproc 2>/dev/null) || echo '?')"
echo

# --- awaited spawn + Time::sleep (does it block a pool thread or yield?) ---
cat > "$TMP/awaited_sleep.wyn" <<'EOF'
fn nap(ms: int) -> int { Time::sleep(ms); return ms }
fn main() {
    var t0 = Time::now_millis()
    var futs = []
    var n = 64
    for i in 0..n { futs.push(spawn nap(100)) }
    var rs = await_all(futs)
    var t1 = Time::now_millis()
    var sum = 0
    for _, r in rs { sum = sum + r }
    println("N=64 x 100ms sleep  sum=" + sum.to_string() + "  wall_ms=" + (t1 - t0).to_string())
}
EOF

# --- fire-and-forget spawn + Time::sleep (contrast: known cooperative) ---
cat > "$TMP/faf_sleep.wyn" <<'EOF'
fn worker(c: int) -> int { Time::sleep(100); Shared.add(c, 1); return 0 }
fn main() {
    var done = Shared.new(0)
    var t0 = Time::now_millis()
    var i = 0
    while i < 64 { spawn worker(done); i = i + 1 }
    while Shared.get(done) < 64 { Time::sleep(2) }
    var t1 = Time::now_millis()
    println("N=64 x 100ms sleep  wall_ms=" + (t1 - t0).to_string())
}
EOF

echo "=== awaited spawn + await_all(64 x Time::sleep(100)) ==="
echo -n "  default (coroutine scheduler): "; $WYN run "$TMP/awaited_sleep.wyn" 2>/dev/null | grep -v -i compiled
echo -n "  WYN_ASYNC_POOL=1 (legacy pool): "; WYN_ASYNC_POOL=1 $WYN run "$TMP/awaited_sleep.wyn" 2>/dev/null | grep -v -i compiled
echo
echo "=== fire-and-forget spawn + Time::sleep(100) x64 (contrast) ==="
echo -n "  default (coroutine scheduler): "; $WYN run "$TMP/faf_sleep.wyn" 2>/dev/null | grep -v -i compiled
