#!/bin/bash
# Wyn v1.7.0 Benchmark Suite
# Run from wyn/ directory: ./benchmarks/run.sh

set -e
cd "$(dirname "$0")/.."
RUNS=5

echo "=== Wyn v1.7.0 Benchmarks ==="
echo "Platform: $(uname -s) $(uname -m)"
echo "Runs per benchmark: $RUNS"
echo ""

time_cmd() {
    local total=0
    local min=999999
    for i in $(seq 1 $RUNS); do
        local t=$( { time "$@" > /dev/null 2>&1; } 2>&1 | grep real | sed 's/real[[:space:]]*//' )
        local ms=$(echo "$t" | awk -F'[ms.]' '{
            if (NF==3) print $1*60000+$2*1000+$3;
            else if (NF==2) print $1*1000+$2;
        }')
        [ -z "$ms" ] && ms=0
        total=$((total + ms))
        [ "$ms" -lt "$min" ] && min=$ms
    done
    local avg=$((total / RUNS))
    echo "${avg}ms (min: ${min}ms)"
}

echo "--- Compile Time ---"
printf "  TCC (wyn run):     "
time_cmd ./wyn run benchmarks/startup.wyn

printf "  Release (--release): "
time_cmd ./wyn run benchmarks/startup.wyn --release

echo ""
echo "--- Fibonacci(35) ---"
# Build once, then time execution
./wyn build benchmarks/fib35.wyn --release 2>/dev/null
printf "  Wyn:  "
time_cmd ./benchmarks/fib35

if command -v go &> /dev/null; then
    go build -o /tmp/fib35_go benchmarks/fib35.go 2>/dev/null
    printf "  Go:   "
    time_cmd /tmp/fib35_go
    rm -f /tmp/fib35_go
fi

echo ""
echo "--- Spawn 10K Tasks ---"
./wyn build benchmarks/spawn_10k.wyn --release 2>/dev/null
printf "  Wyn:  "
time_cmd ./benchmarks/spawn_10k

echo ""
echo "--- Binary Size ---"
./wyn build benchmarks/binary_size.wyn --release 2>/dev/null
WYN_SIZE=$(ls -l benchmarks/binary_size 2>/dev/null | awk '{print $5}')
echo "  Wyn:  $((WYN_SIZE / 1024)) KB"

if command -v go &> /dev/null; then
    go build -o /tmp/startup_go benchmarks/startup.go 2>/dev/null
    GO_SIZE=$(ls -l /tmp/startup_go | awk '{print $5}')
    echo "  Go:   $((GO_SIZE / 1024)) KB"
    rm -f /tmp/startup_go
fi

echo ""
echo "--- Startup Time ---"
./wyn build benchmarks/startup.wyn --release 2>/dev/null
printf "  Wyn:  "
time_cmd ./benchmarks/startup

if command -v go &> /dev/null; then
    go build -o /tmp/startup_go benchmarks/startup.go 2>/dev/null
    printf "  Go:   "
    time_cmd /tmp/startup_go
    rm -f /tmp/startup_go
fi

# Cleanup
rm -f benchmarks/fib35 benchmarks/spawn_10k benchmarks/binary_size benchmarks/startup benchmarks/strings
rm -f benchmarks/*.c

echo ""
echo "=== Done ==="
