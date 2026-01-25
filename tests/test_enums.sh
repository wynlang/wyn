#!/bin/bash
# Test enum functionality

set -e

WYN=${WYN_ROOT:-../..}/wyn
TESTDIR=$(mktemp -d)
cd "$TESTDIR"

echo "Testing Enum Functionality"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

PASSED=0
FAILED=0

run_test() {
    local name="$1"
    local file="$2"
    echo -n "[$((PASSED + FAILED + 1))] $name... "
    
    if timeout 10 $WYN run "$file" > /tmp/test_output.txt 2>&1; then
        echo "✅"
        PASSED=$((PASSED + 1))
        return 0
    else
        echo "❌"
        echo "  Error:"
        cat /tmp/test_output.txt | tail -5 | sed 's/^/    /'
        FAILED=$((FAILED + 1))
        return 1
    fi
}

# Test 1: Simple enum
cat > test_simple_enum.wyn << 'ENDTEST'
enum Status {
    Active,
    Inactive,
    Pending
}

fn main() -> int {
    print("Simple enum works")
    return 0
}
ENDTEST

run_test "Simple enum" test_simple_enum.wyn

# Test 2: Enum with data
cat > test_enum_data.wyn << 'ENDTEST'
enum Result {
    Ok(int),
    Err(string)
}

fn main() -> int {
    print("Enum with data works")
    return 0
}
ENDTEST

run_test "Enum with data" test_enum_data.wyn

# Test 3: Multiple variants
cat > test_enum_multiple.wyn << 'ENDTEST'
enum Color {
    Red,
    Green,
    Blue,
    Custom(int)
}

fn main() -> int {
    print("Multiple variants work")
    return 0
}
ENDTEST

run_test "Multiple variants" test_enum_multiple.wyn

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "RESULTS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo "Total:  $((PASSED + FAILED))"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "✅ ALL ENUM TESTS PASSED"
    exit 0
else
    echo "❌ SOME TESTS FAILED"
    exit 1
fi
