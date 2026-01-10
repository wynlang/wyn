#!/bin/bash

# Error Handling Tests
echo "=== Error Handling Tests ==="

# Test error system integration
echo "Testing error system..."
if ./tests/test_error_integration > /dev/null 2>&1; then
    echo "✅ Error integration working"
else
    echo "❌ Error integration failed"
    exit 1
fi

if ./tests/test_error_recovery > /dev/null 2>&1; then
    echo "✅ Error recovery working"
else
    echo "❌ Error recovery failed"
    exit 1
fi

if ./tests/test_type_checker_errors > /dev/null 2>&1; then
    echo "✅ Type checker errors working"
else
    echo "❌ Type checker errors failed"
    exit 1
fi

echo
echo "✅ Error Handling Tests PASSED"
