#!/bin/bash
# Epic 8: Tooling Tests

cd "$(dirname "$0")/.."

echo "=== Epic 8: Tooling Tests ==="
echo ""

pass=0
fail=0

# Test 1: Compiler version
test_version() {
    output=$(./wyn-llvm --version 2>&1)
    if echo "$output" | grep -qi "wyn\|version"; then
        echo "✓ Test 1: --version flag"
        return 0
    else
        echo "✗ Test 1: --version flag"
        return 1
    fi
}

# Test 2: Help message
test_help() {
    output=$(./wyn-llvm --help 2>&1)
    if echo "$output" | grep -qi "usage\|options\|help"; then
        echo "✓ Test 2: --help flag"
        return 0
    else
        echo "✗ Test 2: --help flag"
        return 1
    fi
}

# Test 3: Output file specification
test_output() {
    cat > /tmp/test_output.wyn << 'EOF'
fn main() -> int {
    return 42
}
EOF
    ./wyn-llvm /tmp/test_output.wyn -o /tmp/custom_output 2>&1 > /dev/null
    if [ -f /tmp/custom_output ]; then
        /tmp/custom_output
        result=$?
        rm -f /tmp/custom_output
        if [ $result -eq 42 ]; then
            echo "✓ Test 3: -o output flag"
            return 0
        fi
    fi
    echo "✗ Test 3: -o output flag"
    return 1
}

# Test 4: Module system integration
test_multi_file() {
    # Test that module imports are recognized
    cat > /tmp/test_import.wyn << 'EOF'
import math_utils

fn main() -> int {
    return 0
}
EOF
    output=$(./wyn-llvm /tmp/test_import.wyn 2>&1)
    # Should either compile or show module-related error (not syntax error)
    if echo "$output" | grep -qi "module\|import" || echo "$output" | grep -qi "compiled successfully"; then
        echo "✓ Test 4: Module system integration"
        return 0
    else
        echo "✗ Test 4: Module system integration"
        return 1
    fi
}

# Test 5: Error reporting
test_error_reporting() {
    cat > /tmp/test_error.wyn << 'EOF'
fn main() -> int {
    return undefined_var
}
EOF
    output=$(./wyn-llvm /tmp/test_error.wyn 2>&1)
    if echo "$output" | grep -qi "error" && echo "$output" | grep -qi "line"; then
        echo "✓ Test 5: Error reporting with line numbers"
        return 0
    else
        echo "✗ Test 5: Error reporting"
        return 1
    fi
}

# Test 6: LLVM IR emission
test_llvm_ir() {
    cat > /tmp/test_ir.wyn << 'EOF'
fn main() -> int {
    return 42
}
EOF
    ./wyn-llvm /tmp/test_ir.wyn 2>&1 > /dev/null
    if [ -f /tmp/test_ir.ll ]; then
        if grep -q "define" /tmp/test_ir.ll; then
            echo "✓ Test 6: LLVM IR generation"
            return 0
        fi
    fi
    echo "✗ Test 6: LLVM IR generation"
    return 1
}

# Test 7: Compilation speed
test_compile_speed() {
    cat > /tmp/test_speed.wyn << 'EOF'
fn main() -> int {
    return 42
}
EOF
    start=$(date +%s%N)
    ./wyn-llvm /tmp/test_speed.wyn 2>&1 > /dev/null
    end=$(date +%s%N)
    elapsed=$(( (end - start) / 1000000 ))
    
    if [ $elapsed -lt 5000 ]; then  # Less than 5 seconds
        echo "✓ Test 7: Compilation speed (${elapsed}ms)"
        return 0
    else
        echo "✗ Test 7: Compilation too slow (${elapsed}ms)"
        return 1
    fi
}

# Run tests
test_version && ((pass++)) || ((fail++))
test_help && ((pass++)) || ((fail++))
test_output && ((pass++)) || ((fail++))
test_multi_file && ((pass++)) || ((fail++))
test_error_reporting && ((pass++)) || ((fail++))
test_llvm_ir && ((pass++)) || ((fail++))
test_compile_speed && ((pass++)) || ((fail++))

echo ""
echo "Results: $pass/$((pass+fail)) passing"

exit $fail
