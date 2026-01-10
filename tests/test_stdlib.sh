#!/bin/bash

# Standard Library Tests (Phase 1 subset)
echo "=== Standard Library Tests ==="
echo "Note: Phase 1 includes basic string operations only"
echo

# Test basic string functionality through integration
echo "Testing string operations via integration tests..."
if ./tests/test_basic_string_operations > /dev/null 2>&1; then
    echo "✅ Basic string operations working"
else
    echo "❌ Basic string operations failed"
    exit 1
fi

if ./tests/test_string_interpolation > /dev/null 2>&1; then
    echo "✅ String interpolation working"
else
    echo "❌ String interpolation failed"
    exit 1
fi

if ./tests/test_string_methods > /dev/null 2>&1; then
    echo "✅ String methods working"
else
    echo "❌ String methods failed"
    exit 1
fi

echo
echo "✅ Phase 1 Standard Library Tests PASSED"
echo "Note: Full stdlib will be implemented in Phase 4"
