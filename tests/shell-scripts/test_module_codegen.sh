#!/bin/bash
# Test: Module system codegen
# Tests that imported modules emit their code correctly

echo "=== Test 1: Simple function import ==="

# Create math module
cat > /tmp/test_math.wyn << 'EOF'
export fn add(a: int, b: int) -> int {
    return a + b
}
EOF

# Create main file
cat > /tmp/test_main.wyn << 'EOF'
import test_math

fn main() -> int {
    var result = test_math_add(2, 3)
    return result
}
EOF

# Compile
cd wyn
./wyn /tmp/test_main.wyn 2>&1

# Check if compiled
if [ -f /tmp/test_main.wyn.out ]; then
    echo "✅ Binary generated"
    
    # Run and check exit code
    /tmp/test_main.wyn.out
    result=$?
    if [ "$result" = "5" ]; then
        echo "✅ Test 1 PASSED: Simple import works (2+3=5)"
    else
        echo "❌ Test 1 FAILED: Expected exit code 5, got $result"
        exit 1
    fi
else
    echo "❌ No binary generated"
    exit 1
fi

echo ""
echo "=== Test 2: Multiple imports ==="

# Create string module
cat > /tmp/test_string.wyn << 'EOF'
export fn length(s: string) -> int {
    return 5
}
EOF

# Create main with multiple imports
cat > /tmp/test_multi.wyn << 'EOF'
import test_math
import test_string

fn main() -> int {
    var sum = test_math_add(2, 3)
    var len = test_string_length("hello")
    return sum + len
}
EOF

# Compile
./wyn /tmp/test_multi.wyn 2>&1

if [ -f /tmp/test_multi.wyn.out ]; then
    /tmp/test_multi.wyn.out
    result=$?
    if [ "$result" = "10" ]; then
        echo "✅ Test 2 PASSED: Multiple imports work (5+5=10)"
    else
        echo "❌ Test 2 FAILED: Expected exit code 10, got $result"
        exit 1
    fi
else
    echo "❌ Test 2 FAILED: No binary generated"
    exit 1
fi

echo ""
echo "=== Test 3: Function with print ==="

# Create main that prints result
cat > /tmp/test_print.wyn << 'EOF'
import test_math

fn main() -> int {
    var result = test_math_add(10, 20)
    print(result)
    return 0
}
EOF

# Compile
./wyn /tmp/test_print.wyn 2>&1

if [ -f /tmp/test_print.wyn.out ]; then
    output=$(/tmp/test_print.wyn.out)
    if [ "$output" = "30" ]; then
        echo "✅ Test 3 PASSED: Print works with imported functions"
    else
        echo "❌ Test 3 FAILED: Expected output '30', got '$output'"
        exit 1
    fi
else
    echo "❌ Test 3 FAILED: No binary generated"
    exit 1
fi

echo ""
echo "=== All module codegen tests PASSED ==="
