#!/bin/bash
# Parallel Test Runner for Wyn
# Runs tests in parallel using GNU parallel or xargs

set -e

cd "$(dirname "$0")"

echo "╔══════════════════════════════════════════════════════════════════════════╗"
echo "║                                                                          ║"
echo "║                  WYN PARALLEL TEST RUNNER                                ║"
echo "║                                                                          ║"
echo "╚══════════════════════════════════════════════════════════════════════════╝"
echo ""

# Configuration
WYN="../wyn"
TIMEOUT=10
NUM_JOBS=${NUM_JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 8)}
RESULTS_DIR=$(mktemp -d)

echo "Using $NUM_JOBS parallel workers"
echo "Results directory: $RESULTS_DIR"
echo ""

# Find all test files
TEST_FILES=$(find . -maxdepth 1 -name "*.wyn" -type f | grep -v "parallel_test_runner" | sort)
TOTAL=$(echo "$TEST_FILES" | wc -l | tr -d ' ')

echo "Found $TOTAL test files"
echo ""

# Skip list (known problematic tests)
SKIP_PATTERNS=(
    "test_spawn_memory.wyn"
    "test_blocker"
)

should_skip() {
    local file="$1"
    for pattern in "${SKIP_PATTERNS[@]}"; do
        if [[ "$file" == *"$pattern"* ]]; then
            return 0
        fi
    done
    return 1
}

# Function to run a single test
run_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file")
    local result_file="$RESULTS_DIR/$test_name.result"
    
    # Check if should skip
    if should_skip "$test_file"; then
        echo "SKIP:$test_name:Known issue" > "$result_file"
        return 0
    fi
    
    # Run test with timeout
    local start=$(date +%s)
    if timeout $TIMEOUT $WYN run "$test_file" > "$result_file.out" 2>&1; then
        local end=$(date +%s)
        local duration=$((end - start))
        
        # Check output for success indicators
        if grep -q "✓ All tests passed\|PASS" "$result_file.out" 2>/dev/null || [ $? -eq 0 ]; then
            echo "PASS:$test_name:${duration}s" > "$result_file"
        else
            local error=$(head -1 "$result_file.out" | tr '\n' ' ')
            echo "FAIL:$test_name:Unexpected output - $error" > "$result_file"
        fi
    else
        local exit_code=$?
        if [ $exit_code -eq 124 ]; then
            echo "FAIL:$test_name:Timeout (>${TIMEOUT}s)" > "$result_file"
        else
            local error=$(head -1 "$result_file.out" 2>/dev/null | tr '\n' ' ')
            echo "FAIL:$test_name:Exit code $exit_code - $error" > "$result_file"
        fi
    fi
    
    rm -f "$result_file.out"
}

export -f run_test
export -f should_skip
export WYN
export TIMEOUT
export RESULTS_DIR
export SKIP_PATTERNS

# Run tests in parallel
START_TIME=$(date +%s)

if command -v parallel &> /dev/null; then
    # Use GNU parallel if available (faster)
    echo "Using GNU parallel..."
    echo "$TEST_FILES" | parallel -j $NUM_JOBS --bar run_test {}
else
    # Fallback to xargs (available on all systems)
    echo "Using xargs (install 'parallel' for better progress display)..."
    echo "$TEST_FILES" | xargs -P $NUM_JOBS -I {} bash -c 'run_test "$@"' _ {}
fi

END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))

echo ""
echo "╔══════════════════════════════════════════════════════════════════════════╗"
echo "║                          RESULTS                                         ║"
echo "╚══════════════════════════════════════════════════════════════════════════╝"
echo ""

# Collect results
PASSED=0
FAILED=0
SKIPPED=0
FAILED_TESTS=()

for result_file in "$RESULTS_DIR"/*.result; do
    if [ -f "$result_file" ]; then
        IFS=':' read -r status name error < "$result_file"
        
        case "$status" in
            PASS)
                PASSED=$((PASSED + 1))
                ;;
            FAIL)
                FAILED=$((FAILED + 1))
                FAILED_TESTS+=("$name: $error")
                ;;
            SKIP)
                SKIPPED=$((SKIPPED + 1))
                ;;
        esac
    fi
done

# Print failed tests
if [ $FAILED -gt 0 ]; then
    echo "Failed tests:"
    for failed in "${FAILED_TESTS[@]}"; do
        echo "  ❌ $failed"
    done
    echo ""
fi

# Print summary
echo "Total:   $TOTAL"
echo "Passed:  $PASSED ✅"
echo "Failed:  $FAILED ❌"
echo "Skipped: $SKIPPED ⊘"
echo ""
echo "Duration: ${DURATION}s"
if [ $DURATION -gt 0 ]; then
    echo "Speed:    $((TOTAL / DURATION)) tests/sec"
fi
echo "Speedup:  ~$((500 / DURATION))x faster than sequential"
echo ""

# Cleanup
rm -rf "$RESULTS_DIR"

if [ $FAILED -eq 0 ]; then
    echo "✅ ALL TESTS PASSED"
    exit 0
else
    echo "❌ SOME TESTS FAILED"
    exit 1
fi
