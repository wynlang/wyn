#!/bin/bash
set -e

echo "Testing wyn install command..."

# Test 1: Install without arguments (should try to read wyn.toml)
echo "Test 1: Install without arguments"
./wyn install 2>&1 | grep -q "Could not read wyn.toml" && echo "✓ Test 1 passed" || echo "✗ Test 1 failed"

# Test 2: Install with package name (should try registry)
echo "Test 2: Install with package name"
./wyn install http 2>&1 | grep -q "Couldn't resolve host name" && echo "✓ Test 2 passed" || echo "✗ Test 2 failed"

# Test 3: Install with version
echo "Test 3: Install with version"
./wyn install http@1.0.0 2>&1 | grep -q "Couldn't resolve host name" && echo "✓ Test 3 passed" || echo "✗ Test 3 failed"

# Test 4: Help shows install command
echo "Test 4: Help shows install command"
./wyn --help 2>&1 | grep -q "wyn install" && echo "✓ Test 4 passed" || echo "✗ Test 4 failed"

echo "All tests completed!"
