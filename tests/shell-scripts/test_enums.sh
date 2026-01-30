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

# Test 7: Option.unwrap()
cat > test_option_unwrap.wyn << 'ENDTEST'
enum Option {
    Some(int),
    None
}

fn main() -> int {
    var some = Option_Some(42)
    var val = some.unwrap()
    
    if val == 42 {
        print("✓ Option.unwrap() works")
        return 0
    }
    
    return 1
}
ENDTEST

run_test "Option.unwrap()" test_option_unwrap.wyn

# Test 8: Result.unwrap()
cat > test_result_unwrap.wyn << 'ENDTEST'
enum Result {
    Ok(int),
    Err(string)
}

fn main() -> int {
    var ok = Result_Ok(42)
    var val = ok.unwrap()
    
    if val == 42 {
        print("✓ Result.unwrap() works")
        return 0
    }
    
    return 1
}
ENDTEST

run_test "Result.unwrap()" test_result_unwrap.wyn

# Test 9: Option.is_some/is_none
cat > test_option_is_some.wyn << 'ENDTEST'
enum Option {
    Some(int),
    None
}

fn main() -> int {
    var some = Option_Some(42)
    var none = Option_None()
    
    if some.is_some() {
        if none.is_none() {
            print("✓ is_some/is_none work")
            return 0
        }
    }
    
    return 1
}
ENDTEST

run_test "Option.is_some/is_none" test_option_is_some.wyn

# Test 10: Result.is_ok/is_err
cat > test_result_is_ok.wyn << 'ENDTEST'
enum Result {
    Ok(int),
    Err(string)
}

fn main() -> int {
    var ok = Result_Ok(42)
    var err = Result_Err("failed")
    
    if ok.is_ok() {
        if err.is_err() {
            print("✓ is_ok/is_err work")
            return 0
        }
    }
    
    return 1
}
ENDTEST

run_test "Result.is_ok/is_err" test_result_is_ok.wyn

# Test 11: Option.unwrap_or
cat > test_option_unwrap_or.wyn << 'ENDTEST'
enum Option {
    Some(int),
    None
}

fn main() -> int {
    var some = Option_Some(42)
    var none = Option_None()
    
    var val1 = some.unwrap_or(0)
    var val2 = none.unwrap_or(99)
    
    if val1 == 42 {
        if val2 == 99 {
            print("✓ unwrap_or works")
            return 0
        }
    }
    
    return 1
}
ENDTEST

run_test "Option.unwrap_or" test_option_unwrap_or.wyn

# Test 12: Result.unwrap_or
cat > test_result_unwrap_or.wyn << 'ENDTEST'
enum Result {
    Ok(int),
    Err(string)
}

fn main() -> int {
    var ok = Result_Ok(42)
    var err = Result_Err("failed")
    
    var val1 = ok.unwrap_or(0)
    var val2 = err.unwrap_or(99)
    
    if val1 == 42 {
        if val2 == 99 {
            print("✓ unwrap_or works")
            return 0
        }
    }
    
    return 1
}
ENDTEST

run_test "Result.unwrap_or" test_result_unwrap_or.wyn

# Test 13: Enum variant destructuring in match
cat > test_enum_match.wyn << 'ENDTEST'
enum Option {
    Some(int),
    None
}

fn main() -> int {
    var some = Option_Some(42)
    var none = Option_None()
    
    var val1 = match some {
        Option_Some(x) => x,
        Option_None => 0
    }
    
    var val2 = match none {
        Option_Some(x) => x,
        Option_None => -1
    }
    
    if val1 == 42 {
        if val2 == -1 {
            print("✓ Enum destructuring in match works")
            return 0
        }
    }
    
    return 1
}
ENDTEST

run_test "Enum destructuring in match" test_enum_match.wyn

# Test 14: Comprehensive enum destructuring (Option + Result)
cat > test_enum_match_comprehensive.wyn << 'ENDTEST'
enum Option {
    Some(int),
    None
}

enum Result {
    Ok(int),
    Err(string)
}

fn main() -> int {
    var some = Option_Some(42)
    var ok = Result_Ok(100)
    var err = Result_Err("error")
    
    var opt_val = match some {
        Option_Some(x) => x,
        Option_None => 0
    }
    
    var ok_val = match ok {
        Result_Ok(x) => x,
        Result_Err(e) => 0
    }
    
    var err_val = match err {
        Result_Ok(x) => x,
        Result_Err(e) => -1
    }
    
    if opt_val == 42 {
        if ok_val == 100 {
            if err_val == -1 {
                print("✓ Option and Result destructuring work")
                return 0
            }
        }
    }
    
    return 1
}
ENDTEST

run_test "Option and Result destructuring" test_enum_match_comprehensive.wyn

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
