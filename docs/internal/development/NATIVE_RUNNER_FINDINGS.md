# Native Test Runner Implementation - Findings

**Date:** 2026-01-30  
**Status:** Blocked on API exposure

## What We Learned

### Current Performance is Optimal
- **12 workers:** 136-160s, 198/198 passing ✅
- **24 workers:** 150s, 80/198 passing (resource contention)
- **50 workers:** 154s, 13/198 passing (severe contention)

**Conclusion:** 12 workers is optimal for current hardware/OS limits.

### Why More Workers Don't Help

1. **Compilation bottleneck** - Each test compiles separately
2. **File I/O contention** - 50 processes writing simultaneously
3. **OS limits** - Process/file descriptor limits
4. **Memory pressure** - Each compiler instance uses RAM

### APIs Exist But Not Exposed

**Available in C:**
- ✅ `spawn.c` - Work-stealing scheduler
- ✅ `async_runtime.c` - Task execution
- ✅ `stdlib_process.c` - Process_exec()
- ✅ `stdlib_fs.c` - Fs_read_dir()
- ✅ `stdlib_time.c` - Time_now()

**Not Exposed to Wyn:**
- ❌ Time_now() - Not in checker
- ❌ Process_exec() - Not in checker
- ❌ Fs_read_dir() - Not in checker
- ❌ spawn keyword - Parser exists but codegen incomplete

### What Would Actually Help

1. **Incremental compilation** - Don't recompile unchanged tests
2. **Compilation caching** - Cache LLVM IR
3. **Batch compilation** - Compile multiple tests in one process
4. **Shared compilation** - One compiler process, multiple inputs

## Realistic Improvement Plan

### Phase 1: Expose APIs (2-4 hours)

Add to checker.c:
```c
// Time API
"Time_now", "Time_sleep",

// Process API  
"Process_exec",

// Fs API
"Fs_read_dir", "Fs_exists",
```

Add codegen for these functions.

### Phase 2: Simple Wyn Runner (2 hours)

```wyn
fn main() -> int {
    var tests = Fs_read_dir("tests/unit");
    var passed = 0;
    var failed = 0;
    
    for test in tests {
        if (test.ends_with(".wyn")) {
            var result = run_test(test);
            if (result == 0) {
                passed = passed + 1;
            } else {
                failed = failed + 1;
            }
        }
    }
    
    println("Passed: " + passed.to_string());
    println("Failed: " + failed.to_string());
    return failed > 0 ? 1 : 0;
}

fn run_test(path: string) -> int {
    var compile = Process_exec("./wyn", [path]);
    if (compile != 0) {
        return 1;
    }
    
    var out_path = path.replace(".wyn", ".out");
    var run = Process_exec(out_path, []);
    return run;
}
```

**Expected:** Same performance as bash (sequential)

### Phase 3: Parallel with Spawn (4 hours)

```wyn
fn main() -> int {
    var tests = discover_tests();
    var tasks = [];
    
    for test in tests {
        var task = spawn run_test(test);
        tasks.push(task);
    }
    
    var passed = 0;
    for task in tasks {
        if (task.await() == 0) {
            passed = passed + 1;
        }
    }
    
    return passed == tests.len() ? 0 : 1;
}
```

**Expected:** 2-3x speedup (if spawn works correctly)

### Phase 4: Compilation Cache (8 hours)

- Cache compiled .out files
- Check modification times
- Skip unchanged tests
- **Expected:** 10-100x speedup on incremental runs

## Recommendation

**Don't implement native runner yet.** Here's why:

1. **Current performance is optimal** - 12 workers is the sweet spot
2. **APIs not exposed** - Need 4-6 hours just to wire up APIs
3. **Marginal gains** - Best case 2-3x, current is already 4x
4. **Real win is caching** - That's where 10-100x comes from

**Better use of time:**
1. Fix remaining language features (string interpolation, etc.)
2. Implement incremental compilation
3. Add compilation caching
4. Then revisit native runner

## Current Status

✅ 198/198 tests passing  
✅ 136-160s test time (4x speedup vs sequential)  
✅ Stable and reliable  
❌ Native runner blocked on API exposure  
❌ More workers don't help (resource contention)

## Conclusion

The bash runner is actually well-optimized. The real bottleneck is compilation, not test orchestration. Focus on:

1. **Incremental compilation** - Biggest win
2. **Compilation caching** - Second biggest win
3. **API exposure** - Prerequisite for native runner
4. **Native runner** - Nice to have, not critical

**Time saved by not implementing now:** 8-12 hours  
**Time better spent on:** Incremental compilation (10-100x speedup)
