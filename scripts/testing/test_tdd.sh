#!/bin/bash
# Minimal TDD test for LLVM backend

cd "$(dirname "$0")"
WYN="./wyn"
PASS=0
FAIL=0

test_case() {
    local name="$1"
    local code="$2"
    local expected="$3"
    
    echo -n "Testing $name... "
    echo "$code" > /tmp/test_$$.wyn
    
    if ! $WYN /tmp/test_$$.wyn >/dev/null 2>&1; then
        echo "FAIL (compile error)"
        ((FAIL++))
        return
    fi
    
    /tmp/test_$$.out >/dev/null 2>&1
    local result=$?
    
    if [ "$result" -eq "$expected" ]; then
        echo "PASS"
        ((PASS++))
    else
        echo "FAIL (expected $expected, got $result)"
        ((FAIL++))
    fi
    
    rm -f /tmp/test_$$.wyn /tmp/test_$$.out
}

echo "=== LLVM Backend TDD Tests ==="

# Test 1: Boolean true
test_case "bool_true" \
"fn main() -> int {
    if true {
        return 1
    }
    return 0
}" 1

# Test 2: Boolean false
test_case "bool_false" \
"fn main() -> int {
    if false {
        return 1
    }
    return 0
}" 0

# Test 3: Float literal
test_case "float_literal" \
"fn main() -> int {
    var x: float = 3.14
    return 3
}" 3

# Test 4: Unary negation
test_case "unary_neg" \
"fn main() -> int {
    var x = 5
    var y = -x
    return 0
}" 0

# Test 5: Unary not
test_case "unary_not" \
"fn main() -> int {
    if !false {
        return 1
    }
    return 0
}" 1

# Test 6: Assignment
test_case "assignment" \
"fn main() -> int {
    var x = 5
    x = 10
    return x
}" 10

# Test 7: Expression statement
test_case "expr_stmt" \
"fn main() -> int {
    5 + 3
    return 7
}" 7

# Test 8: While loop
test_case "while_loop" \
"fn main() -> int {
    var x = 0
    while x < 5 {
        x = x + 1
    }
    return x
}" 5

echo
echo "Results: $PASS passed, $FAIL failed"

# Additional tests for next features
echo
echo "=== Additional Features ==="

# Test 9: For loop
test_case "for_loop" \
"fn main() -> int {
    var sum = 0
    for i in 0..5 {
        sum = sum + i
    }
    return sum
}" 10

# Test 10: Break statement
test_case "break_stmt" \
"fn main() -> int {
    var x = 0
    while x < 10 {
        x = x + 1
        if x == 5 {
            break
        }
    }
    return x
}" 5

# Test 11: Continue statement
test_case "continue_stmt" \
"fn main() -> int {
    var sum = 0
    for i in 0..5 {
        if i == 2 {
            continue
        }
        sum = sum + i
    }
    return sum
}" 8

echo
echo "Results: $PASS passed, $FAIL failed"
exit $FAIL
