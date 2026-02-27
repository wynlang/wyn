#!/bin/bash
# Wyn v1.8 Benchmark Suite — honest methodology
# Pre-built binaries only. 15 runs, warm median. Go comparison for every benchmark.
# Run from wyn/ directory: ./benchmarks/run.sh

set -e
cd "$(dirname "$0")/.."

echo "=== Wyn v1.8 Benchmarks ==="
echo "Platform: $(uname -s) $(uname -m)"
echo "Wyn: $(cat VERSION)"
echo "Go: $(go version 2>/dev/null | awk '{print $3}' || echo 'not installed')"
echo "Method: pre-built binaries, 15 runs, warm median (drop first run)"
echo ""

# Build all benchmarks
echo "Building..."
./wyn build benchmarks/fib35.wyn --release 2>/dev/null
./wyn build benchmarks/strings.wyn --release 2>/dev/null
./wyn build benchmarks/spawn_1m.wyn --release 2>/dev/null
go build -o benchmarks/fib35_go benchmarks/fib35.go 2>/dev/null || true
go build -o benchmarks/strings_go benchmarks/strings.go 2>/dev/null || true
go build -o benchmarks/spawn_1m_go benchmarks/spawn_1m.go 2>/dev/null || true
echo ""

python3 -c "
import subprocess, time, statistics, os

def bench(cmd, runs=15):
    times = []
    for i in range(runs):
        start = time.perf_counter_ns()
        subprocess.run(cmd, capture_output=True)
        end = time.perf_counter_ns()
        times.append((end - start) / 1_000_000)
    times.sort()
    warm = times[1:]
    return statistics.median(warm), warm[0], warm[-1]

def fmt(med, mn, mx):
    return f'{med:.0f}ms (min={mn:.0f}, max={mx:.0f})'

print('--- Fibonacci(35) ---')
w = bench(['./benchmarks/fib35'])
print(f'  Wyn:  {fmt(*w)}')
if os.path.exists('benchmarks/fib35_go'):
    g = bench(['./benchmarks/fib35_go'])
    print(f'  Go:   {fmt(*g)}')
    delta = (g[0] - w[0]) / g[0] * 100
    print(f'  Delta: {abs(delta):.0f}% {\"faster\" if delta > 0 else \"slower\"}')

print()
print('--- Strings 10K ---')
w = bench(['./benchmarks/strings'])
print(f'  Wyn:  {fmt(*w)}')
if os.path.exists('benchmarks/strings_go'):
    g = bench(['./benchmarks/strings_go'])
    print(f'  Go:   {fmt(*g)}')
    delta = (g[0] - w[0]) / g[0] * 100
    print(f'  Delta: {abs(delta):.0f}% {\"faster\" if delta > 0 else \"slower\"}')

print()
print('--- Spawn 1M ---')
w = bench(['./benchmarks/spawn_1m'], runs=5)
print(f'  Wyn:  {fmt(*w)}')
if os.path.exists('benchmarks/spawn_1m_go'):
    g = bench(['./benchmarks/spawn_1m_go'], runs=5)
    print(f'  Go:   {fmt(*g)}')
    delta = (g[0] - w[0]) / g[0] * 100
    print(f'  Delta: {abs(delta):.0f}% {\"faster\" if delta > 0 else \"slower\"}')

print()
print('--- Binary Size ---')
wyn_size = os.path.getsize('benchmarks/fib35')
print(f'  Wyn:  {wyn_size // 1024} KB')
if os.path.exists('benchmarks/fib35_go'):
    go_size = os.path.getsize('benchmarks/fib35_go')
    print(f'  Go:   {go_size // 1024} KB')
    print(f'  Ratio: {go_size / wyn_size:.0f}x smaller')
"

# Cleanup
rm -f benchmarks/fib35 benchmarks/strings benchmarks/spawn_1m
rm -f benchmarks/fib35_go benchmarks/strings_go benchmarks/spawn_1m_go
rm -f benchmarks/*.c

echo ""
echo "=== Done ==="
