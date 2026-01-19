#!/bin/bash
# TDD Test Suite for v1.1.0 New Features

set -e
cd "$(dirname "$0")"

PASS=0
FAIL=0

test_feature() {
    local name="$1"
    local code="$2"
    local expected_exit="$3"
    
    echo -n "Testing $name... "
    echo "$code" > /tmp/test_feature.wyn
    
    if timeout 2 ./wyn /tmp/test_feature.wyn 2>&1 | grep -q "Compiled successfully"; then
        if [ -f /tmp/test_feature.wyn.out ]; then
            timeout 2 /tmp/test_feature.wyn.out 2>/dev/null
            actual_exit=$?
            if [ "$actual_exit" -eq "$expected_exit" ]; then
                echo "✓ PASS"
                ((PASS++))
            else
                echo "✗ FAIL (exit $actual_exit, expected $expected_exit)"
                ((FAIL++))
            fi
        else
            echo "✗ FAIL (no output file)"
            ((FAIL++))
        fi
    else
        echo "✗ FAIL (compile error)"
        ((FAIL++))
    fi
}

echo "=== v1.1.0 Feature Tests ==="
echo ""

# 1. Hex literals
test_feature "Hex literals" \
"fn main() -> int { return 0xFF }" \
255

# 2. Binary literals
test_feature "Binary literals" \
"fn main() -> int { return 0b1010 }" \
10

# 3. Underscore in numbers
test_feature "Underscore in numbers" \
"fn main() -> int { return 1_000 + 2_000 }" \
3000

# 4. String escapes
test_feature "String escape \\n" \
'fn main() -> int { var s = "hello\nworld"; return s.len() }' \
11

test_feature "String escape \\t" \
'fn main() -> int { var s = "a\tb"; return s.len() }' \
3

# 5. Compiler flags
echo -n "Testing --version flag... "
if ./wyn --version 2>&1 | grep -q "1.1"; then
    echo "✓ PASS"
    ((PASS++))
else
    echo "✗ FAIL"
    ((FAIL++))
fi

echo -n "Testing --help flag... "
if ./wyn --help 2>&1 | grep -q "Usage"; then
    echo "✓ PASS"
    ((PASS++))
else
    echo "✗ FAIL"
    ((FAIL++))
fi

echo -n "Testing -o flag... "
echo "fn main() -> int { return 42 }" > /tmp/test_output.wyn
if ./wyn /tmp/test_output.wyn -o /tmp/custom_output >/dev/null 2>&1; then
    if [ -f /tmp/custom_output ]; then
        echo "✓ PASS"
        ((PASS++))
        rm -f /tmp/custom_output
    else
        echo "✗ FAIL (no output file)"
        ((FAIL++))
    fi
else
    echo "✗ FAIL (compile error)"
    ((FAIL++))
fi

# 6. Array slicing
test_feature "Array slicing" \
"fn main() -> int { var arr = [1, 2, 3, 4, 5]; var slice = arr[1..3]; return slice.len() }" \
2

# 7. String slicing
test_feature "String slicing" \
'fn main() -> int { var s = "hello"; var sub = s[0..2]; return sub.len() }' \
2

# 8. Multi-line strings
test_feature "Multi-line strings" \
'fn main() -> int { var s = """hello
world"""; return s.len() }' \
11

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
