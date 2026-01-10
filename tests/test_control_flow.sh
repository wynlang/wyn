#!/bin/bash

# Control Flow Tests
echo "=== Control Flow Tests ==="

# Test control flow features
echo "Testing control flow features..."
if ./tests/test_while_loop_ast > /dev/null 2>&1; then
    echo "✅ While loops working"
else
    echo "❌ While loops failed"
    exit 1
fi

if ./tests/test_break_continue > /dev/null 2>&1; then
    echo "✅ Break/continue working"
else
    echo "❌ Break/continue failed"
    exit 1
fi

if ./tests/test_match_statement > /dev/null 2>&1; then
    echo "✅ Match statements working"
else
    echo "❌ Match statements failed"
    exit 1
fi

if ./tests/test_control_flow_codegen > /dev/null 2>&1; then
    echo "✅ Control flow codegen working"
else
    echo "❌ Control flow codegen failed"
    exit 1
fi

echo
echo "✅ Control Flow Tests PASSED"
