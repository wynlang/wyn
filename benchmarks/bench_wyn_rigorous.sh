#!/bin/bash

echo "=== WYN BENCHMARK (Rigorous Testing) ==="
echo

cd /Users/aoaws/src/ao/wyn-lang/wyn

# Compile
./wyn benchmarks/bench_wyn_rigorous.wyn >/dev/null 2>&1

# Run and parse output
output=$(timeout 60 ./benchmarks/bench_wyn_rigorous.out)

# Parse the output (6 numbers per benchmark)
echo "$output" | awk '
{
    # Split into individual numbers
    n = split($0, nums, "")
    
    # Extract groups of numbers (this is tricky with concatenated output)
    # We need to manually parse based on expected format
    print "Raw output:", $0
}
'

echo
echo "Running manual tests instead..."
echo

# Test 10k spawns - 10 passes
echo "10,000 spawns (10 passes):"
for i in {1..10}; do
    ./wyn benchmarks/bench_spawn_overhead.wyn >/dev/null 2>&1
    result=$(./benchmarks/bench_spawn_overhead.out)
    ms=$(echo "scale=2; $result/1000000" | bc)
    echo "  Pass $i: ${result}ns (${ms}ms)"
done

echo
echo "100,000 spawns (5 passes):"
for i in {1..5}; do
    ./wyn benchmarks/bench_100k.wyn >/dev/null 2>&1
    result=$(timeout 10 ./benchmarks/bench_100k.out)
    ms=$(echo "scale=2; $result/1000000" | bc)
    echo "  Pass $i: ${result}ns (${ms}ms)"
done

echo
echo "1,000,000 spawns (3 passes):"
for i in {1..3}; do
    ./wyn benchmarks/bench_1m.wyn >/dev/null 2>&1
    result=$(timeout 30 ./benchmarks/bench_1m.out)
    ms=$(echo "scale=2; $result/1000000" | bc)
    echo "  Pass $i: ${result}ns (${ms}ms)"
done
