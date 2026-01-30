#!/bin/bash

# Wyn Test Framework Runner
# Comprehensive test execution script

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
VERBOSE=0
RUN_UNIT=1
RUN_INTEGRATION=1
RUN_BENCHMARKS=0
RUN_MEMORY=0
RUN_COVERAGE=0
PARALLEL=0

# Directories
FRAMEWORK_DIR="framework"
UNIT_DIR="unit"
INTEGRATION_DIR="integration"
BENCHMARK_DIR="benchmarks"
MEMORY_DIR="memory"
COVERAGE_DIR="coverage_report"

print_usage() {
    echo "Wyn Test Framework Runner"
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -h, --help          Show this help message"
    echo "  -v, --verbose       Enable verbose output"
    echo "  -u, --unit-only     Run only unit tests"
    echo "  -i, --integration-only  Run only integration tests"
    echo "  -b, --benchmarks    Run performance benchmarks"
    echo "  -m, --memory        Run memory leak detection"
    echo "  -c, --coverage      Generate code coverage report"
    echo "  -a, --all           Run all test types"
    echo "  -p, --parallel      Run tests in parallel (where possible)"
    echo "  --clean             Clean all test artifacts before running"
    echo ""
    echo "Examples:"
    echo "  $0                  # Run unit and integration tests"
    echo "  $0 -a               # Run all test types"
    echo "  $0 -b -m            # Run benchmarks and memory tests"
    echo "  $0 -c --clean       # Clean and generate coverage report"
}

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

check_dependencies() {
    log_info "Checking dependencies..."
    
    # Check if make is available
    if ! command -v make &> /dev/null; then
        log_error "make is required but not installed"
        exit 1
    fi
    
    # Check if gcc is available
    if ! command -v gcc &> /dev/null; then
        log_error "gcc is required but not installed"
        exit 1
    fi
    
    # Check if Wyn compiler exists
    if [ ! -f "../wyn" ]; then
        log_warning "Wyn compiler not found at ../wyn"
        log_info "Building Wyn compiler..."
        (cd .. && make wyn)
    fi
    
    log_success "Dependencies check passed"
}

build_framework() {
    log_info "Building test framework..."
    
    if make framework; then
        log_success "Test framework built successfully"
    else
        log_error "Failed to build test framework"
        exit 1
    fi
}

