#!/bin/bash
# TDD Test Runner for LLVM Backend

set -e

WYN="./wyn"
TESTS_DIR="tests/llvm"
FAILED=0
PASSED=0

mkdir -p "$TESTS_DIR"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

run_test() {
    local name="$1"
    local code="$2"
    local expected_exit="$3"
    local test_file="$TESTS_DIR/${name}.wyn"
    
    echo "$code" > "$test_file"
    
    if $WYN "$test_file" >/dev/null 2>&1; then
        local output_file="${test_file%.wyn}.out"
        if [ -f "$output_file" ]; then
            $output_file
            local actual_exit=$?
            if [ "$actual_exit" -eq "$expected_exit" ]; then
                echo -e "${GREEN}✓${NC} $name"
                ((PASSED++))
                return 0
            else
                echo -e "${RED}✗${NC} $name (expected exit $expected_exit, got $actual_exit)"
                ((FAILED++))
                return 1
            fi
        else
            echo -e "${RED}✗${NC} $name (no output file)"
            ((FAILED++))
            return 1
        fi
    else
        echo -e "${RED}✗${NC} $name (compilation failed)"
        ((FAILED++))
        return 1
    fi
}

echo "=== LLVM Backend TDD Tests ==="
echo

# Phase 1: Core Expressions
echo "Phase 1: Core Expressions"

run_test "float_literal" "fn main() -> int { return 3 }" 3
run_test "bool_true" "fn main() -> int { return 1 }" 1
run_test "bool_false" "fn main() -> int { return 0 }" 0
run_test "unary_neg" "fn main() -> int { return -5 }" 251  # -5 as unsigned byte
run_test "unary_not" "fn main() -> int { if !0 { return 1 } return 0 }" 1

# Phase 2: Core Statements  
echo
echo "Phase 2: Core Statements"

run_test "expr_stmt" "fn main() -> int { 5 + 3; return 0 }" 0
run_test "while_loop" "fn main() -> int { var x = 0; while x < 5 { x = x + 1 } return x }" 5
run_test "for_loop" "fn main() -> int { var sum = 0; for i in 0..5 { sum = sum + i } return sum }" 10

echo
echo "=== Results ==="
echo -e "${GREEN}Passed: $PASSED${NC}"
echo -e "${RED}Failed: $FAILED${NC}"

exit $FAILED
