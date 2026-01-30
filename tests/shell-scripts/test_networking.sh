#!/bin/bash
# Comprehensive networking validation tests

set -e

WYN_BIN="$(cd "$(dirname "$0")/.." && pwd)/wyn"
WYN_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export WYN_ROOT

echo "=== Wyn Networking Validation ==="
echo ""

# Test 1: TCP Client Connection
echo "Test 1: TCP Client Connection"
cd /tmp
cat > test_tcp_client.wyn << 'EOF'
fn main() -> int {
    var sock = Net::connect("1.1.1.1", 80)
    if sock >= 0 {
        print("✓ TCP connection successful")
        Net::close(sock)
        return 0
    }
    print("✗ TCP connection failed")
    return 1
}
EOF

timeout 5 $WYN_BIN run test_tcp_client.wyn > test_output.txt 2>&1
if grep -q "✓" test_output.txt; then
    echo "  ✅ TCP client works"
else
    echo "  ❌ TCP client failed"
    cat test_output.txt
    exit 1
fi
cd - > /dev/null
echo ""

# Test 2: HTTP Request
echo "Test 2: HTTP Request"
cat > /tmp/test_http.wyn << 'EOF'
fn main() -> int {
    var sock = Net::connect("1.1.1.1", 80)
    if sock >= 0 {
        Net::send(sock, "GET / HTTP/1.0\r\nHost: 1.1.1.1\r\n\r\n")
        var response = Net::recv(sock)
        Net::close(sock)
        if response != "" {
            print("✓ HTTP request successful")
            return 0
        }
    }
    print("✗ HTTP request failed")
    return 1
}
EOF

timeout 5 $WYN_BIN run /tmp/test_http.wyn 2>&1 | grep -q "✓" && echo "  ✅ HTTP request works" || (echo "  ❌ HTTP request failed" && exit 1)
echo ""

# Test 3: TCP Server (basic)
echo "Test 3: TCP Server"
cat > /tmp/test_tcp_server.wyn << 'EOF'
fn main() -> int {
    var server = Net::listen(9999)
    if server >= 0 {
        print("✓ TCP server listening")
        Net::close(server)
        return 0
    }
    print("✗ TCP server failed")
    return 1
}
EOF

timeout 5 $WYN_BIN run /tmp/test_tcp_server.wyn 2>&1 | grep -q "✓" && echo "  ✅ TCP server works" || (echo "  ❌ TCP server failed" && exit 1)
echo ""

# Test 4: Send/Recv
echo "Test 4: Send/Recv Operations"
cat > /tmp/test_send_recv.wyn << 'EOF'
fn main() -> int {
    var sock = Net::connect("1.1.1.1", 80)
    if sock >= 0 {
        var sent = Net::send(sock, "GET / HTTP/1.0\r\n\r\n")
        if sent > 0 {
            print("✓ Send successful")
            var data = Net::recv(sock)
            if data != "" {
                print("✓ Recv successful")
                Net::close(sock)
                return 0
            }
        }
    }
    print("✗ Send/Recv failed")
    return 1
}
EOF

timeout 5 $WYN_BIN run /tmp/test_send_recv.wyn 2>&1 | grep -q "✓ Recv successful" && echo "  ✅ Send/Recv works" || (echo "  ❌ Send/Recv failed" && exit 1)
echo ""

# Test 5: Connection Error Handling
echo "Test 5: Error Handling"
cat > /tmp/test_error.wyn << 'EOF'
fn main() -> int {
    var sock = Net::connect("192.0.2.1", 9999)
    if sock < 0 {
        print("✓ Error handling works")
        return 0
    }
    print("✗ Should have failed")
    return 1
}
EOF

timeout 5 $WYN_BIN run /tmp/test_error.wyn 2>&1 | grep -q "✓" && echo "  ✅ Error handling works" || (echo "  ❌ Error handling failed" && exit 1)
echo ""

echo "==================================="
echo "✅ All networking tests passed!"
echo "==================================="
echo ""
echo "Summary:"
echo "  ✅ TCP client connections"
echo "  ✅ HTTP requests"
echo "  ✅ TCP server listening"
echo "  ✅ Send/Recv operations"
echo "  ✅ Error handling"
echo ""
echo "Wyn Networking: FULLY FUNCTIONAL ✅"
