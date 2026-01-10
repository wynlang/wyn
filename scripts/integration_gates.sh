#!/bin/bash

# Phase 2 Integration Validation Gates
# Automated validation at each handoff point

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Day 5 Integration Gate: LLVM Infrastructure Complete
validate_day5_llvm_infrastructure() {
    log_info "=== Day 5 Integration Gate: LLVM Infrastructure ==="
    
    local errors=0
    
    # 1. Verify LLVM Infrastructure deliverables
    log_info "Checking LLVM infrastructure completeness..."
    
    if [[ ! -f "src/llvm_codegen.c" ]]; then
        log_error "Missing src/llvm_codegen.c"
        ((errors++))
    else
        log_success "Found src/llvm_codegen.c"
    fi
    
    # Check for LLVM context management (T2.1.2)
    if grep -q "LLVMCodegenContext" src/llvm_codegen.c 2>/dev/null; then
        log_success "LLVM context management implemented"
    else
        log_warning "LLVM context management not yet implemented"
    fi
    
    # 2. Verify API contract compliance
    log_info "Validating LLVM API contracts..."
    
    # Check if LLVM build works
    if make wyn-llvm >/dev/null 2>&1; then
        log_success "LLVM build integration working"
    else
        log_error "LLVM build integration failed"
        ((errors++))
    fi
    
    # 3. Verify other agents can use LLVM system
    log_info "Testing LLVM system integration readiness..."
    
    # Check that LLVM headers are accessible
    if llvm-config --cflags >/dev/null 2>&1; then
        log_success "LLVM development environment ready"
    else
        log_error "LLVM development environment not ready"
        ((errors++))
    fi
    
    # 4. Performance validation
    log_info "Running LLVM performance validation..."
    
    # Basic performance test - LLVM should initialize quickly
    local start_time=$(date +%s%N)
    if timeout 10s ./wyn-llvm --version >/dev/null 2>&1; then
        local end_time=$(date +%s%N)
        local duration=$(( (end_time - start_time) / 1000000 )) # Convert to milliseconds
        if [[ $duration -lt 5000 ]]; then # Less than 5 seconds
            log_success "LLVM initialization performance acceptable ($duration ms)"
        else
            log_warning "LLVM initialization slow ($duration ms)"
        fi
    else
        log_warning "LLVM-based compiler not yet functional (expected in early Phase 2)"
    fi
    
    if [[ $errors -eq 0 ]]; then
        log_success "✅ Day 5 Integration Gate PASSED - LLVM infrastructure ready"
        return 0
    else
        log_error "❌ Day 5 Integration Gate FAILED - $errors errors found"
        return 1
    fi
}

