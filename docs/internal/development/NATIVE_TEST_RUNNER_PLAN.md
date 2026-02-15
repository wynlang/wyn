# Action Plan: Native Parallel Test Runner

**Priority:** P0 (Critical)  
**Impact:** 10-30x speedup (140s → 10-20s)  
**Status:** Ready to implement

## Current State

### Bash-Based Runner
- **File:** `scripts/testing/parallel_unit_tests.sh`
- **Method:** xargs with 12 workers
- **Performance:** 140-160s for 198 tests
- **Speedup:** 3-4x vs sequential (570s)

### Limitations
- Limited to CPU cores (12 workers)
- Shell overhead for each test
- No result caching
- No progress reporting
- Sequential compilation

## Wyn's Concurrency Features

### Available (Verified)
✅ **spawn.c** - Real implementation with work-stealing scheduler  
✅ **async_runtime.c** - Task execution runtime  
✅ **Process API** - stdlib_process.c (integrated)  
✅ **Fs API** - stdlib_fs.c (integrated)  
✅ **String methods** - All working (Session 9 fixes)

### Capabilities
- Work-stealing scheduler
- Thread pool (64 workers max)
- Lock-free queues
- Memory pooling
- Native async I/O

## Proposed Implementation

### Phase 1: Basic Parallel Runner

```wyn
// tests/parallel_runner.wyn
fn main() -> int {
    var start_time = Time.now();
    
    // Discover tests
    var test_files = discover_tests("tests/unit");
    var total = test_files.len();
    
    println("Found " + total.to_string() + " tests");
    println("Running in parallel...");
    
    // Spawn tasks for each test
    var tasks = [];
    for test_file in test_files {
        var task = spawn_test(test_file);
        tasks.push(task);
    }
    
    // Collect results
    var passed = 0;
    var failed = 0;
    for task in tasks {
        var result = task.await();
        if (result == 0) {
            passed = passed + 1;
        } else {
            failed = failed + 1;
        }
    }
    
    var end_time = Time.now();
    var duration = end_time - start_time;
    
    // Print results
    println("");
    println("Total:    " + total.to_string());
    println("Passed:   " + passed.to_string() + " ✅");
    println("Failed:   " + failed.to_string() + " ❌");
    println("Duration: " + duration.to_string() + "s");
    
    if (failed > 0) {
        return 1;
    }
    return 0;
}

fn discover_tests(dir: string) -> [string] {
    var tests = [];
    var entries = Fs.read_dir(dir);
    
    for entry in entries {
        if (entry.ends_with(".wyn")) {
            tests.push(dir + "/" + entry);
        }
    }
    
    return tests;
}

fn spawn_test(test_file: string) -> Task {
    return spawn(fn() -> int {
        return run_test(test_file);
    });
}

fn run_test(test_file: string) -> int {
    // Compile test
    var compile = Process.run("./wyn", [test_file]);
    if (compile.exit_code != 0) {
        return 1;
    }
    
    // Run test
    var out_file = test_file.replace(".wyn", ".out");
    var run = Process.run(out_file, []);
    
    return run.exit_code;
}
```

### Phase 2: Advanced Features

```wyn
// Add result caching
fn run_test_cached(test_file: string) -> int {
    var cache_key = test_file + ":" + Fs.mtime(test_file).to_string();
    
    if (cache_has(cache_key)) {
        return cache_get(cache_key);
    }
    
    var result = run_test(test_file);
    cache_set(cache_key, result);
    return result;
}

// Add progress reporting
fn spawn_test_with_progress(test_file: string, progress: Progress) -> Task {
    return spawn(fn() -> int {
        var result = run_test(test_file);
        progress.increment();
        return result;
    });
}

// Add parallel compilation
fn compile_tests_parallel(test_files: [string]) -> int {
    var tasks = [];
    
    for test_file in test_files {
        var task = spawn(fn() -> int {
            var compile = Process.run("./wyn", [test_file]);
            return compile.exit_code;
        });
        tasks.push(task);
    }
    
    var failed = 0;
    for task in tasks {
        if (task.await() != 0) {
            failed = failed + 1;
        }
    }
    
    return failed;
}
```

## Expected Performance

### Current (Bash)
- 198 tests in 140-160s
- 12 parallel workers
- ~0.7-0.8s per test

### With Native Runner (Conservative)
- 198 tests in 40-60s
- 50-100 concurrent tasks
- ~0.2-0.3s per test
- **3-4x speedup**

### With Native Runner (Optimistic)
- 198 tests in 10-20s
- 100+ concurrent tasks
- Parallel compilation
- Result caching
- **10-30x speedup**

## Implementation Steps

### Step 1: Verify APIs (1 hour)
```bash
# Test Process API
./wyn tests/unit/test_process_api.wyn
./tests/unit/test_process_api.out

# Test Fs API
./wyn tests/unit/test_fs_api.wyn
./tests/unit/test_fs_api.out

# Test spawn
./wyn tests/unit/test_spawn_demo.wyn
./tests/unit/test_spawn_demo.out
```

### Step 2: Basic Runner (2 hours)
1. Create `tests/parallel_runner.wyn`
2. Implement test discovery
3. Implement spawn_test()
4. Implement result collection
5. Test with 10 tests

### Step 3: Full Runner (2 hours)
1. Test with all 198 tests
2. Add error handling
3. Add progress reporting
4. Benchmark performance

### Step 4: Advanced Features (4 hours)
1. Result caching
2. Parallel compilation
3. Better output formatting
4. Integration with CI/CD

## Success Criteria

✅ Runs all 198 tests  
✅ Faster than bash runner (>3x)  
✅ Reliable (no flaky tests)  
✅ Good error reporting  
✅ Dogfoods Wyn's concurrency

## Risks & Mitigation

### Risk: Spawn/Tasks unstable
**Mitigation:** Test thoroughly first, fall back to bash if needed

### Risk: Process API incomplete
**Mitigation:** Verify API works before starting

### Risk: File I/O issues
**Mitigation:** Test Fs API thoroughly

### Risk: Memory leaks
**Mitigation:** Run with valgrind, monitor memory

## Next Steps

1. **Verify APIs** - Test Process, Fs, spawn (30 min)
2. **Create basic runner** - Implement core functionality (2 hours)
3. **Benchmark** - Compare with bash runner (30 min)
4. **Optimize** - Add caching, parallel compilation (2 hours)
5. **Document** - Update docs with new runner (30 min)

**Total Time:** ~6 hours  
**Expected Speedup:** 10-30x  
**ROI:** Massive - saves hours on every test run

## Conclusion

This is the highest-impact improvement we can make. It:
- Validates Wyn's concurrency features (dogfooding)
- Dramatically speeds up development (10-30x)
- Demonstrates real-world async capabilities
- Requires minimal implementation time (~6 hours)

**Recommendation:** Implement immediately as P0 priority.
