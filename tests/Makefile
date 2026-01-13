CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
DEBUG_FLAGS = -g -DDEBUG
COVERAGE_FLAGS = --coverage -fprofile-arcs -ftest-coverage

# Directories
FRAMEWORK_DIR = framework
UNIT_DIR = unit
INTEGRATION_DIR = integration
BENCHMARK_DIR = benchmarks
MEMORY_DIR = memory
COVERAGE_DIR = coverage_report

# Framework executables
TEST_RUNNER = $(FRAMEWORK_DIR)/test_runner
INTEGRATION_TEST = $(FRAMEWORK_DIR)/integration_test
BENCHMARK = $(FRAMEWORK_DIR)/benchmark
MEMORY_TEST = $(FRAMEWORK_DIR)/memory_test
COVERAGE = $(FRAMEWORK_DIR)/coverage

# Framework sources
FRAMEWORK_SOURCES = $(FRAMEWORK_DIR)/test_runner.c \
                   $(FRAMEWORK_DIR)/integration_test.c \
                   $(FRAMEWORK_DIR)/benchmark.c \
                   $(FRAMEWORK_DIR)/memory_test.c \
                   $(FRAMEWORK_DIR)/coverage.c \
                   $(FRAMEWORK_DIR)/unit_test.c

.PHONY: all clean test unit-test integration-test benchmark memory-test coverage help

all: framework

# Build the test framework
framework: $(TEST_RUNNER) $(INTEGRATION_TEST) $(BENCHMARK) $(MEMORY_TEST) $(COVERAGE)

$(TEST_RUNNER): $(FRAMEWORK_DIR)/test_runner.c
	$(CC) $(CFLAGS) -o $@ $<

$(INTEGRATION_TEST): $(FRAMEWORK_DIR)/integration_test.c
	$(CC) $(CFLAGS) -o $@ $<

$(BENCHMARK): $(FRAMEWORK_DIR)/benchmark.c
	$(CC) $(CFLAGS) -o $@ $<

$(MEMORY_TEST): $(FRAMEWORK_DIR)/memory_test.c
	$(CC) $(CFLAGS) -o $@ $<

$(COVERAGE): $(FRAMEWORK_DIR)/coverage.c
	$(CC) $(CFLAGS) -o $@ $<

# Run all tests
test: unit-test integration-test

# Run unit tests
unit-test: $(TEST_RUNNER)
	@echo "Running unit tests..."
	@mkdir -p $(UNIT_DIR)
	@$(TEST_RUNNER) $(UNIT_DIR)

# Run integration tests
integration-test: $(INTEGRATION_TEST)
	@echo "Running integration tests..."
	@mkdir -p $(INTEGRATION_DIR)
	@cd $(INTEGRATION_DIR) && ../$(INTEGRATION_TEST) integration_tests.conf

# Run performance benchmarks
benchmark: $(BENCHMARK)
	@echo "Running performance benchmarks..."
	@mkdir -p $(BENCHMARK_DIR)
	@cd $(BENCHMARK_DIR) && ../$(BENCHMARK) -i 10 -o benchmark_results.json

# Run memory leak detection
memory-test: $(MEMORY_TEST)
	@echo "Running memory leak detection..."
	@mkdir -p $(MEMORY_DIR)
	@cd $(MEMORY_DIR) && ../$(MEMORY_TEST) --valgrind -o memory_report.md

# Generate coverage report
coverage: $(COVERAGE)
	@echo "Generating code coverage report..."
	@$(COVERAGE) --html -o $(COVERAGE_DIR)

# Build example unit test
example-unit-test: $(FRAMEWORK_DIR)/unit_test.h $(FRAMEWORK_DIR)/unit_test.c
	@echo "Building example unit test..."
	$(CC) $(CFLAGS) -I$(FRAMEWORK_DIR) -o example_test \
		examples/example_unit_test.c $(FRAMEWORK_DIR)/unit_test.c

# Build with debug symbols
debug: CFLAGS += $(DEBUG_FLAGS)
debug: framework

# Build with coverage instrumentation
coverage-build: CFLAGS += $(COVERAGE_FLAGS)
coverage-build: framework

# Create example test files
examples:
	@mkdir -p examples integration
	@echo "Creating example test files..."
	@echo "Example files would be created here (simplified for Makefile compatibility)"

# Install the test framework
install: framework
	@echo "Installing test framework..."
	@mkdir -p /usr/local/bin/wyn-test
	@cp $(TEST_RUNNER) /usr/local/bin/wyn-test/
	@cp $(INTEGRATION_TEST) /usr/local/bin/wyn-test/
	@cp $(BENCHMARK) /usr/local/bin/wyn-test/
	@cp $(MEMORY_TEST) /usr/local/bin/wyn-test/
	@cp $(COVERAGE) /usr/local/bin/wyn-test/
	@cp $(FRAMEWORK_DIR)/unit_test.h /usr/local/include/
	@echo "Test framework installed to /usr/local/bin/wyn-test/"

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -f $(TEST_RUNNER) $(INTEGRATION_TEST) $(BENCHMARK) $(MEMORY_TEST) $(COVERAGE)
	@rm -f example_test
	@rm -rf $(COVERAGE_DIR)
	@rm -f *.gcov *.gcda *.gcno
	@rm -f benchmark_results.json memory_report.md
	@find . -name "*.out" -delete
	@find . -name "*.bench" -delete
	@find . -name "*.memtest" -delete

# Show help
help:
	@echo "Wyn Test Framework"
	@echo "=================="
	@echo ""
	@echo "Available targets:"
	@echo "  all              - Build the complete test framework"
	@echo "  framework        - Build framework executables"
	@echo "  test             - Run all tests (unit + integration)"
	@echo "  unit-test        - Run unit tests"
	@echo "  integration-test - Run integration tests"
	@echo "  benchmark        - Run performance benchmarks"
	@echo "  memory-test      - Run memory leak detection"
	@echo "  coverage         - Generate code coverage report"
	@echo "  examples         - Create example test files"
	@echo "  debug            - Build with debug symbols"
	@echo "  coverage-build   - Build with coverage instrumentation"
	@echo "  install          - Install framework system-wide"
	@echo "  clean            - Clean build artifacts"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Usage examples:"
	@echo "  make framework           # Build the test framework"
	@echo "  make examples           # Create example files"
	@echo "  make test               # Run all tests"
	@echo "  make benchmark          # Run performance tests"
	@echo "  make memory-test        # Check for memory leaks"
	@echo "  make coverage --html    # Generate HTML coverage report"