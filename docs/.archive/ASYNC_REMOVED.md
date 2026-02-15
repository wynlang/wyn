# async Keyword REMOVED ✅

## Change Summary

**Removed `async` keyword from Wyn language.**

**Reason**: Simplicity. `spawn` does everything needed for concurrency.

## What Was Removed

1. **Lexer** (`src/lexer.c`):
   - Removed `async` keyword recognition
   
2. **Token Types** (`src/common.h`):
   - Removed `TOKEN_ASYNC` enum value
   
3. **Parser** (`src/parser.c`):
   - Removed `async` parsing from function declarations
   - `is_async` flag always set to false

## Wyn Concurrency Model (Final)

### Simple and Explicit

```wyn
// Regular function
fn compute(n: int) -> int {
    return n * n;
}

// Spawn returns Future
var f = spawn compute(42);

// Await blocks and returns result
var result = f.await();

// Fire-and-forget (no await needed)
spawn log_event("User logged in");

// Multiple concurrent tasks
var f1 = spawn task1();
var f2 = spawn task2();
var r1 = f1.await();
var r2 = f2.await();
```

### One Keyword, One Behavior

- `spawn` - Creates concurrent task, returns Future
- `.await()` - Blocks until result ready
- No `async` keyword needed
- No async contagion
- Explicit concurrency

## Verification

### Test: async keyword rejected
```bash
$ cat tests/test_async_removed.wyn
async fn compute(n: int) -> int {
    return n * n;
}

$ ./wyn tests/test_async_removed.wyn
Error at line 1: Undefined variable 'async'
```
✅ **async is no longer recognized**

### Test: spawn/await still works
```bash
$ ./wyn tests/test_future_complete.wyn && ./tests/test_future_complete.out
Compiled successfully
1
```
✅ **spawn/await works perfectly**

## Comparison with Other Languages

| Language | Async Keyword | Spawn Keyword | Complexity |
|----------|--------------|---------------|------------|
| JavaScript | Required | None | High (async contagion) |
| Rust | Required | spawn/tokio | High (lazy evaluation) |
| Go | None | go | Low (explicit) |
| **Wyn** | **None** | **spawn** | **Low (explicit)** |

## Benefits

1. **Simpler**: One keyword instead of two
2. **Explicit**: You see exactly when concurrency happens
3. **No confusion**: spawn always creates a task
4. **No contagion**: Don't need to mark everything async
5. **Go-style**: Proven design at scale

## Migration

If you had code with `async fn`:

**Before:**
```wyn
async fn compute(n: int) -> int {
    return n * n;
}
```

**After:**
```wyn
fn compute(n: int) -> int {
    return n * n;
}
```

Just remove the `async` keyword. It did nothing anyway.

## Final Design

```wyn
// Define any function
fn task() -> int {
    return 42;
}

// Spawn it explicitly
var future = spawn task();

// Await it explicitly
var result = future.await();
```

**Simple. Explicit. No confusion.**

This is the right design for Wyn.
