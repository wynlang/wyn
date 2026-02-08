#!/bin/bash
# Comprehensive Module System Tests for v1.1.0

cd "$(dirname "$0")"

# Set WYN_ROOT so compiler can find src files
export WYN_ROOT="$(cd ../.. && pwd)"

PASS=0
FAIL=0

echo "=== Wyn v1.1.0 Module System Tests ==="
echo ""

# Setup test directories
mkdir -p /tmp/wyn_modules
mkdir -p modules
mkdir -p ~/.wyn/modules
mkdir -p ~/.wyn/packages/mypackage

# Test 1: Local module (current directory)
echo -n "Testing local module... "
cat > local_utils.wyn << 'EOF'
pub fn double(x: int) -> int {
    return x * 2
}
EOF

cat > /tmp/test_local.wyn << 'EOF'
import local_utils

fn main() -> int {
    return local_utils.double(21)
}
EOF

cp local_utils.wyn /tmp/
../../wyn /tmp/test_local.wyn 2>&1
if [ -f /tmp/test_local.wyn.out ]; then
    /tmp/test_local.wyn.out 2>/dev/null
    if [ $? -eq 42 ]; then
        echo "✓ PASS"
        ((PASS++))
    else
        echo "✗ FAIL (wrong result)"
        ((FAIL++))
    fi
else
    echo "✗ FAIL (compile error)"
    ((FAIL++))
fi

# Test 3: Project modules directory
echo -n "Testing ./modules/ directory... "
cat > modules/project_utils.wyn << 'EOF'
pub fn triple(x: int) -> int {
    return x * 3
}
EOF

cat > /tmp/test_project_modules.wyn << 'EOF'
import project_utils

fn main() -> int {
    return project_utils.triple(7)
}
EOF

../../wyn /tmp/test_project_modules.wyn 2>&1
if [ -f /tmp/test_project_modules.wyn.out ]; then
    /tmp/test_project_modules.wyn.out 2>/dev/null
    if [ $? -eq 21 ]; then
        echo "✓ PASS"
        ((PASS++))
    else
        echo "✗ FAIL (wrong result)"
        ((FAIL++))
    fi
else
    echo "✗ FAIL (compile error)"
    ((FAIL++))
fi

# Test 4: User modules (~/.wyn/modules/)
echo -n "Testing ~/.wyn/modules/ directory... "
cat > ~/.wyn/modules/user_utils.wyn << 'EOF'
pub fn quadruple(x: int) -> int {
    return x * 4
}
EOF

cat > /tmp/test_user_modules.wyn << 'EOF'
import user_utils

fn main() -> int {
    return user_utils.quadruple(5)
}
EOF

../../wyn /tmp/test_user_modules.wyn 2>&1
if [ -f /tmp/test_user_modules.wyn.out ]; then
    /tmp/test_user_modules.wyn.out 2>/dev/null
    if [ $? -eq 20 ]; then
        echo "✓ PASS"
        ((PASS++))
    else
        echo "✗ FAIL (wrong result)"
        ((FAIL++))
    fi
else
    echo "✗ FAIL (compile error)"
    ((FAIL++))
fi

# Test 5: Package directory (~/.wyn/packages/)
echo -n "Testing ~/.wyn/packages/ directory... "
cat > ~/.wyn/packages/mypackage/mypackage.wyn << 'EOF'
pub fn quintuple(x: int) -> int {
    return x * 5
}
EOF

cat > /tmp/test_packages.wyn << 'EOF'
import mypackage

fn main() -> int {
    return mypackage.quintuple(4)
}
EOF

../../wyn /tmp/test_packages.wyn 2>&1
if [ -f /tmp/test_packages.wyn.out ]; then
    /tmp/test_packages.wyn.out 2>/dev/null
    if [ $? -eq 20 ]; then
        echo "✓ PASS"
        ((PASS++))
    else
        echo "✗ FAIL (wrong result)"
        ((FAIL++))
    fi
else
    echo "✗ FAIL (compile error)"
    ((FAIL++))
fi

# Test 6: Module with struct
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

../../wyn /tmp/test_struct.wyn 2>&1
if [ -f /tmp/test_struct.wyn.out ]; then
    /tmp/test_struct.wyn.out 2>/dev/null
    if [ $? -eq 7 ]; then
        echo "✓ PASS"
        ((PASS++))
    else
        echo "✗ FAIL (wrong result)"
        ((FAIL++))
    fi
else
    echo "✗ FAIL (compile error)"
    ((FAIL++))
fi

# Test 7: Nested imports
echo -n "Testing nested imports... "
cat > /tmp/wyn_modules/math.wyn << 'EOF'
pub fn add(a: int, b: int) -> int {
    return a + b
}
EOF

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
    if [ $? -eq 30 ]; then
        echo "✓ PASS"
        ((PASS++))
    else
        echo "✗ FAIL (wrong result)"
        ((FAIL++))
    fi
else
    echo "✗ FAIL (compile error)"
    ((FAIL++))
fi

# Test 8: Priority order (local overrides user)
echo -n "Testing module priority order... "
cat > priority_test.wyn << 'EOF'
pub fn value() -> int {
    return 100
}
EOF

cat > ~/.wyn/modules/priority_test.wyn << 'EOF'
pub fn value() -> int {
    return 200
}
EOF

cat > /tmp/test_priority.wyn << 'EOF'
import priority_test

fn main() -> int {
    return priority_test.value()
}
EOF

cp priority_test.wyn /tmp/
../../wyn /tmp/test_priority.wyn 2>&1
if [ -f /tmp/test_priority.wyn.out ]; then
    /tmp/test_priority.wyn.out 2>/dev/null
    if [ $? -eq 100 ]; then
        echo "✓ PASS (local takes priority)"
        ((PASS++))
    else
        echo "✗ FAIL (wrong priority)"
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
    echo "✓ All module system tests passed!"
    exit 0
else
    echo "✗ Some tests failed"
    exit 1
fi
