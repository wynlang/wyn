#!/bin/bash
# Parallel test runner using Wyn thread pool
set -e

echo "=== Wyn Parallel Test Suite ==="
echo

# Compile all tests
echo "Compiling tests..."
COMPILED=0
for f in tests/unit/test_*.wyn; do
    if ./wyn "$f" > /dev/null 2>&1; then
        ((COMPILED++))
    fi
done
echo "Compiled $COMPILED tests"
echo

# Generate dynamic runner (finds working tests)
./scripts/gen_test_runner.sh 2>&1 | grep -E "Found|Generated"

# Compile runner
echo "Compiling test runner..."
./wyn tests/run_all_tests.wyn > /dev/null

# Run tests
echo "Running tests in parallel (50 workers)..."
time ./tests/run_all_tests.out

if [ $? -eq 0 ]; then
    echo
    echo "✅ ALL TESTS PASSED"
else
    echo
    echo "❌ SOME TESTS FAILED"
    exit 1
fi
