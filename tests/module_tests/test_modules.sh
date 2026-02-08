#!/bin/bash
# Module System Tests

cd "$(dirname "$0")"

# Set WYN_ROOT so compiler can find src files
export WYN_ROOT="$(cd ../.. && pwd)"

PASS=0
FAIL=0

echo "=== Module System Tests ==="
echo ""

# Test 1: Basic module import
echo -n "Testing basic module import... "
mkdir -p /tmp/wyn_modules
cat > /tmp/wyn_modules/math.wyn << 'EOF'
pub fn add(a: int, b: int) -> int {
    return a + b
}

pub fn multiply(a: int, b: int) -> int {
    return a * b
}
EOF

cat > /tmp/test_import.wyn << 'EOF'
import math

fn main() -> int {
    return math.add(10, 20) + math.multiply(2, 3)
}
EOF

../../wyn /tmp/test_import.wyn 2>&1
if [ -f /tmp/test_import.wyn.out ]; then
    /tmp/test_import.wyn.out 2>/dev/null
    exit_code=$?
    if [ "$exit_code" -eq 36 ]; then
        echo "✓ PASS"
        ((PASS++))
    else
        echo "✗ FAIL (exit $exit_code, expected 36)"
        ((FAIL++))
    fi
else
    echo "✗ FAIL (compile error)"
    ((FAIL++))
fi

# Test 2: Module with struct
echo -n "Testing module with struct... "
cat > /tmp/wyn_modules/point.wyn << 'EOF'
pub struct Point {
    x: int,
    y: int
}

pub fn distance(p: Point) -> int {
    return p.x + p.y
}
EOF

cat > /tmp/test_struct.wyn << 'EOF'
import point

fn main() -> int {
    var p = point.Point { x: 3, y: 4 }
    return point.distance(p)
}
EOF

../../wyn /tmp/test_struct.wyn 2>&1 >/dev/null
if [ -f /tmp/test_struct.wyn.out ]; then
    /tmp/test_struct.wyn.out 2>/dev/null
    exit_code=$?
    if [ "$exit_code" -eq 7 ]; then
        echo "✓ PASS"
        ((PASS++))
    else
        echo "✗ FAIL (exit $exit_code, expected 7)"
        ((FAIL++))
    fi
else
    echo "✗ FAIL (compile error)"
    ((FAIL++))
fi

# Test 3: Nested imports
echo -n "Testing nested imports... "
cat > /tmp/wyn_modules/utils.wyn << 'EOF'
import math

pub fn calculate(a: int, b: int) -> int {
    return math.add(a, b) * 2
}
EOF

cat > /tmp/test_nested.wyn << 'EOF'
import utils

fn main() -> int {
    return utils.calculate(5, 10)
}
EOF

../../wyn /tmp/test_nested.wyn 2>&1
if [ -f /tmp/test_nested.wyn.out ]; then
    /tmp/test_nested.wyn.out 2>/dev/null
    exit_code=$?
    if [ "$exit_code" -eq 30 ]; then
        echo "✓ PASS"
        ((PASS++))
    else
        echo "✗ FAIL (exit $exit_code, expected 30)"
        ((FAIL++))
    fi
else
    echo "✗ FAIL (compile error)"
    ((FAIL++))
fi

echo ""
echo "=== Results ==="
echo "PASS: $PASS"
echo "FAIL: $FAIL"
echo ""

# Cleanup
rm -rf /tmp/wyn_modules

if [ $FAIL -eq 0 ]; then
    echo "✓ All module tests passed!"
    exit 0
else
    echo "✗ Some tests failed"
    exit 1
fi
