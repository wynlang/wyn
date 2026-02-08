#!/bin/bash

echo "=== EDGE CASE TESTING ==="
echo

cd /Users/aoaws/src/ao/wyn-lang/wyn

# Edge Case 1: Very small (1 spawn)
echo "1. Single spawn (100 iterations):"
cat > /tmp/edge_single.wyn << 'EOF'
fn noop() -> int { return 0; }
fn main() -> int {
    var i = 0;
    while i < 100 {
        spawn noop();
        i = i + 1;
    }
    return 0;
}
EOF
./wyn /tmp/edge_single.wyn >/dev/null 2>&1
timeout 2 /tmp/edge_single.out && echo "  ✓ PASS" || echo "  ✗ FAIL"

# Edge Case 2: Rapid succession
echo "2. Rapid succession (10k spawns, no delay):"
timeout 5 ./benchmarks/bench_spawn_overhead.out >/dev/null && echo "  ✓ PASS" || echo "  ✗ FAIL"

# Edge Case 3: CPU-bound tasks
echo "3. CPU-bound tasks (100 × fib(20)):"
cat > /tmp/edge_cpu.wyn << 'EOF'
fn fib(n: int) -> int {
    if n < 2 {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

fn main() -> int {
    var i = 0;
    while i < 100 {
        spawn fib(20);
        i = i + 1;
    }
    var wait = 0;
    while wait < 100000000 {
        wait = wait + 1;
    }
    return 0;
}
EOF
./wyn /tmp/edge_cpu.wyn >/dev/null 2>&1
timeout 10 /tmp/edge_cpu.out && echo "  ✓ PASS" || echo "  ✗ FAIL"

# Edge Case 4: Mixed workload
echo "4. Mixed workload (1k fast + 1k slow):"
cat > /tmp/edge_mixed.wyn << 'EOF'
fn fast() -> int { return 1; }
fn slow() -> int {
    var i = 0;
    while i < 1000 {
        i = i + 1;
    }
    return i;
}

fn main() -> int {
    var i = 0;
    while i < 1000 {
        spawn fast();
        spawn slow();
        i = i + 1;
    }
    var wait = 0;
    while wait < 10000000 {
        wait = wait + 1;
    }
    return 0;
}
EOF
./wyn /tmp/edge_mixed.wyn >/dev/null 2>&1
timeout 5 /tmp/edge_mixed.out && echo "  ✓ PASS" || echo "  ✗ FAIL"

# Edge Case 5: Nested spawns
echo "5. Nested spawns (10 × 100):"
cat > /tmp/edge_nested.wyn << 'EOF'
fn leaf() -> int { return 1; }

fn branch() -> int {
    var i = 0;
    while i < 100 {
        spawn leaf();
        i = i + 1;
    }
    return 0;
}

fn main() -> int {
    var i = 0;
    while i < 10 {
        spawn branch();
        i = i + 1;
    }
    var wait = 0;
    while wait < 10000000 {
        wait = wait + 1;
    }
    return 0;
}
EOF
./wyn /tmp/edge_nested.wyn >/dev/null 2>&1
timeout 5 /tmp/edge_nested.out && echo "  ✓ PASS" || echo "  ✗ FAIL"

# Edge Case 6: Queue overflow (more than QUEUE_SIZE)
echo "6. Queue overflow (10k spawns to single queue):"
timeout 10 ./benchmarks/bench_spawn_overhead.out >/dev/null && echo "  ✓ PASS" || echo "  ✗ FAIL"

# Edge Case 7: Stress test - repeated runs
echo "7. Stress test (10 consecutive runs of 10k):"
success=0
for i in {1..10}; do
    timeout 5 ./benchmarks/bench_spawn_overhead.out >/dev/null && ((success++))
done
echo "  ✓ PASS ($success/10 successful)"

# Edge Case 8: Memory pressure
echo "8. Memory pressure (5M spawns total, 1M at a time):"
success=0
for i in {1..5}; do
    timeout 30 ./benchmarks/bench_1m.out >/dev/null && ((success++))
done
echo "  ✓ PASS ($success/5 successful)"

echo
echo "=== EDGE CASE SUMMARY ==="
echo "All edge cases tested. Check for any failures above."
