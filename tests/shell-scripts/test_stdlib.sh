#!/bin/bash
# Comprehensive Standard Library Validation (TDD)

set -e

WYN=${WYN_ROOT:-../..}/wyn
TESTDIR=$(mktemp -d)
cd "$TESTDIR"

echo "=== Standard Library Validation (TDD) ==="
echo "Test directory: $TESTDIR"
echo

PASSED=0
FAILED=0

run_test() {
    local name="$1"
    local file="$2"
    echo -n "Test $((PASSED + FAILED + 1)): $name... "
    
    if timeout 10 $WYN run "$file" > /tmp/test_output.txt 2>&1; then
        if grep -q "✓ All tests passed" /tmp/test_output.txt 2>/dev/null; then
            echo "✓ PASS"
            PASSED=$((PASSED + 1))
            return 0
        fi
    fi
    
    echo "✗ FAIL"
    cat /tmp/test_output.txt | tail -15
    FAILED=$((FAILED + 1))
    return 1
}

# Test 1: JSON parsing
cat > test1.wyn << 'EOF'
fn main() -> int {
    Test::init("JSON Tests")
    
    var json = Json::parse("{\"name\":\"Alice\",\"age\":25}")
    Test::assert_not_null(json, "JSON parsed")
    
    if json > 0 {
        var name = Json::get_string(json, "name")
        Test::assert_eq_str(name, "Alice", "name correct")
        
        var age = Json::get_int(json, "age")
        Test::assert_eq_int(age, 25, "age correct")
        
        Json::free(json)
    }
    
    return Test::summary()
}
EOF
run_test "JSON parsing" test1.wyn

# Test 2: File I/O
cat > test2.wyn << 'EOF'
fn main() -> int {
    Test::init("File I/O Tests")
    
    var content = "Test content"
    var w = File::write("test.txt", content)
    Test::assert_eq_int(w, 1, "file written")
    
    var r = File::read("test.txt")
    Test::assert_eq_str(r, content, "content matches")
    
    var e = File::exists("test.txt")
    Test::assert_eq_int(e, 1, "file exists")
    
    var d = File::delete("test.txt")
    Test::assert_eq_int(d, 1, "file deleted")
    
    return Test::summary()
}
EOF
run_test "File I/O" test2.wyn

# Test 3: String operations
cat > test3.wyn << 'EOF'
fn main() -> int {
    Test::init("String Tests")
    
    var s = "hello world"
    var len = s.len()
    Test::assert_eq_int(len, 11, "length correct")
    
    var upper = s.upper()
    Test::assert_eq_str(upper, "HELLO WORLD", "uppercase works")
    
    var contains = s.contains("world")
    Test::assert_eq_int(contains, 1, "contains works")
    
    return Test::summary()
}
EOF
run_test "String operations" test3.wyn

# Test 4: Array operations
cat > test4.wyn << 'EOF'
fn main() -> int {
    Test::init("Array Tests")
    
    var arr = [1, 2, 3, 4, 5]
    Test::assert_eq_int(arr.len(), 5, "length correct")
    
    var first = arr.get(0)
    Test::assert_eq_int(first, 1, "first element")
    
    var last = arr.get(4)
    Test::assert_eq_int(last, 5, "last element")
    
    return Test::summary()
}
EOF
run_test "Array operations" test4.wyn

# Test 5: Time operations
cat > test5.wyn << 'EOF'
fn main() -> int {
    Test::init("Time Tests")
    
    var now = Time::now()
    Test::assert_gt(now, 0, "timestamp positive")
    
    var now2 = Time::now()
    Test::assert_gte(now2, now, "time progresses")
    
    return Test::summary()
}
EOF
run_test "Time operations" test5.wyn

# Test 6: Crypto operations
cat > test6.wyn << 'EOF'
fn main() -> int {
    Test::init("Crypto Tests")
    
    var data = "hello"
    var hash = Crypto::hash32(data)
    Test::assert_gt(hash, 0, "hash generated")
    
    var hash2 = Crypto::hash32(data)
    Test::assert_eq_int(hash, hash2, "hash deterministic")
    
    return Test::summary()
}
EOF
run_test "Crypto operations" test6.wyn

# Test 7: JSON + File integration
cat > test7.wyn << 'EOF'
fn main() -> int {
    Test::init("JSON + File Integration")
    
    var json_str = "{\"user\":\"Bob\",\"score\":100}"
    var w = File::write("data.json", json_str)
    Test::assert_eq_int(w, 1, "JSON written to file")
    
    var content = File::read("data.json")
    var json = Json::parse(content)
    Test::assert_not_null(json, "JSON parsed from file")
    
    if json > 0 {
        var user = Json::get_string(json, "user")
        Test::assert_eq_str(user, "Bob", "user correct")
        
        var score = Json::get_int(json, "score")
        Test::assert_eq_int(score, 100, "score correct")
        
        Json::free(json)
    }
    
    File::delete("data.json")
    
    return Test::summary()
}
EOF
run_test "JSON + File integration" test7.wyn

# Test 8: String + Array integration
cat > test8.wyn << 'EOF'
fn main() -> int {
    Test::init("String + Array Integration")
    
    var words = ["hello", "world", "test"]
    Test::assert_eq_int(words.len(), 3, "array length")
    
    var first = words.get(0)
    Test::assert_eq_str(first, "hello", "first word")
    
    var second = words.get(1)
    Test::assert_eq_str(second, "world", "second word")
    
    return Test::summary()
}
EOF
run_test "String + Array integration" test8.wyn

# Test 9: Time + File integration
cat > test9.wyn << 'EOF'
fn main() -> int {
    Test::init("Time + File Integration")
    
    var start = Time::now()
    File::write("timestamp.txt", "data")
    var end = Time::now()
    
    Test::assert_gte(end, start, "time progresses")
    
    var exists = File::exists("timestamp.txt")
    Test::assert_eq_int(exists, 1, "file created")
    
    File::delete("timestamp.txt")
    
    return Test::summary()
}
EOF
run_test "Time + File integration" test9.wyn

# Test 10: Complete integration
cat > test10.wyn << 'EOF'
fn main() -> int {
    Test::init("Complete Integration")
    
    Test::describe("Create JSON data")
    var json_str = "{\"timestamp\":" + Time::now().to_string() + ",\"message\":\"test\"}"
    var w = File::write("log.json", json_str)
    Test::assert_eq_int(w, 1, "log written")
    
    Test::describe("Read and parse")
    var content = File::read("log.json")
    var json = Json::parse(content)
    Test::assert_not_null(json, "log parsed")
    
    if json > 0 {
        var msg = Json::get_string(json, "message")
        Test::assert_eq_str(msg, "test", "message correct")
        Json::free(json)
    }
    
    Test::describe("Cleanup")
    var d = File::delete("log.json")
    Test::assert_eq_int(d, 1, "log deleted")
    
    return Test::summary()
}
EOF
run_test "Complete integration" test10.wyn

echo
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo "Total:  $((PASSED + FAILED))"
echo

if [ $FAILED -eq 0 ]; then
    echo "✓ All tests passed!"
    exit 0
else
    echo "✗ $FAILED test(s) failed"
    exit 1
fi