run_unit_tests() {
    if [ $RUN_UNIT -eq 0 ]; then
        return 0
    fi
    
    log_info "Running unit tests..."
    
    # Build unit test executables
    for test_file in $UNIT_DIR/*.c; do
        if [ -f "$test_file" ]; then
            test_name=$(basename "$test_file" .c)
            log_info "Building unit test: $test_name"
            
            if gcc -Wall -Wextra -std=c99 -I$FRAMEWORK_DIR -o "$UNIT_DIR/$test_name" \
                   "$test_file" "$FRAMEWORK_DIR/unit_test.c"; then
                log_success "Built $test_name"
            else
                log_error "Failed to build $test_name"
                return 1
            fi
        fi
    done
    
    # Run unit tests
    if [ -x "$FRAMEWORK_DIR/test_runner" ]; then
        if $FRAMEWORK_DIR/test_runner $UNIT_DIR; then
            log_success "Unit tests completed"
            return 0
        else
            log_error "Unit tests failed"
            return 1
        fi
    else
        log_error "Test runner not found"
        return 1
    fi
}

run_integration_tests() {
    if [ $RUN_INTEGRATION -eq 0 ]; then
        return 0
    fi
    
    log_info "Running integration tests..."
    
    if [ -x "$FRAMEWORK_DIR/integration_test" ] && [ -f "$INTEGRATION_DIR/integration_tests.conf" ]; then
        cd $INTEGRATION_DIR
        if ../$FRAMEWORK_DIR/integration_test integration_tests.conf; then
            cd ..
            log_success "Integration tests completed"
            return 0
        else
            cd ..
            log_error "Integration tests failed"
            return 1
        fi
    else
        log_warning "Integration test runner or configuration not found"
        return 0
    fi
}

run_benchmarks() {
    if [ $RUN_BENCHMARKS -eq 0 ]; then
        return 0
    fi
    
    log_info "Running performance benchmarks..."
    
    if [ -x "$FRAMEWORK_DIR/benchmark" ]; then
        cd $BENCHMARK_DIR
        if ../$FRAMEWORK_DIR/benchmark -i 5 -o benchmark_results.json; then
            cd ..
            log_success "Benchmarks completed"
            return 0
        else
            cd ..
            log_error "Benchmarks failed"
            return 1
        fi
    else
        log_warning "Benchmark runner not found"
        return 0
    fi
}

run_memory_tests() {
    if [ $RUN_MEMORY -eq 0 ]; then
        return 0
    fi
    
    log_info "Running memory leak detection..."
    
    if [ -x "$FRAMEWORK_DIR/memory_test" ]; then
        cd $MEMORY_DIR
        if ../$FRAMEWORK_DIR/memory_test -o memory_report.md; then
            cd ..
            log_success "Memory tests completed"
            return 0
        else
            cd ..
            log_error "Memory tests failed"
            return 1
        fi
    else
        log_warning "Memory test runner not found"
        return 0
    fi
}

run_coverage() {
    if [ $RUN_COVERAGE -eq 0 ]; then
        return 0
    fi
    
    log_info "Generating code coverage report..."
    
    if [ -x "$FRAMEWORK_DIR/coverage" ]; then
        if $FRAMEWORK_DIR/coverage --html -o $COVERAGE_DIR; then
            log_success "Coverage report generated in $COVERAGE_DIR/"
            return 0
        else
            log_error "Coverage generation failed"
            return 1
        fi
    else
        log_warning "Coverage tool not found"
        return 0
    fi
}

clean_artifacts() {
    log_info "Cleaning test artifacts..."
    make clean
    log_success "Artifacts cleaned"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            print_usage
            exit 0
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -u|--unit-only)
            RUN_UNIT=1
            RUN_INTEGRATION=0
            shift
            ;;
        -i|--integration-only)
            RUN_UNIT=0
            RUN_INTEGRATION=1
            shift
            ;;
        -b|--benchmarks)
            RUN_BENCHMARKS=1
            shift
            ;;
        -m|--memory)
            RUN_MEMORY=1
            shift
            ;;
        -c|--coverage)
            RUN_COVERAGE=1
            shift
            ;;
        -a|--all)
            RUN_UNIT=1
            RUN_INTEGRATION=1
            RUN_BENCHMARKS=1
            RUN_MEMORY=1
            RUN_COVERAGE=1
            shift
            ;;
        -p|--parallel)
            PARALLEL=1
            shift
            ;;
        --clean)
            clean_artifacts
            shift
            ;;
        *)
            log_error "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

# Main execution
main() {
    log_info "Starting Wyn Test Framework Runner"
    
    check_dependencies
    build_framework
    
    local exit_code=0
    
    # Run tests
    if ! run_unit_tests; then
        exit_code=1
    fi
    
    if ! run_integration_tests; then
        exit_code=1
    fi
    
    if ! run_benchmarks; then
        exit_code=1
    fi
    
    if ! run_memory_tests; then
        exit_code=1
    fi
    
    if ! run_coverage; then
        exit_code=1
    fi
    
    # Summary
    echo ""
    log_info "Test execution summary:"
    echo "  Unit Tests: $([ $RUN_UNIT -eq 1 ] && echo "✓" || echo "○")"
    echo "  Integration Tests: $([ $RUN_INTEGRATION -eq 1 ] && echo "✓" || echo "○")"
    echo "  Benchmarks: $([ $RUN_BENCHMARKS -eq 1 ] && echo "✓" || echo "○")"
    echo "  Memory Tests: $([ $RUN_MEMORY -eq 1 ] && echo "✓" || echo "○")"
    echo "  Coverage: $([ $RUN_COVERAGE -eq 1 ] && echo "✓" || echo "○")"
    
    if [ $exit_code -eq 0 ]; then
        log_success "All tests completed successfully!"
    else
        log_error "Some tests failed. Check the output above for details."
    fi
    
    exit $exit_code
}

# Run main function
main