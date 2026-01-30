#!/bin/bash
# Comprehensive modules system validation

WYN_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export WYN_ROOT
WYN_BIN="$WYN_ROOT/wyn"

echo "=== Modules System Validation ==="
echo ""

cd /tmp
rm -rf module_validation
mkdir -p module_validation
cd module_validation

# Test 1: Single module import
echo "Test 1: Single Module Import"
cat > utils.wyn << 'EOF'
pub fn helper() -> int {
    return 42
}
EOF

cat > test1.wyn << 'EOF'
import root::utils

fn main() -> int {
    var result = utils::helper()
    if result == 42 {
        print("PASS")
        return 0
    }
    return 1
}
EOF

if $WYN_BIN run test1.wyn 2>&1 | grep -q "PASS"; then
    echo "  ✅ Single module import works"
else
    echo "  ❌ Single module import failed"
    exit 1
fi

# Test 2: Multiple module imports
echo "Test 2: Multiple Module Imports"
cat > math.wyn << 'EOF'
pub fn add(a: int, b: int) -> int {
    return a + b
}
EOF

cat > string.wyn << 'EOF'
pub fn greet(name: string) -> string {
    return "Hello, " + name
}
EOF

cat > test2.wyn << 'EOF'
import root::math
import root::string

fn main() -> int {
    var sum = math::add(5, 3)
    var msg = string::greet("World")
    if sum == 8 {
        print("PASS")
        return 0
    }
    return 1
}
EOF

if $WYN_BIN run test2.wyn 2>&1 | grep -q "PASS"; then
    echo "  ✅ Multiple module imports work"
else
    echo "  ❌ Multiple module imports failed"
    exit 1
fi

# Test 3: Cross-module function calls
echo "Test 3: Cross-Module Function Calls"
cat > calc.wyn << 'EOF'
pub fn multiply(a: int, b: int) -> int {
    return a * b
}

pub fn square(x: int) -> int {
    return multiply(x, x)
}
EOF

cat > test3.wyn << 'EOF'
import root::calc

fn main() -> int {
    var result = calc::square(7)
    if result == 49 {
        print("PASS")
        return 0
    }
    return 1
}
EOF

if $WYN_BIN run test3.wyn 2>&1 | grep -q "PASS"; then
    echo "  ✅ Cross-module function calls work"
else
    echo "  ❌ Cross-module function calls failed"
    exit 1
fi

# Test 4: Module with multiple functions
echo "Test 4: Module with Multiple Functions"
cat > ops.wyn << 'EOF'
pub fn add(a: int, b: int) -> int {
    return a + b
}

pub fn sub(a: int, b: int) -> int {
    return a - b
}

pub fn mul(a: int, b: int) -> int {
    return a * b
}
EOF

cat > test4.wyn << 'EOF'
import root::ops

fn main() -> int {
    var a = ops::add(10, 5)
    var b = ops::sub(10, 5)
    var c = ops::mul(10, 5)
    if a == 15 {
        if b == 5 {
            if c == 50 {
                print("PASS")
                return 0
            }
        }
    }
    return 1
}
EOF

if $WYN_BIN run test4.wyn 2>&1 | grep -q "PASS"; then
    echo "  ✅ Multiple functions per module work"
else
    echo "  ❌ Multiple functions per module failed"
    exit 1
fi

# Test 5: Compile (not run) also works
echo "Test 5: Compile Command"
cat > test5.wyn << 'EOF'
import root::math

fn main() -> int {
    var x = math::add(1, 2)
    print("PASS")
    return 0
}
EOF

if $WYN_BIN test5.wyn 2>&1 | grep -q "Compiled successfully"; then
    if ./test5.wyn.out 2>&1 | grep -q "PASS"; then
        echo "  ✅ Compile command works with modules"
    else
        echo "  ❌ Compiled binary failed"
        exit 1
    fi
else
    echo "  ❌ Compile command failed"
    exit 1
fi

echo ""
echo "==================================="
echo "✅ All modules tests passed!"
echo "==================================="
echo ""
echo "Summary:"
echo "  ✅ Single module import"
echo "  ✅ Multiple module imports"
echo "  ✅ Cross-module function calls"
echo "  ✅ Multiple functions per module"
echo "  ✅ Compile command with modules"
echo ""
echo "Modules System: FULLY FUNCTIONAL ✅"

# Cleanup
cd /tmp
rm -rf module_validation
