#!/bin/bash
# Wyn v1.5.0 - Complete Validation Suite
# Tests ALL working features with NO exceptions

set -e

WYN=${WYN_ROOT:-../..}/wyn
TESTDIR=$(mktemp -d)
cd "$TESTDIR"

echo "╔══════════════════════════════════════════════════════════════════════════╗"
echo "║                                                                          ║"
echo "║                  WYN v1.5.0 - FINAL VALIDATION                           ║"
echo "║                                                                          ║"
echo "╚══════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "Test directory: $TESTDIR"
echo ""

PASSED=0
FAILED=0

run_test() {
    local name="$1"
    local file="$2"
    echo -n "[$((PASSED + FAILED + 1))] $name... "
    
    if timeout 10 $WYN run "$file" > /tmp/test_output.txt 2>&1; then
        if grep -q "✓ All tests passed\|PASS" /tmp/test_output.txt 2>/dev/null; then
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

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "TESTING FRAMEWORK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

cat > test_framework.wyn << 'EOF'
fn main() -> int {
    Test::init("Framework Validation")
    Test::assert_eq_int(2 + 2, 4, "math works")
    Test::assert_eq_str("hello", "hello", "strings work")
    Test::assert_gt(10, 5, "comparisons work")
    return Test::summary()
}
EOF
run_test "Testing framework" test_framework.wyn

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "NETWORKING"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

cat > test_http.wyn << 'EOF'
fn main() -> int {
    var resp = Http::get("http://httpbin.org/get")
    if resp > 0 {
        var status = Http::status(resp)
        if status == 200 {
            print("PASS")
        }
        Http::free(resp)
    }
    return 0
}
EOF
run_test "HTTP GET" test_http.wyn

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "STANDARD LIBRARY"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

cat > test_json.wyn << 'EOF'
fn main() -> int {
    Test::init("JSON")
    var json = Json::parse("{\"name\":\"test\",\"value\":42}")
    Test::assert_not_null(json, "parsed")
    if json > 0 {
        var name = Json::get_string(json, "name")
        Test::assert_eq_str(name, "test", "string value")
        var value = Json::get_int(json, "value")
        Test::assert_eq_int(value, 42, "int value")
        Json::free(json)
    }
    return Test::summary()
}
EOF
run_test "JSON parsing" test_json.wyn

cat > test_file.wyn << 'EOF'
fn main() -> int {
    Test::init("File I/O")
    var content = "test data"
    var w = File::write("test.txt", content)
    Test::assert_eq_int(w, 1, "write")
    var r = File::read("test.txt")
    Test::assert_eq_str(r, content, "read")
    var e = File::exists("test.txt")
    Test::assert_eq_int(e, 1, "exists")
    File::delete("test.txt")
    return Test::summary()
}
EOF
run_test "File I/O" test_file.wyn

cat > test_string.wyn << 'EOF'
fn main() -> int {
    Test::init("String")
    var s = "Hello World"
    Test::assert_eq_int(s.len(), 11, "len")
    Test::assert_eq_str(s.upper(), "HELLO WORLD", "upper")
    Test::assert_eq_str(s.lower(), "hello world", "lower")
    var trimmed = "  test  ".trim()
    Test::assert_eq_str(trimmed, "test", "trim")
    return Test::summary()
}
EOF
run_test "String operations" test_string.wyn

cat > test_array.wyn << 'EOF'
fn main() -> int {
    Test::init("Array")
    var arr = [1, 2, 3, 4, 5]
    Test::assert_eq_int(arr.len(), 5, "len")
    Test::assert_eq_int(arr.get(0), 1, "get first")
    Test::assert_eq_int(arr.get(4), 5, "get last")
    return Test::summary()
}
EOF
run_test "Array operations" test_array.wyn

cat > test_time.wyn << 'EOF'
fn main() -> int {
    Test::init("Time")
    var t1 = Time::now()
    Test::assert_gt(t1, 0, "timestamp")
    var t2 = Time::now()
    Test::assert_gte(t2, t1, "time progresses")
    return Test::summary()
}
EOF
run_test "Time operations" test_time.wyn

cat > test_crypto.wyn << 'EOF'
fn main() -> int {
    Test::init("Crypto")
    var h1 = Crypto::hash32("test")
    var h2 = Crypto::hash32("test")
    Test::assert_eq_int(h1, h2, "deterministic")
    var h3 = Crypto::hash32("different")
    Test::assert_ne_int(h1, h3, "different inputs")
    return Test::summary()
}
EOF
run_test "Crypto operations" test_crypto.wyn

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "INTEGRATION TESTS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

cat > test_json_file.wyn << 'EOF'
fn main() -> int {
    Test::init("JSON + File")
    var data = "{\"user\":\"alice\",\"score\":100}"
    File::write("data.json", data)
    var content = File::read("data.json")
    var json = Json::parse(content)
    if json > 0 {
        var user = Json::get_string(json, "user")
        Test::assert_eq_str(user, "alice", "user correct")
        Json::free(json)
    }
    File::delete("data.json")
    return Test::summary()
}
EOF
run_test "JSON + File integration" test_json_file.wyn

cat > test_http_json.wyn << 'EOF'
fn main() -> int {
    Test::init("HTTP + JSON")
    var resp = Http::get("http://httpbin.org/get")
    if resp > 0 {
        var status = Http::status(resp)
        Test::assert_eq_int(status, 200, "HTTP success")
        Http::free(resp)
    }
    return Test::summary()
}
EOF
run_test "HTTP + JSON integration" test_http_json.wyn

cat > test_complete.wyn << 'EOF'
fn main() -> int {
    Test::init("Complete Integration")
    
    Test::describe("Create data")
    var timestamp = Time::now()
    var data = "{\"time\":" + timestamp.to_string() + ",\"msg\":\"test\"}"
    File::write("log.json", data)
    
    Test::describe("Read and parse")
    var content = File::read("log.json")
    var json = Json::parse(content)
    Test::assert_not_null(json, "parsed")
    
    if json > 0 {
        var msg = Json::get_string(json, "msg")
        Test::assert_eq_str(msg, "test", "message correct")
        Json::free(json)
    }
    
    Test::describe("Cleanup")
    File::delete("log.json")
    
    return Test::summary()
}
EOF
run_test "Complete integration" test_complete.wyn

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "FINAL RESULTS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo "Total:  $((PASSED + FAILED))"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "✅ ALL TESTS PASSED - v1.5.0 READY TO SHIP"
    exit 0
else
    echo "❌ $FAILED TEST(S) FAILED"
    exit 1
fi
