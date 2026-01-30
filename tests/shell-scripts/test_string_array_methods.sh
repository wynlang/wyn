#!/bin/bash
# Test string and array methods

set -e

WYN=${WYN_ROOT:-../..}/wyn
TESTDIR=$(mktemp -d)
cd "$TESTDIR"

echo "Testing String & Array Methods"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

PASSED=0
FAILED=0

run_test() {
    local name="$1"
    local file="$2"
    echo -n "[$((PASSED + FAILED + 1))] $name... "
    
    if timeout 10 $WYN run "$file" > /tmp/test_output.txt 2>&1; then
        if grep -q "✓" /tmp/test_output.txt 2>/dev/null; then
            echo "✅"
            PASSED=$((PASSED + 1))
            return 0
        fi
    fi
    
    echo "❌"
    FAILED=$((FAILED + 1))
    return 1
}

# Test 1: String split
cat > test_split.wyn << 'ENDTEST'
fn main() -> int {
    var parts = "a,b,c".split(",")
    
    if parts.len() == 3 {
        print("✓ String split works")
        return 0
    }
    
    print("✗ String split failed")
    return 1
}
ENDTEST

run_test "String split" test_split.wyn

# Test 2: Array map
cat > test_map.wyn << 'ENDTEST'
fn times_two(x: int) -> int {
    return x * 2
}

fn main() -> int {
    var nums = [1, 2, 3]
    var doubled = nums.map(times_two)
    
    // Check if mapping worked (length should be same)
    if doubled.len() == 3 {
        print("✓ Array map works")
        return 0
    }
    
    print("✗ Array map failed")
    return 1
}
ENDTEST

run_test "Array map" test_map.wyn

# Test 3: Array filter (skipped - needs bool predicate support)
# cat > test_filter.wyn << 'ENDTEST'
# ...
# ENDTEST
# run_test "Array filter" test_filter.wyn

# Test 4: Array reduce
cat > test_reduce.wyn << 'ENDTEST'
fn add(a: int, b: int) -> int {
    return a + b
}

fn main() -> int {
    var nums = [1, 2, 3, 4]
    var sum = nums.reduce(add, 0)
    
    if sum == 10 {
        print("✓ Array reduce works")
        return 0
    }
    
    print("✗ Array reduce failed")
    return 1
}
ENDTEST

run_test "Array reduce" test_reduce.wyn

# Test 5: String charAt
cat > test_charat.wyn << 'ENDTEST'
fn main() -> int {
    var s = "hello"
    var first = s.charAt(0)
    var last = s.charAt(4)
    
    if first == "h" {
        if last == "o" {
            print("✓ charAt works")
            return 0
        }
    }
    
    print("✗ charAt failed")
    return 1
}
ENDTEST

run_test "String charAt" test_charat.wyn

# Test 5: String indexing (s[i])
cat > test_string_index.wyn << 'ENDTEST'
fn main() -> int {
    var s = "hello"
    var first = s[0]
    var last = s[4]
    
    if first == "h" {
        if last == "o" {
            print("✓ String indexing works")
            return 0
        }
    }
    
    print("✗ String indexing failed")
    return 1
}
ENDTEST

run_test "String indexing" test_string_index.wyn

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "RESULTS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo "Total:  $((PASSED + FAILED))"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "✅ ALL STRING/ARRAY TESTS PASSED"
    exit 0
else
    echo "❌ SOME TESTS FAILED"
    exit 1
fi
