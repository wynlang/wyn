#!/bin/bash
# TDD Test Suite for v1.1.0 New Features

cd "$(dirname "$0")"

PASS=0
FAIL=0

test_feature() {
    local name="$1"
    local code="$2"
    local expected_exit="$3"
    
    echo -n "Testing $name... "
    echo "$code" > /tmp/test_f.wyn
    
    ./wyn /tmp/test_f.wyn >/dev/null 2>&1
    if [ ! -f /tmp/test_f.wyn.out ]; then
        echo "✗ FAIL (compile error)"
        ((FAIL++))
        return
    fi
    
    /tmp/test_f.wyn.out 2>/dev/null
    actual=$?
    
    if [ "$actual" -eq "$expected_exit" ]; then
        echo "✓ PASS"
        ((PASS++))
    else
        echo "✗ FAIL (exit $actual, expected $expected_exit)"
        ((FAIL++))
    fi
}

echo "=== v1.1.0 Feature Tests ==="
echo ""

# Test 1: Hex literals
test_feature "Hex literals" "fn main() -> int { return 0xFF }" 255

# Test 2: Binary literals  
test_feature "Binary literals" "fn main() -> int { return 0b1010 }" 10

# Test 3: Underscore in numbers
test_feature "Underscore in numbers" "fn main() -> int { return 1_000 }" 232

# Test 4: String escape \n
test_feature "String escape \\n" 'fn main() -> int { var s = "a\nb"; return s.len() }' 3

# Test 5: String escape \t
test_feature "String escape \\t" 'fn main() -> int { var s = "a\tb"; return s.len() }' 3

# Test 6: --version flag
echo -n "Testing --version flag... "
if ./wyn --version 2>&1 | grep -q "1.1"; then
    echo "✓ PASS"
    ((PASS++))
else
    echo "✗ FAIL"
    ((FAIL++))
fi

# Test 7: --help flag
echo -n "Testing --help flag... "
if ./wyn --help 2>&1 | grep -q "Compiler"; then
    echo "✓ PASS"
    ((PASS++))
else
    echo "✗ FAIL"
    ((FAIL++))
fi

# Test 8: -o output flag
echo -n "Testing -o flag... "
echo "fn main() -> int { return 42 }" > /tmp/test_output.wyn
if ./wyn /tmp/test_output.wyn -o /tmp/custom_out >/dev/null 2>&1; then
    if [ -f /tmp/custom_out ]; then
        /tmp/custom_out 2>/dev/null
        exit_code=$?
        if [ "$exit_code" -eq 42 ]; then
            echo "✓ PASS"
            ((PASS++))
        else
            echo "✗ FAIL (wrong exit code: $exit_code)"
            ((FAIL++))
        fi
        rm -f /tmp/custom_out
    else
        echo "✗ FAIL (no output file)"
        ((FAIL++))
    fi
else
    echo "✗ FAIL (compile error)"
    ((FAIL++))
fi

# Test 9: Array slicing
test_feature "Array slicing" \
"fn main() -> int { var arr = [10, 20, 30, 40]; var s = arr.slice(1, 3); return s.len() }" \
2

# Test 10: String slicing  
test_feature "String slicing" \
'fn main() -> int { var s = "hello"; var sub = s.slice(0, 2); return sub.len() }' \
2

# Test 11: Multi-line strings
test_feature "Multi-line strings" \
'fn main() -> int { 
    var s = """hello
world"""
    return s.len() 
}' \
11

# Test 12: Array slice syntax
test_feature "Array slice syntax" \
"fn main() -> int { var arr = [10, 20, 30, 40, 50]; var s = arr[1..3]; return s.len() }" \
2

# Test 13: String slice syntax
test_feature "String slice syntax" \
'fn main() -> int { var s = "hello"; var sub = s[0..2]; return sub.len() }' \
2

echo ""
echo "=== Results ==="
echo "PASS: $PASS"
echo "FAIL: $FAIL"
echo ""

if [ $FAIL -eq 0 ]; then
    echo "✓ All tests passed!"
    exit 0
else
    echo "✗ Some tests failed"
    exit 1
fi