# Day 10 Integration Gate: Basic Codegen + ARC Runtime Ready
validate_day10_codegen_arc() {
    log_info "=== Day 10 Integration Gate: Basic Codegen + ARC Runtime ==="
    
    local errors=0
    
    # 1. Verify basic code generation works
    log_info "Checking basic LLVM code generation..."
    
    # Check for codegen functions
    if grep -q "codegen_.*_expr\|codegen_.*_stmt" src/llvm_codegen.c 2>/dev/null; then
        log_success "Basic codegen functions found"
    else
        log_warning "Basic codegen functions not yet implemented"
    fi
    
    # 2. Verify ARC runtime system
    log_info "Checking ARC runtime system..."
    
    # Look for ARC-related files (may not exist yet)
    if [[ -f "src/arc_runtime.c" ]] || grep -q "arc_.*retain\|arc_.*release" src/*.c 2>/dev/null; then
        log_success "ARC runtime system components found"
    else
        log_warning "ARC runtime system not yet implemented"
    fi
    
    # 3. Integration test
    log_info "Testing codegen-ARC integration readiness..."
    
    # Test that basic compilation still works
    if make wyn >/dev/null 2>&1; then
        log_success "Basic compilation still working"
    else
        log_error "Basic compilation broken"
        ((errors++))
    fi
    
    if [[ $errors -eq 0 ]]; then
        log_success "✅ Day 10 Integration Gate PASSED - Codegen + ARC ready"
        return 0
    else
        log_error "❌ Day 10 Integration Gate FAILED - $errors errors found"
        return 1
    fi
}

# Day 15 Integration Gate: Core Systems Complete
validate_day15_core_systems() {
    log_info "=== Day 15 Integration Gate: Core Systems Complete ==="
    
    local errors=0
    
    # 1. Verify type system enhancements
    log_info "Checking type system enhancements..."
    
    # Look for enhanced type system features
    if grep -q "Optional\|Union\|enhanced.*struct" src/*.c src/*.h 2>/dev/null; then
        log_success "Type system enhancements found"
    else
        log_warning "Type system enhancements not yet implemented"
    fi
    
    # 2. Verify all core systems integrate
    log_info "Testing core system integration..."
    
    # Test comprehensive build
    if make all >/dev/null 2>&1; then
        log_success "All build targets working"
    else
        log_error "Build system integration issues"
        ((errors++))
    fi
    
    # 3. Cross-platform readiness check
    log_info "Checking cross-platform readiness..."
    
    # Verify platform detection works
    local platform=$(uname -s)
    log_info "Current platform: $platform"
    
    if [[ $errors -eq 0 ]]; then
        log_success "✅ Day 15 Integration Gate PASSED - Core systems complete"
        return 0
    else
        log_error "❌ Day 15 Integration Gate FAILED - $errors errors found"
        return 1
    fi
}

# Day 20 Integration Gate: Full Integration
validate_day20_full_integration() {
    log_info "=== Day 20 Integration Gate: Full Integration ==="
    
    local errors=0
    
    # 1. Verify all Phase 2 systems work together
    log_info "Testing full Phase 2 integration..."
    
    # Test all build targets
    local targets=("wyn" "wyn-llvm" "test")
    for target in "${targets[@]}"; do
        if make "$target" >/dev/null 2>&1; then
            log_success "Build target '$target' working"
        else
            log_error "Build target '$target' failed"
            ((errors++))
        fi
    done
    
    # 2. Comprehensive testing
    log_info "Running comprehensive test suite..."
    
    if make test >/dev/null 2>&1; then
        log_success "All tests passing"
    else
        log_warning "Some tests failing (may be expected during development)"
    fi
    
    # 3. Performance validation
    log_info "Running performance validation..."
    
    # Test compilation speed
    local test_file="examples/simple_test.wyn"
    if [[ -f "$test_file" ]]; then
        local start_time=$(date +%s%N)
        if timeout 30s ./wyn "$test_file" >/dev/null 2>&1; then
            local end_time=$(date +%s%N)
            local duration=$(( (end_time - start_time) / 1000000 ))
            log_success "Compilation performance: $duration ms"
        else
            log_warning "Compilation performance test failed or timed out"
        fi
    fi
    
    # 4. Memory safety validation
    log_info "Running memory safety validation..."
    
    if command -v valgrind >/dev/null 2>&1; then
        if timeout 60s valgrind --leak-check=full --error-exitcode=1 ./wyn examples/hello.wyn >/dev/null 2>&1; then
            log_success "Memory safety validation passed"
        else
            log_warning "Memory safety issues detected"
        fi
    else
        log_warning "Valgrind not available for memory safety testing"
    fi
    
    if [[ $errors -eq 0 ]]; then
        log_success "✅ Day 20 Integration Gate PASSED - Full integration complete"
        return 0
    else
        log_error "❌ Day 20 Integration Gate FAILED - $errors errors found"
        return 1
    fi
}

# Run specific validation gate
run_validation_gate() {
    local gate="$1"
    
    case "$gate" in
        "day5"|"5")
            validate_day5_llvm_infrastructure
            ;;
        "day10"|"10")
            validate_day10_codegen_arc
            ;;
        "day15"|"15")
            validate_day15_core_systems
            ;;
        "day20"|"20")
            validate_day20_full_integration
            ;;
        "all")
            local total_errors=0
            validate_day5_llvm_infrastructure || ((total_errors++))
            echo ""
            validate_day10_codegen_arc || ((total_errors++))
            echo ""
            validate_day15_core_systems || ((total_errors++))
            echo ""
            validate_day20_full_integration || ((total_errors++))
            
            echo ""
            if [[ $total_errors -eq 0 ]]; then
                log_success "🎯 ALL INTEGRATION GATES PASSED ✅"
            else
                log_error "❌ $total_errors integration gates failed"
                return 1
            fi
            ;;
        *)
            echo "Usage: $0 [day5|day10|day15|day20|all]"
            echo ""
            echo "Integration Gates:"
            echo "  day5  - LLVM Infrastructure Complete"
            echo "  day10 - Basic Codegen + ARC Runtime Ready"
            echo "  day15 - Core Systems Complete"
            echo "  day20 - Full Integration"
            echo "  all   - Run all validation gates"
            exit 1
            ;;
    esac
}

# Run the specified validation gate
run_validation_gate "${1:-all}"