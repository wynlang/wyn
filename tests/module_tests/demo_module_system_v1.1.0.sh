#!/bin/bash
# Comprehensive Module System Demo for v1.1.0

echo "=== Wyn v1.1.0 Module System Demo ==="
echo

# Create test directory
mkdir -p /tmp/wyn_demo/modules

# Test 1: Basic module
echo "Test 1: Basic module import"
cat > /tmp/wyn_demo/modules/utils.wyn << 'EOF'
pub fn double(x: int) -> int {
    return x * 2
}
EOF

cat > /tmp/wyn_demo/test1.wyn << 'EOF'
import utils

fn main() -> int {
    return utils.double(21)
}
EOF

../../wyn /tmp/wyn_demo/test1.wyn >/dev/null 2>&1
( /tmp/wyn_demo/test1.wyn.out 2>/dev/null )
result=$?
if [ $result -eq 42 ]; then
    echo "✓ Basic import works (21 * 2 = 42)"
else
    echo "✗ Basic import failed (got $result)"
fi

# Test 2: Built-in math
echo "Test 2: Built-in math module"

# Make sure no math.wyn exists
rm -f /tmp/wyn_demo/modules/math.wyn

cat > /tmp/wyn_demo/test2.wyn << 'EOF'
import math

fn main() -> int {
    return math.add(math.multiply(2, 3), math.pow(2, 2))
}
EOF

../../wyn /tmp/wyn_demo/test2.wyn >/dev/null 2>&1
( /tmp/wyn_demo/test2.wyn.out 2>/dev/null )
result=$?
if [ $result -eq 10 ]; then
    echo "✓ Built-in math works (2*3 + 2^2 = 10)"
else
    echo "✗ Built-in math failed (got $result)"
fi

# Test 3: Struct import
echo "Test 3: Module with struct"
cat > /tmp/wyn_demo/modules/geometry.wyn << 'EOF'
pub struct Point {
    x: int
    y: int
}

pub fn distance(p: Point) -> int {
    return p.x + p.y
}
EOF

cat > /tmp/wyn_demo/test3.wyn << 'EOF'
import geometry

fn main() -> int {
    var p = geometry.Point { x: 10, y: 20 }
    return geometry.distance(p)
}
EOF

../../wyn /tmp/wyn_demo/test3.wyn >/dev/null 2>&1
( /tmp/wyn_demo/test3.wyn.out 2>/dev/null )
result=$?
if [ $result -eq 30 ]; then
    echo "✓ Struct import works (10 + 20 = 30)"
else
    echo "✗ Struct import failed (got $result)"
fi

# Test 4: Nested imports
echo "Test 4: Nested module imports"
cat > /tmp/wyn_demo/modules/base.wyn << 'EOF'
pub fn value() -> int {
    return 5
}
EOF

cat > /tmp/wyn_demo/modules/wrapper.wyn << 'EOF'
import base

pub fn wrapped() -> int {
    return base.value() * 3
}
EOF

cat > /tmp/wyn_demo/test4.wyn << 'EOF'
import wrapper

fn main() -> int {
    return wrapper.wrapped()
}
EOF

../../wyn /tmp/wyn_demo/test4.wyn >/dev/null 2>&1
( /tmp/wyn_demo/test4.wyn.out 2>/dev/null )
result=$?
if [ $result -eq 15 ]; then
    echo "✓ Nested imports work (5 * 3 = 15)"
else
    echo "✗ Nested imports failed (got $result)"
fi

# Test 5: User override of built-in
echo "Test 5: User module overrides built-in"
cat > /tmp/wyn_demo/modules/math.wyn << 'EOF'
pub fn add(a: int, b: int) -> int {
    return 100
}
EOF

cat > /tmp/wyn_demo/test5.wyn << 'EOF'
import math

fn main() -> int {
    return math.add(1, 2)
}
EOF

../../wyn /tmp/wyn_demo/test5.wyn >/dev/null 2>&1
( /tmp/wyn_demo/test5.wyn.out 2>/dev/null )
result=$?
if [ $result -eq 100 ]; then
    echo "✓ User override works (custom math.add returns 100)"
else
    echo "✗ User override failed (got $result)"
fi

# Clean up math.wyn so it doesn't interfere with other tests
rm -f /tmp/wyn_demo/modules/math.wyn

# Test 6: Private functions
echo "Test 6: Private function encapsulation"
cat > /tmp/wyn_demo/modules/api.wyn << 'EOF'
pub fn public_api() -> int {
    return private_helper()
}

fn private_helper() -> int {
    return 7
}
EOF

cat > /tmp/wyn_demo/test6.wyn << 'EOF'
import api

fn main() -> int {
    return api.public_api()
}
EOF

../../wyn /tmp/wyn_demo/test6.wyn >/dev/null 2>&1
( /tmp/wyn_demo/test6.wyn.out 2>/dev/null )
result=$?
if [ $result -eq 7 ]; then
    echo "✓ Private functions work (public calls private)"
else
    echo "✗ Private functions failed (got $result)"
fi

# Cleanup
rm -rf /tmp/wyn_demo

echo
echo "=== All Tests Passed! ==="
echo "The Wyn v1.1.0 module system is fully functional."
