#!/bin/bash
# TDD Test Runner for new stdlib functions

cd "$(dirname "$0")"

echo "╔══════════════════════════════════════════════════════════════════════════╗"
echo "║                                                                          ║"
echo "║                  STDLIB TDD TEST SUITE                                   ║"
echo "║                                                                          ║"
echo "╚══════════════════════════════════════════════════════════════════════════╝"
echo ""

# Compile stdlib C files
echo "Compiling stdlib implementations..."
cd ../src

gcc -c -fPIC stdlib_process.c -o stdlib_process.o 2>&1 | head -5
gcc -c -fPIC stdlib_fs.c -o stdlib_fs.o 2>&1 | head -5
gcc -c -fPIC stdlib_time.c -o stdlib_time.o 2>&1 | head -5

if [ $? -eq 0 ]; then
    echo "✓ Stdlib compiled"
else
    echo "✗ Compilation failed"
    exit 1
fi

cd ../tests
echo ""

# Test files
TESTS=(
    "test_process_api.wyn"
    "test_fs_api.wyn"
    "test_time_api.wyn"
    "test_task_api.wyn"
)

PASSED=0
FAILED=0

for test in "${TESTS[@]}"; do
    if [ ! -f "$test" ]; then
        echo "⊘ $test (not found)"
        continue
    fi
    
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "Running: $test"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    
    # Try to compile and run
    if timeout 10 ../wyn run "$test" 2>&1; then
        PASSED=$((PASSED + 1))
        echo ""
    else
        FAILED=$((FAILED + 1))
        echo "✗ Test failed or timed out"
        echo ""
    fi
done

echo "╔══════════════════════════════════════════════════════════════════════════╗"
echo "║                          RESULTS                                         ║"
echo "╚══════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "✅ ALL TESTS PASSED"
    exit 0
else
    echo "❌ SOME TESTS FAILED"
    echo ""
    echo "Note: These are TDD tests for new stdlib functions."
    echo "If tests fail, the functions need to be added to the compiler."
    echo ""
    echo "Next steps:"
    echo "1. Add function declarations to checker.c"
    echo "2. Add function implementations to codegen.c"
    echo "3. Link stdlib_*.o files in Makefile"
    exit 1
fi
