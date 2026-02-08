#!/bin/bash
# Comprehensive self-compilation test suite
# Tests that compiler_modular.wyn can compile various Wyn programs

set -e

echo "=== Self-Compilation Test Suite ==="
echo ""

cd "$(dirname "$0")/.."

COMPILER="./lib/compiler_modular.wyn.out"
PASS=0
FAIL=0

# Helper function to run a test
run_test() {
    local test_name="$1"
    local test_code="$2"
    local expected_vars="$3"
    
    echo "Test: $test_name"
    
    # Write test code
    echo "$test_code" > /tmp/test_input.wyn
    
    # Compile with our compiler
    if ! $COMPILER > /tmp/compile_output.txt 2>&1; then
        echo "  ❌ FAIL: Compilation failed"
        cat /tmp/compile_output.txt
        FAIL=$((FAIL + 1))
        return 1
    fi
    
    # Check if C file was generated
    if [ ! -f /tmp/test_input.c ]; then
        echo "  ❌ FAIL: No C file generated"
        FAIL=$((FAIL + 1))
        return 1
    fi
    
    # Compile the generated C code
    if ! gcc /tmp/test_input.c -o /tmp/test_input 2>/tmp/gcc_errors.txt; then
        echo "  ❌ FAIL: Generated C code doesn't compile"
        cat /tmp/gcc_errors.txt
        FAIL=$((FAIL + 1))
        return 1
    fi
    
    # Run the generated binary
    if ! /tmp/test_input; then
        echo "  ❌ FAIL: Generated binary failed to run"
        FAIL=$((FAIL + 1))
        return 1
    fi
    
    # Verify expected variables in generated C code
    if [ -n "$expected_vars" ]; then
        for var in $expected_vars; do
            if ! grep -q "int $var = " /tmp/test_input.c; then
                echo "  ❌ FAIL: Expected variable '$var' not found in generated C code"
                FAIL=$((FAIL + 1))
                return 1
            fi
        done
    fi
    
    echo "  ✅ PASS"
    PASS=$((PASS + 1))
    return 0
}

# Test 1: Single variable
run_test "Single variable" "var x = 42" "x"

# Test 2: Multiple variables
run_test "Multiple variables" "var a = 1
var b = 2
var c = 3" "a b c"

# Test 3: Large numbers
run_test "Large numbers" "var big = 999999" "big"

# Test 4: Zero value
run_test "Zero value" "var zero = 0" "zero"

# Test 5: Multiple variables with different values
run_test "Different values" "var x = 10
var y = 20
var z = 30
var w = 40" "x y z w"

# Test 6: Variables with underscores
run_test "Underscore names" "var my_var = 100
var another_one = 200" "my_var another_one"

# Test 7: Empty program (should still generate valid C)
echo "Test: Empty program"
echo "" > /tmp/test_input.wyn
if $COMPILER > /tmp/compile_output.txt 2>&1; then
    if gcc /tmp/test_input.c -o /tmp/test_input 2>/dev/null; then
        if /tmp/test_input; then
            echo "  ✅ PASS"
            PASS=$((PASS + 1))
        else
            echo "  ❌ FAIL: Generated binary failed"
            FAIL=$((FAIL + 1))
        fi
    else
        echo "  ❌ FAIL: Generated C doesn't compile"
        FAIL=$((FAIL + 1))
    fi
else
    echo "  ❌ FAIL: Compilation failed"
    FAIL=$((FAIL + 1))
fi

# Test 8: Whitespace handling
run_test "Whitespace handling" "var   x   =   42
var y=99" "x y"

# Summary
echo ""
echo "=== Test Summary ==="
echo "Passed: $PASS"
echo "Failed: $FAIL"
echo ""

if [ $FAIL -eq 0 ]; then
    echo "✅ All tests passed!"
    exit 0
else
    echo "❌ Some tests failed"
    exit 1
fi
