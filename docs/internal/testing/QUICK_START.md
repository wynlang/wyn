# Quick Start Guide

## Immediate: Use Bash Parallel Runner (5-8x Speedup)

```bash
cd wyn/tests
./parallel_test_runner.sh
```

**Expected:** 579 tests complete in ~60-90 seconds (vs 500s)

## Demo the Speedup

```bash
cd wyn/tests
./demo_parallel.sh
```

This runs 20 tests both ways to show the difference.

## TDD: Test the New Stdlib Functions

```bash
cd wyn/tests
./run_tdd_tests.sh
```

This compiles and tests all new stdlib functions.

## Files Overview

### Ready to Use NOW
- `parallel_test_runner.sh` - **Use this for 5-8x speedup**
- `demo_parallel.sh` - Demo the speedup

### TDD Tests (Test-First)
- `test_process_api.wyn` - Process execution tests
- `test_fs_api.wyn` - Filesystem tests
- `test_time_api.wyn` - Time tests
- `test_task_api.wyn` - Task API tests

### Implementations
- `../src/stdlib_process.c` - Process functions
- `../src/stdlib_fs.c` - Filesystem functions
- `../src/stdlib_time.c` - Time functions

### Documentation
- `README_TDD.md` - Complete summary
- `TDD_IMPLEMENTATION_GUIDE.md` - Integration steps
- `PARALLEL_TESTING.md` - Parallel testing docs

## One-Liner

```bash
cd wyn/tests && ./parallel_test_runner.sh
```

That's it! 5-8x faster tests.
