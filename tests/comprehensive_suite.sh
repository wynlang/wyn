#!/bin/bash
# Comprehensive test suite for Wyn compiler

PASS=0
FAIL=0

run_test() {
    local name="$1"
    local code="$2"
    local expected="$3"
    
    echo "$code" > /tmp/test_wyn.wyn
    if ./wyn-llvm /tmp/test_wyn.wyn 2>&1 > /dev/null && [ -f /tmp/test_wyn.out ]; then
        /tmp/test_wyn.out > /dev/null 2>&1
        local result=$?
        if [ "$result" -eq "$expected" ]; then
            echo "  ✓ $name"
            ((PASS++))
            return 0
        else
            echo "  ✗ $name (expected $expected, got $result)"
            ((FAIL++))
            return 1
        fi
    else
        echo "  ✗ $name (compilation failed)"
        ((FAIL++))
        return 1
    fi
}

echo "=== Core Language Features ==="

# Literals
run_test "Int literal" "fn main() -> int { return 42; }" 42
run_test "Float literal" "fn main() -> int { var x = 3.14; return 0; }" 0
run_test "Negative float" "fn main() -> int { var x = -3.14; if x < 0.0 { return 0; } return 1; }" 0
run_test "String literal" "fn main() -> int { var s = \"hello\"; return 0; }" 0
run_test "Bool literal" "fn main() -> int { var b = true; return 0; }" 0

# Arithmetic
run_test "Addition" "fn main() -> int { return 2 + 3; }" 5
run_test "Subtraction" "fn main() -> int { return 10 - 3; }" 7
run_test "Multiplication" "fn main() -> int { return 4 * 5; }" 20
run_test "Division" "fn main() -> int { return 20 / 4; }" 5

# Control flow
run_test "If-else true" "fn main() -> int { if true { return 1; } else { return 0; } }" 1
run_test "If-else false" "fn main() -> int { if false { return 0; } else { return 1; } }" 1
run_test "While loop" "fn main() -> int { var i = 0; while i < 3 { i = i + 1; } return i; }" 3
run_test "For loop" "fn main() -> int { var sum = 0; for i in 0..3 { sum = sum + i; } return sum; }" 3

# Match expressions
run_test "Match literal" "fn main() -> int { var x = 5; match x { 5 => { return 10; } _ => { return 0; } } }" 10
run_test "Match wildcard" "fn main() -> int { var x = 99; match x { 5 => { return 5; } _ => { return 42; } } }" 42
run_test "Match or-pattern" "fn main() -> int { var x = 2; var r = match x { 1 | 2 | 3 => 100, _ => 0 }; return r; }" 100

# Float methods
run_test "Float abs" "fn main() -> int { var x = -3.14; var a = x.abs(); if a > 3.0 { return 0; } return 1; }" 0
run_test "Float floor" "fn main() -> int { var x = 3.7; var f = x.floor(); if f == 3.0 { return 0; } return 1; }" 0
run_test "Float ceil" "fn main() -> int { var x = 3.2; var c = x.ceil(); if c == 4.0 { return 0; } return 1; }" 0
run_test "Float round" "fn main() -> int { var x = 3.6; var r = x.round(); if r == 4.0 { return 0; } return 1; }" 0

# Functions
run_test "Function call" "fn add(a: int, b: int) -> int { return a + b; } fn main() -> int { return add(2, 3); }" 5
run_test "Recursion" "fn fib(n: int) -> int { if n <= 1 { return n; } return fib(n-1) + fib(n-2); } fn main() -> int { return fib(5); }" 5

# Arrays
run_test "Array literal" "fn main() -> int { var arr = [1, 2, 3]; return 0; }" 0
run_test "Array access" "fn main() -> int { var arr = [10, 20, 30]; return arr[1]; }" 20

echo ""
echo "=========================================="
echo "  TOTAL: $PASS/$((PASS+FAIL)) tests passing"
echo "  SUCCESS RATE: $(( PASS * 100 / (PASS + FAIL) ))%"
echo "=========================================="

if [ $FAIL -eq 0 ]; then
    echo "✅ All tests passed!"
    exit 0
else
    echo "⚠️  $FAIL tests failed"
    exit 1
fi
