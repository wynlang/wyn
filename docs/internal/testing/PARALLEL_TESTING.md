# Parallel Test Runner

## Problem

Running 579 tests sequentially takes **500+ seconds** (~8 minutes).

## Solution

Run tests in **parallel** using multiple CPU cores.

## Usage

### Quick Start

```bash
./parallel_test_runner.sh
```

This will:
- Auto-detect CPU count (e.g., 8 cores)
- Run 8 tests simultaneously
- Complete in **~60-90 seconds** (5-8x faster)

### Custom Worker Count

```bash
NUM_JOBS=16 ./parallel_test_runner.sh
```

### Requirements

**Minimum:**
- `xargs` (built-in on all Unix systems)

**Recommended:**
- `gnu-parallel` for better progress display

Install on macOS:
```bash
brew install parallel
```

Install on Linux:
```bash
sudo apt-get install parallel  # Debian/Ubuntu
sudo yum install parallel      # RHEL/CentOS
```

## Performance

### Before (Sequential)
```
500+ seconds for 579 tests
~1.2 tests/sec
```

### After (Parallel, 8 cores)
```
60-90 seconds for 579 tests
~6-10 tests/sec
5-8x speedup
```

### After (Parallel, 16 cores)
```
30-45 seconds for 579 tests
~13-19 tests/sec
10-16x speedup
```

## How It Works

1. **Finds all `.wyn` test files** in current directory
2. **Spawns N worker processes** (N = CPU count)
3. **Each worker runs one test** at a time
4. **Results collected** in temp directory
5. **Summary printed** at end

## Features

- ✅ Parallel execution (uses all CPU cores)
- ✅ Timeout per test (10s default)
- ✅ Skip known problematic tests
- ✅ Progress display (with GNU parallel)
- ✅ Detailed failure reporting
- ✅ Works on macOS and Linux

## Limitations

- Tests must be **independent** (no shared state)
- Tests must be **deterministic** (same input = same output)
- Tests that spawn background processes may interfere

## Future: Wyn-Native Parallel Runner

The `parallel_test_runner.wyn` file demonstrates native Wyn implementation using `spawn`. 

**Current status:** Spawn is implemented and stable (see `examples/54_spawn_basics.wyn`). The test runner needs:
- High-level Task API (`Spawn::task`, `Task::await`) exposed in stdlib
- Process execution API (`Process::exec_timeout`)
- File system API (`Fs::read_dir`)

Once these APIs are added, the native runner will:
- Use Wyn's built-in `spawn` for concurrency
- Better integration with Wyn runtime
- Cross-platform (no bash required)
- More control over scheduling

## Comparison

| Method | Time | Speedup | Pros | Cons |
|--------|------|---------|------|------|
| Sequential | 500s | 1x | Simple, reliable | Slow |
| Bash parallel | 60-90s | 5-8x | Fast, works now | Requires bash |
| Wyn native | TBD | TBD | Native, portable | Needs stdlib APIs |

## Recommendation

**Use `parallel_test_runner.sh` now** for immediate 5-8x speedup.

Switch to `parallel_test_runner.wyn` once stdlib APIs are added.
