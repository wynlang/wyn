#!/bin/bash
# Epic 6: Error Messages Test Suite

cd "$(dirname "$0")/.."

echo "=== Epic 6: Error Messages Tests ==="
echo ""

pass=0
fail=0

# Test 1: Undefined variable with suggestions
test_undefined_var() {
    cat > /tmp/test_undefined.wyn << 'EOF'
fn main() -> int {
    var hello = 5
    return helo
}
EOF
    output=$(./wyn-llvm /tmp/test_undefined.wyn 2>&1)
    if echo "$output" | grep -q "Undefined variable" && echo "$output" | grep -q "line 3"; then
        echo "✓ Test 1: Undefined variable error"
        return 0
    else
        echo "✗ Test 1: Undefined variable error"
        return 1
    fi
}

# Test 2: Type mismatch
test_type_mismatch() {
    cat > /tmp/test_type.wyn << 'EOF'
fn takes_int(x: int) -> int {
    return x
}

fn main() -> int {
    return takes_int("string")
}
EOF
    output=$(./wyn-llvm /tmp/test_type.wyn 2>&1)
    if echo "$output" | grep -qi "type\|mismatch\|expected.*int"; then
        echo "✓ Test 2: Type mismatch error"
        return 0
    else
        echo "✗ Test 2: Type mismatch error (not detected)"
        return 1
    fi
}

# Test 3: Wrong argument count
test_arg_count() {
    cat > /tmp/test_args.wyn << 'EOF'
fn add(a: int, b: int) -> int {
    return a + b
}

fn main() -> int {
    return add(1)
}
EOF
    output=$(./wyn-llvm /tmp/test_args.wyn 2>&1)
    if echo "$output" | grep -qi "argument"; then
        echo "✓ Test 3: Wrong argument count"
        return 0
    else
        echo "✗ Test 3: Wrong argument count"
        return 1
    fi
}

# Test 4: Undefined function
test_undefined_func() {
    cat > /tmp/test_func.wyn << 'EOF'
fn main() -> int {
    return undefined_function()
}
EOF
    output=$(./wyn-llvm /tmp/test_func.wyn 2>&1)
    if echo "$output" | grep -qi "undefined.*function"; then
        echo "✓ Test 4: Undefined function"
        return 0
    else
        echo "✗ Test 4: Undefined function"
        return 1
    fi
}

# Test 5: Parse error with context
test_parse_error() {
    cat > /tmp/test_parse.wyn << 'EOF'
fn main() -> int
    return 5
}
EOF
    output=$(./wyn-llvm /tmp/test_parse.wyn 2>&1)
    if echo "$output" | grep -qi "error\|expected"; then
        echo "✓ Test 5: Parse error"
        return 0
    else
        echo "✗ Test 5: Parse error"
        return 1
    fi
}

# Run tests
test_undefined_var && ((pass++)) || ((fail++))
test_type_mismatch && ((pass++)) || ((fail++))
test_arg_count && ((pass++)) || ((fail++))
test_undefined_func && ((pass++)) || ((fail++))
test_parse_error && ((pass++)) || ((fail++))

echo ""
echo "Results: $pass/$((pass+fail)) passing"

exit $fail
