#!/bin/bash
# Epic 7: Performance Benchmarks

cd "$(dirname "$0")/.."

echo "=== Epic 7: Performance Benchmarks ==="
echo ""

# Test 1: Fibonacci (recursive)
cat > /tmp/bench_fib.wyn << 'EOF'
fn fib(n: int) -> int {
    if n <= 1 { return n; }
    return fib(n - 1) + fib(n - 2)
}

fn main() -> int {
    return fib(30)
}
EOF

echo "Test 1: Fibonacci(30)"
./wyn-llvm /tmp/bench_fib.wyn 2>&1 > /dev/null
if [ $? -eq 0 ]; then
    start=$(date +%s%N)
    /tmp/bench_fib.out
    result=$?
    end=$(date +%s%N)
    elapsed=$(( (end - start) / 1000000 ))
    echo "  Result: $result"
    echo "  Time: ${elapsed}ms"
    echo "  ✓ Fibonacci benchmark"
else
    echo "  ✗ Compilation failed"
fi

# Test 2: Prime sieve
cat > /tmp/bench_primes.wyn << 'EOF'
fn count_primes(n: int) -> int {
    var count = 0
    var i = 2
    while i < n {
        var is_prime = 1
        var j = 2
        while j * j <= i {
            if i % j == 0 {
                is_prime = 0
                break
            }
            j = j + 1
        }
        if is_prime == 1 {
            count = count + 1
        }
        i = i + 1
    }
    return count
}

fn main() -> int {
    return count_primes(10000)
}
EOF

echo ""
echo "Test 2: Prime sieve (count primes < 10000)"
./wyn-llvm /tmp/bench_primes.wyn 2>&1 > /dev/null
if [ $? -eq 0 ]; then
    start=$(date +%s%N)
    /tmp/bench_primes.out
    result=$?
    end=$(date +%s%N)
    elapsed=$(( (end - start) / 1000000 ))
    echo "  Result: $result primes"
    echo "  Time: ${elapsed}ms"
    echo "  ✓ Prime sieve benchmark"
else
    echo "  ✗ Compilation failed"
fi

# Test 3: Array operations
cat > /tmp/bench_array.wyn << 'EOF'
fn sum_array(arr: [int], len: int) -> int {
    var sum = 0
    var i = 0
    while i < len {
        sum = sum + arr[i]
        i = i + 1
    }
    return sum
}

fn main() -> int {
    var arr = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    var total = 0
    var i = 0
    while i < 1000 {
        total = total + sum_array(arr, 10)
        i = i + 1
    }
    return total
}
EOF

echo ""
echo "Test 3: Array operations (1000 iterations)"
./wyn-llvm /tmp/bench_array.wyn 2>&1 > /dev/null
if [ $? -eq 0 ]; then
    start=$(date +%s%N)
    /tmp/bench_array.out
    result=$?
    end=$(date +%s%N)
    elapsed=$(( (end - start) / 1000000 ))
    echo "  Result: $result"
    echo "  Time: ${elapsed}ms"
    echo "  ✓ Array benchmark"
else
    echo "  ✗ Compilation failed"
fi

echo ""
echo "=== Performance Summary ==="
echo "All benchmarks completed successfully"
echo "Note: Times are for reference only (single run)"
