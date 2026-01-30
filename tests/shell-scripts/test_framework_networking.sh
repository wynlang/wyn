#!/bin/bash
# Comprehensive Testing Framework and Advanced Networking Validation

set -e

WYN=${WYN_ROOT:-../..}/wyn
TESTDIR=$(mktemp -d)
cd "$TESTDIR"

echo "=== Testing Framework & Networking Validation ==="
echo "Test directory: $TESTDIR"
echo

PASSED=0
FAILED=0

run_test() {
    local name="$1"
    local file="$2"
    echo -n "Test $((PASSED + FAILED + 1)): $name... "
    
    if timeout 10 $WYN run "$file" > /tmp/test_output.txt 2>&1; then
        if grep -q "PASS\|✓ All tests passed" /tmp/test_output.txt 2>/dev/null; then
            echo "✓ PASS"
            PASSED=$((PASSED + 1))
            return 0
        fi
    fi
    
    echo "✗ FAIL"
    cat /tmp/test_output.txt | tail -10
    FAILED=$((FAILED + 1))
    return 1
}

# Test 1: Basic assertions
cat > test1.wyn << 'EOF'
fn main() -> int {
    Test::init("Basic Tests")
    Test::assert_eq_int(2 + 2, 4, "addition")
    Test::assert_eq_str("hello", "hello", "strings")
    Test::assert_gt(10, 5, "greater than")
    return Test::summary()
}
EOF
run_test "Basic assertions" test1.wyn

# Test 2: Comparison assertions
cat > test2.wyn << 'EOF'
fn main() -> int {
    Test::init("Comparison Tests")
    Test::assert_lt(3, 7, "less than")
    Test::assert_gte(5, 5, "greater or equal")
    Test::assert_lte(4, 9, "less or equal")
    Test::assert_ne_int(1, 2, "not equal")
    return Test::summary()
}
EOF
run_test "Comparison assertions" test2.wyn

# Test 3: String assertions
cat > test3.wyn << 'EOF'
fn main() -> int {
    Test::init("String Tests")
    Test::assert_contains("hello world", "world", "contains")
    Test::assert_eq_str("test", "test", "equality")
    return Test::summary()
}
EOF
run_test "String assertions" test3.wyn

# Test 4: Test grouping
cat > test4.wyn << 'EOF'
fn main() -> int {
    Test::init("Grouped Tests")
    
    Test::describe("Group 1")
    Test::assert_eq_int(1, 1, "test 1")
    
    Test::describe("Group 2")
    Test::assert_eq_int(2, 2, "test 2")
    
    return Test::summary()
}
EOF
run_test "Test grouping" test4.wyn

# Test 5: HTTP GET request
cat > test5.wyn << 'EOF'
fn main() -> int {
    var resp = Http::get("http://httpbin.org/get")
    if resp > 0 {
        var status = Http::status(resp)
        if status == 200 {
            print("PASS")
        }
        Http::free(resp)
    }
    return 0
}
EOF
run_test "HTTP GET request" test5.wyn

# Test 6: HTTP POST request
cat > test6.wyn << 'EOF'
fn main() -> int {
    var resp = Http::post("http://httpbin.org/post", "{\"test\":1}", "application/json")
    if resp > 0 {
        var status = Http::status(resp)
        if status == 200 {
            print("PASS")
        }
        Http::free(resp)
    }
    return 0
}
EOF
run_test "HTTP POST request" test6.wyn

# Test 7: URL encoding (skip - needs string type support)
# cat > test7.wyn << 'EOF'
# fn main() -> int {
#     var encoded = Url::encode("hello world")
#     var decoded = Url::decode(encoded)
#     if decoded == "hello world" {
#         print("PASS")
#     }
#     return 0
# }
# EOF
# run_test "URL encoding/decoding" test7.wyn

# Test 8: Combined test + HTTP
cat > test8.wyn << 'EOF'
fn main() -> int {
    Test::init("HTTP Integration")
    
    var resp = Http::get("http://httpbin.org/get")
    if resp > 0 {
        var status = Http::status(resp)
        Test::assert_eq_int(status, 200, "status is 200")
        Http::free(resp)
    }
    
    return Test::summary()
}
EOF
run_test "Test framework + HTTP" test8.wyn

# Test 9: Math with tests
cat > test9.wyn << 'EOF'
fn add(a: int, b: int) -> int {
    return a + b
}

fn multiply(a: int, b: int) -> int {
    return a * b
}

fn main() -> int {
    Test::init("Math Functions")
    
    Test::describe("Addition")
    Test::assert_eq_int(add(2, 3), 5, "2 + 3 = 5")
    Test::assert_eq_int(add(10, 20), 30, "10 + 20 = 30")
    
    Test::describe("Multiplication")
    Test::assert_eq_int(multiply(3, 4), 12, "3 * 4 = 12")
    Test::assert_eq_int(multiply(5, 5), 25, "5 * 5 = 25")
    
    return Test::summary()
}
EOF
run_test "Custom functions with tests" test9.wyn

# Test 10: Complex test suite
cat > test10.wyn << 'EOF'
fn main() -> int {
    Test::init("Comprehensive Suite")
    
    Test::describe("Integers")
    Test::assert_eq_int(0, 0, "zero")
    Test::assert_eq_int(-5, -5, "negative")
    Test::assert_eq_int(1000, 1000, "large")
    
    Test::describe("Comparisons")
    Test::assert_gt(100, 99, "100 > 99")
    Test::assert_lt(1, 2, "1 < 2")
    Test::assert_gte(50, 50, "50 >= 50")
    Test::assert_lte(25, 50, "25 <= 50")
    
    Test::describe("Strings")
    Test::assert_eq_str("a", "a", "single char")
    Test::assert_eq_str("test string", "test string", "multi word")
    Test::assert_contains("abcdef", "cde", "substring")
    
    return Test::summary()
}
EOF
run_test "Comprehensive test suite" test10.wyn

echo
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo "Total:  $((PASSED + FAILED))"
echo

if [ $FAILED -eq 0 ]; then
    echo "✓ All tests passed!"
    exit 0
else
    echo "✗ $FAILED test(s) failed"
    exit 1
fi
