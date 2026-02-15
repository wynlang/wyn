# Spawn Implementation Status - Critical Finding

**Date:** 2026-01-30  
**Status:** ⚠️ Spawn is synchronous, not parallel

## Test Results

### Test 1: spawn_many.wyn
- Spawned 1000 tasks
- Took 1 second
- ✓ Fast but...

### Test 2: spawn_parallel.wyn  
- Spawned 10 tasks, each sleep(100ms)
- Expected: ~100ms (parallel)
- Actual: 2000ms (sequential)
- ✗ **Tasks run sequentially, not in parallel**

## Root Cause

Current LLVM codegen:
```c
void codegen_spawn_statement(Stmt* stmt, LLVMCodegenContext* ctx) {
    // Just calls the function directly
    codegen_expression(stmt->spawn.call, ctx);
}
```

**This is synchronous!** It doesn't actually spawn.

## What's Needed

The spawn runtime exists (`spawn.c`):
```c
void wyn_spawn(WynSpawnFunc func, void* arg);
```

But to use it, we need:

1. **Create wrapper functions** for each spawn call
2. **Pass function pointer** to wyn_spawn
3. **Handle arguments** properly
4. **Link with spawn.c** runtime

## Old Codegen Does This

File: `src/codegen.c` lines 4733-4750
```c
case STMT_SPAWN: {
    // Generate wrapper function
    emit("wyn_spawn(__spawn_wrapper_");
    emit(func_name);
    emit(", NULL);\n");
}
```

It generates wrapper functions in a pre-scan phase.

## Why LLVM Codegen Doesn't

LLVM codegen is single-pass. Creating wrapper functions dynamically is complex:
1. Need to create new LLVM functions
2. Need to handle closures/captures
3. Need to manage function pointers
4. Need proper calling conventions

## Options

### Option 1: Implement Full Async Spawn (Complex)
- Create wrapper functions in LLVM
- Handle closures
- Proper async execution
- **Time:** 8-12 hours

### Option 2: Use Old Codegen for Spawn (Hybrid)
- Keep LLVM for most code
- Use C codegen for spawn statements
- **Time:** 2-4 hours

### Option 3: Simple Thread Pool (Pragmatic)
- Create simple thread pool API
- Expose to Wyn
- Use for test runner
- **Time:** 2-3 hours

## Recommendation

**Option 3: Simple Thread Pool**

Create:
```wyn
fn thread_pool_run(func_name: string, arg: string) -> int;
fn thread_pool_wait() -> int;
```

Use for test runner:
```wyn
for test in tests {
    thread_pool_run("run_test", test);
}
thread_pool_wait();
```

This gives us true parallelism without complex spawn implementation.

## Current State

✅ spawn keyword compiles  
✅ spawn executes  
❌ spawn is synchronous (not parallel)  
❌ Cannot run millions of tasks  
❌ Not like goroutines

**Spawn works, but it's not async. It's just a function call.**
