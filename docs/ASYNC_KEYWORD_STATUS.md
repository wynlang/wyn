# async Keyword - Implementation Status

## Current Status: PARTIALLY WORKING ⏳

The `async` keyword is **parsed** but doesn't change function behavior yet.

## What Works ✅

### 1. Parsing
```wyn
async fn compute(n: int) -> int {
    return n * n;
}
```
- ✅ Lexer recognizes `async` keyword
- ✅ Parser sets `is_async` flag on function
- ✅ Compiles without errors

### 2. Basic Usage
```wyn
async fn simple() -> int {
    return 42;
}

fn main() -> int {
    simple();        // Works
    spawn simple();  // Works
    return 0;
}
```

## What Doesn't Work Yet ❌

### 1. Automatic Future Return Type
```wyn
async fn compute() -> int {
    return 42;
}

// Should return Future<int>, but currently returns int
var future = compute();  // Type mismatch
```

### 2. await Inside async Functions
```wyn
async fn fetch() -> int {
    var f = other_async();
    var result = f.await();  // Not implemented
    return result;
}
```

### 3. Type Checking
- async functions should have Future<T> return type
- Calling async function should return Future
- Type checker doesn't enforce this yet

## Implementation Plan

### Phase 1: Type System (Minimal) ✅
- [x] Add TYPE_FUTURE to TypeKind
- [x] Add FutureType struct
- [x] Add EXPR_SPAWN

### Phase 2: Parser (Done) ✅
- [x] TOKEN_ASYNC exists
- [x] is_async flag on FnStmt
- [x] Parse async fn syntax

### Phase 3: Type Checker (TODO)
```c
// In checker.c, when checking function:
if (stmt->fn.is_async) {
    // Wrap return type in Future<T>
    Type* future_type = make_future_type(stmt->fn.return_type);
    stmt->fn.return_type = future_type;
}
```

### Phase 4: Code Generation (TODO)
```c
// In llvm_function_codegen.c:
if (fn_stmt->is_async) {
    // Generate wrapper that:
    // 1. Allocates Future
    // 2. Spawns task
    // 3. Returns Future*
}
```

### Phase 5: await Expression (TODO)
```c
// In parser.c:
if (match(TOKEN_DOT) && match(TOKEN_AWAIT)) {
    // Create await expression
}

// In codegen:
// Generate call to future_get()
```

## Minimal Working Implementation

For now, `async` is just a marker. To make it functional:

### Option A: Simple (Current Approach)
Make `async` just documentation - doesn't change behavior:
```wyn
async fn compute() -> int {  // Just a hint, returns int
    return 42;
}

spawn compute();  // Explicitly spawn it
```

### Option B: Full Implementation (Future Work)
Make `async` change return type:
```wyn
async fn compute() -> int {  // Actually returns Future<int>
    return 42;
}

var future = compute();  // Returns Future<int>
var result = future.await();  // Get value
```

## Current Recommendation

**Keep async as a marker for now**. It:
- ✅ Compiles
- ✅ Documents intent
- ✅ Works with spawn
- ❌ Doesn't auto-wrap in Future (use spawn explicitly)

This is similar to Go's approach: no `async` keyword, just explicit `go` (spawn).

## Testing

```bash
# Test async keyword parsing
./wyn tests/test_async_keyword.wyn
./tests/test_async_keyword.out
# Output: 1 (success)
```

## Summary

| Feature | Status | Notes |
|---------|--------|-------|
| `async` keyword | ✅ Parsed | Recognized by lexer/parser |
| `is_async` flag | ✅ Set | Stored in AST |
| Future return type | ❌ Not implemented | Returns T, not Future<T> |
| await in async | ❌ Not implemented | No special behavior |
| Type checking | ❌ Not implemented | Doesn't enforce Future |
| Code generation | ❌ Not implemented | No wrapper generated |

**Status**: async keyword exists but is currently just a marker/hint. Full implementation requires type checker and codegen changes.

**Recommendation**: Use `spawn` explicitly for now. The `async` keyword can be enhanced later when needed.
