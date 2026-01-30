#!/bin/bash
# Test LSP server functionality

set -e

echo "=== Testing LSP Server ==="

# Test 1: Server starts
echo "Test 1: Server starts"
timeout 1 ./wyn lsp 2>&1 | grep -q "Wyn Language Server starting" && echo "✅ PASS" || echo "❌ FAIL"

# Test 2: Initialize request
echo "Test 2: Initialize request"
INIT_MSG='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"capabilities":{}}}'
INIT_LEN=${#INIT_MSG}
(printf "Content-Length: %d\r\n\r\n%s" $INIT_LEN "$INIT_MSG"; sleep 0.5) | timeout 2 ./wyn lsp 2>/dev/null | grep -q "capabilities" && echo "✅ PASS" || echo "❌ FAIL"

# Test 3: Hover request
echo "Test 3: Hover request"
HOVER_MSG='{"jsonrpc":"2.0","id":2,"method":"textDocument/hover","params":{"textDocument":{"uri":"file:///test.wyn"},"position":{"line":0,"character":0}}}'
HOVER_LEN=${#HOVER_MSG}
(printf "Content-Length: %d\r\n\r\n%s" $HOVER_LEN "$HOVER_MSG"; sleep 0.5) | timeout 2 ./wyn lsp 2>/dev/null | grep -q "contents" && echo "✅ PASS" || echo "❌ FAIL"

# Test 4: Completion request
echo "Test 4: Completion request"
COMP_MSG='{"jsonrpc":"2.0","id":3,"method":"textDocument/completion","params":{"textDocument":{"uri":"file:///test.wyn"},"position":{"line":0,"character":0}}}'
COMP_LEN=${#COMP_MSG}
(printf "Content-Length: %d\r\n\r\n%s" $COMP_LEN "$COMP_MSG"; sleep 0.5) | timeout 2 ./wyn lsp 2>/dev/null | grep -q "label" && echo "✅ PASS" || echo "❌ FAIL"

# Test 5: Definition request
echo "Test 5: Definition request"
DEF_MSG='{"jsonrpc":"2.0","id":4,"method":"textDocument/definition","params":{"textDocument":{"uri":"file:///test.wyn"},"position":{"line":0,"character":0}}}'
DEF_LEN=${#DEF_MSG}
(printf "Content-Length: %d\r\n\r\n%s" $DEF_LEN "$DEF_MSG"; sleep 0.5) | timeout 2 ./wyn lsp 2>/dev/null | grep -q "jsonrpc" && echo "✅ PASS" || echo "❌ FAIL"

# Test 6: Shutdown request
echo "Test 6: Shutdown request"
SHUT_MSG='{"jsonrpc":"2.0","id":5,"method":"shutdown","params":null}'
SHUT_LEN=${#SHUT_MSG}
(printf "Content-Length: %d\r\n\r\n%s" $SHUT_LEN "$SHUT_MSG"; sleep 0.5) | timeout 2 ./wyn lsp 2>/dev/null | grep -q "jsonrpc" && echo "✅ PASS" || echo "❌ FAIL"

echo ""
echo "=== LSP Test Summary ==="
echo "Basic LSP server exists but needs enhancement for full functionality"
