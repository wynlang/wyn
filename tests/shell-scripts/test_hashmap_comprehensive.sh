#!/bin/bash
# Comprehensive HashMap tests

set -e

WYN=${WYN_ROOT:-../..}/wyn
TESTDIR=$(mktemp -d)
cd "$TESTDIR"

echo "Testing HashMap Comprehensive Functionality"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

PASSED=0
FAILED=0

run_test() {
    local name="$1"
    local file="$2"
    echo -n "[$((PASSED + FAILED + 1))] $name... "
    
    if timeout 10 $WYN run "$file" > /tmp/test_output.txt 2>&1; then
        if grep -q "✓" /tmp/test_output.txt 2>/dev/null; then
            echo "✅"
            PASSED=$((PASSED + 1))
            return 0
        fi
    fi
    
    echo "❌"
    echo "  Error:"
    cat /tmp/test_output.txt | tail -5 | sed 's/^/    /'
    FAILED=$((FAILED + 1))
    return 1
}

# Test 1: Basic operations
cat > test_basic.wyn << 'ENDTEST'
fn main() -> int {
    var m: HashMap<string, int> = {}
    m.insert("a", 1)
    m.insert("b", 2)
    
    var val_a = m.get("a")
    var val_b = m.get("b")
    
    if val_a == 1 {
        if val_b == 2 {
            print("✓ Basic operations work")
            return 0
        }
    }
    
    print("✗ Basic operations failed")
    return 1
}
ENDTEST

run_test "Basic insert/get" test_basic.wyn

# Test 2: Index syntax
cat > test_index.wyn << 'ENDTEST'
fn main() -> int {
    var m: HashMap<string, int> = {}
    m["x"] = 10
    m["y"] = 20
    
    var x = m["x"]
    var y = m["y"]
    
    if x == 10 {
        if y == 20 {
            print("✓ Index syntax works")
            return 0
        }
    }
    
    print("✗ Index syntax failed")
    return 1
}
ENDTEST

run_test "Index syntax" test_index.wyn

# Test 3: Variable keys
cat > test_var_keys.wyn << 'ENDTEST'
fn main() -> int {
    var m: HashMap<string, int> = {}
    var key1 = "first"
    var key2 = "second"
    
    m[key1] = 100
    m[key2] = 200
    
    if m[key1] == 100 {
        if m[key2] == 200 {
            print("✓ Variable keys work")
            return 0
        }
    }
    
    print("✗ Variable keys failed")
    return 1
}
ENDTEST

run_test "Variable keys" test_var_keys.wyn

# Test 4: Contains
cat > test_contains.wyn << 'ENDTEST'
fn main() -> int {
    var m: HashMap<string, int> = {}
    m.insert("exists", 42)
    
    var has_exists = m.contains("exists")
    var has_missing = m.contains("missing")
    
    if has_exists {
        if has_missing == false {
            print("✓ Contains works")
            return 0
        }
    }
    
    print("✗ Contains failed")
    return 1
}
ENDTEST

run_test "Contains method" test_contains.wyn

# Test 5: Length
cat > test_len.wyn << 'ENDTEST'
fn main() -> int {
    var m: HashMap<string, int> = {}
    
    var len0 = m.len()
    m.insert("a", 1)
    var len1 = m.len()
    m.insert("b", 2)
    var len2 = m.len()
    
    if len0 == 0 {
        if len1 == 1 {
            if len2 == 2 {
                print("✓ Length works")
                return 0
            }
        }
    }
    
    print("✗ Length failed")
    return 1
}
ENDTEST

run_test "Length tracking" test_len.wyn

# Test 6: Type inference through function returns
cat > test_type_inference.wyn << 'ENDTEST'
fn create_map() -> HashMap<string, int> {
    var m: HashMap<string, int> = {}
    m.insert("initial", 42)
    return m
}

fn main() -> int {
    var my_map = create_map()
    my_map.insert("key", 100)
    var val = my_map.get("key")
    
    if val == 100 {
        print("✓ Type inference works")
        return 0
    }
    
    print("✗ Type inference failed")
    return 1
}
ENDTEST

run_test "Type inference" test_type_inference.wyn

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
    echo "✅ ALL HASHMAP TESTS PASSED"
    exit 0
else
    echo "❌ SOME TESTS FAILED"
    exit 1
fi

# Test 6: Type inference through function returns
cat > test_type_inference.wyn << 'ENDTEST'
fn create_map() -> HashMap<string, int> {
    var m: HashMap<string, int> = {}
    m.insert("initial", 42)
    return m
}

fn main() -> int {
    var my_map = create_map()
    my_map.insert("key", 100)
    var val = my_map.get("key")
    
    if val == 100 {
        print("✓ Type inference works")
        return 0
    }
    
    print("✗ Type inference failed")
    return 1
}
ENDTEST

run_test "Type inference" test_type_inference.wyn
