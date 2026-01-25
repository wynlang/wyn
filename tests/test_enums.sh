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

# Test 4: Enum constructors
cat > test_enum_constructors.wyn << 'ENDTEST'
enum Result {
    Ok(int),
    Err(string)
}

fn main() -> int {
    var success = Result_Ok(42)
    var failure = Result_Err("failed")
    print("Constructors work")
    return 0
}
ENDTEST

run_test "Enum constructors" test_enum_constructors.wyn

# Test 5: Enum toString
cat > test_enum_tostring.wyn << 'ENDTEST'
enum Status {
    Active,
    Inactive
}

fn main() -> int {
    var s = Active
    var str = Status_toString(s)
    print("toString works")
    return 0
}
ENDTEST

run_test "Enum toString" test_enum_tostring.wyn

# Test 6: Mixed enum variants (Some(T) + None)
cat > test_mixed_enum.wyn << 'ENDTEST'
enum Option {
    Some(int),
    None
}

fn main() -> int {
    var some_val = Option_Some(42)
    var none_val = Option_None()
    
    var s1 = Option_toString(some_val)
    var s2 = Option_toString(none_val)
    
    if s1 == "Some" {
        if s2 == "None" {
            print("✓ Mixed enum variants work")
            return 0
        }
    }
    
    print("✗ Mixed enum variants failed")
    return 1
}
ENDTEST

run_test "Mixed enum variants" test_mixed_enum.wyn

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
