#!/bin/bash
# Reproducible benchmark script for Wyn vs Go spawn performance
# Run this to verify the claims yourself

set -e

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                                                              ║"
echo "║     Wyn vs Go Spawn Performance - Reproducible Test         ║"
echo "║                                                              ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo

cd "$(dirname "$0")/.."

# Check prerequisites
if ! command -v go &> /dev/null; then
    echo "❌ Go not found. Please install Go to run this test."
    exit 1
fi

if [ ! -f "./wyn" ]; then
    echo "❌ Wyn compiler not found. Please build it first with 'make wyn-llvm'."
    exit 1
fi

echo "System Info:"
echo "  OS: $(uname -s)"
echo "  CPUs: $(sysconf _SC_NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo "unknown")"
echo "  Go version: $(go version)"
echo

# Build Go benchmarks
echo "Building Go benchmarks..."
go build -o bench_go_test benchmarks/bench_go_rigorous.go
echo "✓ Go benchmarks built"
echo

# Build Wyn benchmarks
echo "Building Wyn benchmarks..."
./wyn benchmarks/bench_spawn_overhead.wyn >/dev/null 2>&1
./wyn benchmarks/bench_100k.wyn >/dev/null 2>&1
./wyn benchmarks/bench_1m.wyn >/dev/null 2>&1
echo "✓ Wyn benchmarks built"
echo

echo "════════════════════════════════════════════════════════════════"
echo
echo "Running Go benchmarks (this may take 30-60 seconds)..."
echo
./bench_go_test > /tmp/go_bench_results.txt
cat /tmp/go_bench_results.txt

echo
echo "════════════════════════════════════════════════════════════════"
echo
echo "Running Wyn benchmarks (this may take 30-60 seconds)..."
echo

# 10k spawns - 10 passes
echo "10,000 spawns (10 passes):"
wyn_10k_sum=0
wyn_10k_min=999999999
wyn_10k_max=0
for i in {1..10}; do
    result=$(./benchmarks/bench_spawn_overhead.out)
    ms=$(echo "scale=2; $result/1000000" | bc)
    echo "  Pass $i: ${ms}ms"
    wyn_10k_sum=$((wyn_10k_sum + result))
    [ $result -lt $wyn_10k_min ] && wyn_10k_min=$result
    [ $result -gt $wyn_10k_max ] && wyn_10k_max=$result
done
wyn_10k_avg=$((wyn_10k_sum / 10))
echo "  Min: $(echo "scale=2; $wyn_10k_min/1000000" | bc)ms"
echo "  Avg: $(echo "scale=2; $wyn_10k_avg/1000000" | bc)ms"
echo "  Max: $(echo "scale=2; $wyn_10k_max/1000000" | bc)ms"
echo "  Per-spawn: $((wyn_10k_avg / 10000))ns"
echo

# 100k spawns - 5 passes
echo "100,000 spawns (5 passes):"
wyn_100k_sum=0
wyn_100k_min=999999999
wyn_100k_max=0
for i in {1..5}; do
    result=$(timeout 10 ./benchmarks/bench_100k.out)
    ms=$(echo "scale=2; $result/1000000" | bc)
    echo "  Pass $i: ${ms}ms"
    wyn_100k_sum=$((wyn_100k_sum + result))
    [ $result -lt $wyn_100k_min ] && wyn_100k_min=$result
    [ $result -gt $wyn_100k_max ] && wyn_100k_max=$result
done
wyn_100k_avg=$((wyn_100k_sum / 5))
echo "  Min: $(echo "scale=2; $wyn_100k_min/1000000" | bc)ms"
echo "  Avg: $(echo "scale=2; $wyn_100k_avg/1000000" | bc)ms"
echo "  Max: $(echo "scale=2; $wyn_100k_max/1000000" | bc)ms"
echo "  Per-spawn: $((wyn_100k_avg / 100000))ns"
echo

# 1M spawns - 3 passes
echo "1,000,000 spawns (3 passes):"
wyn_1m_sum=0
wyn_1m_min=999999999
wyn_1m_max=0
for i in {1..3}; do
    result=$(timeout 30 ./benchmarks/bench_1m.out)
    ms=$(echo "scale=2; $result/1000000" | bc)
    echo "  Pass $i: ${ms}ms"
    wyn_1m_sum=$((wyn_1m_sum + result))
    [ $result -lt $wyn_1m_min ] && wyn_1m_min=$result
    [ $result -gt $wyn_1m_max ] && wyn_1m_max=$result
done
wyn_1m_avg=$((wyn_1m_sum / 3))
echo "  Min: $(echo "scale=2; $wyn_1m_min/1000000" | bc)ms"
echo "  Avg: $(echo "scale=2; $wyn_1m_avg/1000000" | bc)ms"
echo "  Max: $(echo "scale=2; $wyn_1m_max/1000000" | bc)ms"
echo "  Per-spawn: $((wyn_1m_avg / 1000000))ns"
echo

echo "════════════════════════════════════════════════════════════════"
echo
echo "SUMMARY:"
echo
echo "  10k spawns:"
echo "    Wyn: $(echo "scale=2; $wyn_10k_avg/1000000" | bc)ms"
echo "    Go:  (see above)"
echo
echo "  100k spawns:"
echo "    Wyn: $(echo "scale=2; $wyn_100k_avg/1000000" | bc)ms"
echo "    Go:  (see above)"
echo
echo "  1M spawns:"
echo "    Wyn: $(echo "scale=2; $wyn_1m_avg/1000000" | bc)ms"
echo "    Go:  (see above)"
echo
echo "Compare the results above to verify Wyn's performance advantage."
echo
echo "✅ Test complete! Results saved to /tmp/go_bench_results.txt"
