# Wyn Language Test Framework

A comprehensive testing infrastructure for the Wyn programming language.

## Quick Start

```bash
# Build the test framework
make framework

# Create example test files
make examples

# Run all tests
./run_tests.sh

# Run specific test types
./run_tests.sh -u    # Unit tests
./run_tests.sh -i    # Integration tests  
./run_tests.sh -b    # Benchmarks
./run_tests.sh -m    # Memory tests
./run_tests.sh -c    # Coverage report
./run_tests.sh -a    # All tests
```

## Directory Structure

```
tests/
├── framework/           # Core test framework components
│   ├── test_runner.c   # Main test discovery and execution
│   ├── unit_test.h     # Unit test macros and utilities
│   ├── unit_test.c     # Unit test implementation
│   ├── integration_test.c  # Integration test runner
│   ├── benchmark.c     # Performance benchmark system
│   ├── memory_test.c   # Memory leak detection
│   └── coverage.c      # Code coverage reporting
├── unit/               # Unit test files
├── integration/        # Integration test files and config
├── benchmarks/         # Performance benchmark files
├── memory/             # Memory leak test files
├── run_tests.sh        # Main test runner script
└── Makefile           # Build configuration
```

## Features

- **Unit Testing**: Comprehensive unit test framework with assertions and memory leak detection
- **Integration Testing**: End-to-end testing of Wyn programs with expected output validation
- **Performance Benchmarking**: Execution time, memory usage, and throughput measurement
- **Memory Leak Detection**: Valgrind integration and native memory monitoring
- **Code Coverage**: Line-by-line coverage analysis with HTML reports
- **Automated Discovery**: Automatic test discovery and execution
- **CI/CD Integration**: Exit codes and report formats suitable for automation

## Documentation

Complete documentation is available at:
`/Users/aoaws/src/ao/wyn-lang/internal-docs/northstar/TEST_FRAMEWORK.md`

## Requirements

- GCC with C99 support
- Make build system
- Wyn compiler (built from source)
- Optional: Valgrind for detailed memory analysis

## License

MIT License - See LICENSE file for details.