#!/bin/bash
# Wyn v1.10 Benchmark Suite
set -euo pipefail
WYN="${WYN:-./wyn}"
echo "=== Wyn Benchmark Suite ==="
echo ""

# Compile speed
echo "--- Compile Speed ---"
echo 'fn main() -> int { println("hello"); return 0 }' > /tmp/wyn_bench.wyn
START=$(date +%s%N 2>/dev/null || python3 -c 'import time; print(int(time.time()*1e9))')
$WYN build /tmp/wyn_bench.wyn 2>/dev/null
END=$(date +%s%N 2>/dev/null || python3 -c 'import time; print(int(time.time()*1e9))')
MS=$(( (END - START) / 1000000 ))
echo "  wyn build (hello):     ${MS}ms"

START=$(date +%s%N 2>/dev/null || python3 -c 'import time; print(int(time.time()*1e9))')
$WYN build --release /tmp/wyn_bench.wyn 2>/dev/null
END=$(date +%s%N 2>/dev/null || python3 -c 'import time; print(int(time.time()*1e9))')
MS=$(( (END - START) / 1000000 ))
echo "  wyn build --release:   ${MS}ms"

# Binary sizes
echo ""
echo "--- Binary Size ---"
$WYN build /tmp/wyn_bench.wyn 2>/dev/null
SIZE=$(stat -f%z /tmp/wyn_bench 2>/dev/null || stat -c%s /tmp/wyn_bench)
echo "  dev build:             $(( SIZE / 1024 ))KB"
$WYN build --release /tmp/wyn_bench.wyn 2>/dev/null
SIZE=$(stat -f%z /tmp/wyn_bench 2>/dev/null || stat -c%s /tmp/wyn_bench)
echo "  release build:         $(( SIZE / 1024 ))KB"

# Spawn performance
echo ""
echo "--- Spawn/Await ---"
cat > /tmp/wyn_bench_spawn.wyn << 'EOF'
fn fib(n: int) -> int {
    if n <= 1 { return n }
    return fib(n - 1) + fib(n - 2)
}
fn main() -> int {
    var start = DateTime_millis()
    var f1 = spawn fib(30)
    var f2 = spawn fib(30)
    var f3 = spawn fib(30)
    var f4 = spawn fib(30)
    var r = await f1 + await f2 + await f3 + await f4
    var elapsed = DateTime_millis() - start
    println(elapsed.to_string() + "ms (4x fib(30) = " + r.to_string() + ")")
    return 0
}
EOF
echo -n "  4x spawn fib(30):      "
$WYN run /tmp/wyn_bench_spawn.wyn 2>&1 | grep -v "Compiled\|Warning"

# Sequential comparison
cat > /tmp/wyn_bench_seq.wyn << 'EOF'
fn fib(n: int) -> int {
    if n <= 1 { return n }
    return fib(n - 1) + fib(n - 2)
}
fn main() -> int {
    var start = DateTime_millis()
    var r = fib(30) + fib(30) + fib(30) + fib(30)
    var elapsed = DateTime_millis() - start
    println(elapsed.to_string() + "ms (sequential)")
    return 0
}
EOF
echo -n "  4x sequential fib(30): "
$WYN run /tmp/wyn_bench_seq.wyn 2>&1 | grep -v "Compiled\|Warning"

rm -f /tmp/wyn_bench* 2>/dev/null
echo ""
echo "Done."
