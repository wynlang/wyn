#!/bin/bash
set -e

echo "=== Testing Package Registry Commands ==="
echo ""

echo "Test 1: Search command exists"
./wyn search 2>&1 | grep -q "Usage: wyn search" && echo "✅ PASS" || echo "❌ FAIL"

echo "Test 2: Info command exists"
./wyn info 2>&1 | grep -q "Usage: wyn info" && echo "✅ PASS" || echo "❌ FAIL"

echo "Test 3: Versions command exists"
./wyn versions 2>&1 | grep -q "Usage: wyn versions" && echo "✅ PASS" || echo "❌ FAIL"

echo "Test 4: Help shows registry commands"
./wyn --help 2>&1 | grep -q "wyn search" && echo "✅ PASS" || echo "❌ FAIL"

echo "Test 5: Commands handle network errors gracefully"
./wyn search test 2>&1 | grep -q "Error:" && echo "✅ PASS" || echo "❌ FAIL"

echo ""
echo "=== All Registry Tests Complete ==="
