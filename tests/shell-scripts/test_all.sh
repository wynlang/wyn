#!/bin/bash
# Master test runner for all Wyn tests

set -e

cd "$(dirname "$0")"  # Change to tests directory

echo "╔══════════════════════════════════════════════════════════════════════════╗"
echo "║                                                                          ║"
echo "║                     WYN COMPREHENSIVE TEST SUITE                         ║"
echo "║                                                                          ║"
echo "╚══════════════════════════════════════════════════════════════════════════╝"
echo ""

TOTAL_PASSED=0
TOTAL_FAILED=0

run_suite() {
    local name="$1"
    local script="$2"
    
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "$name"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    
    if ./"$script" > /tmp/suite_output.txt 2>&1; then
        # Extract pass/fail counts
        local passed=$(grep "^Passed:" /tmp/suite_output.txt | awk '{print $2}')
        local failed=$(grep "^Failed:" /tmp/suite_output.txt | awk '{print $2}')
        
        # Default to 0 if empty
        passed=${passed:-0}
        failed=${failed:-0}
        
        TOTAL_PASSED=$((TOTAL_PASSED + passed))
        TOTAL_FAILED=$((TOTAL_FAILED + failed))
        
        cat /tmp/suite_output.txt
        echo ""
    else
        echo "❌ Suite failed to run"
        TOTAL_FAILED=$((TOTAL_FAILED + 1))
        echo ""
    fi
}

# Run all test suites
run_suite "V1.5.0 CORE TESTS" "test_v1_5_0_final.sh"
run_suite "ENUM TESTS" "test_enums.sh"
run_suite "HASHMAP TESTS" "test_hashmap_comprehensive.sh"
run_suite "STRING/ARRAY TESTS" "test_string_array_methods.sh"

echo "╔══════════════════════════════════════════════════════════════════════════╗"
echo "║                                                                          ║"
echo "║                          FINAL RESULTS                                   ║"
echo "║                                                                          ║"
echo "╚══════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "Total Passed: $TOTAL_PASSED"
echo "Total Failed: $TOTAL_FAILED"
echo "Total Tests:  $((TOTAL_PASSED + TOTAL_FAILED))"
echo ""

if [ $TOTAL_FAILED -eq 0 ]; then
    echo "✅ ALL TESTS PASSED"
    exit 0
else
    echo "❌ SOME TESTS FAILED"
    exit 1
fi
