# async vs spawn: Design Analysis

## Current State

### What Works
- `spawn func()` - Creates concurrent task, returns Future
- `future.await()` - Blocks and gets result
- `async fn` - Parsed but does nothing (no-op marker)

### The Question
**Is `async` keyword needed if spawn works for everything?**

## Analysis

### Option 1: Keep Both (Current)
```wyn
async fn compute(n: int) -> int {  // Just a marker
    return n * n;
}

var f = spawn compute(10);  // Explicit concurrency
var r = f.await();
```

**Pros:**
- `async` documents intent (this function is concurrent-safe)
- `spawn` is explicit about when concurrency happens
- Clear separation: async = marker, spawn = action

**Cons:**
- `async` does nothing - confusing
- Extra keyword with no behavior
- Not like Rust/JS where async changes behavior

### Option 2: Remove async, Use spawn Only
```wyn
fn compute(n: int) -> int {
    return n * n;
}

var f = spawn compute(10);  // Explicit concurrency
var r = f.await();
```

**Pros:**
- ✅ Simpler - one less keyword
- ✅ Explicit - you see exactly when concurrency happens
- ✅ No confusion - spawn does what it says
- ✅ Like Go - no async keyword, just explicit spawn

**Cons:**
- No way to mark functions as "concurrent-safe"
- Can spawn any function (might not be thread-safe)

### Option 3: Make async Actually Do Something
```wyn
async fn compute(n: int) -> int {
    return n * n;
}

// Auto-returns Future<int>
var f = compute(10);  // Implicitly spawned
var r = f.await();
```

**Pros:**
- Like Rust/JavaScript
- async changes behavior (returns Future)
- No need for spawn keyword

**Cons:**
- ❌ Confusing when code runs (lazy evaluation)
- ❌ Async contagion (must await everywhere)
- ❌ More complex implementation
- ❌ Against Wyn's "explicit" philosophy

## Recommendation: **REMOVE async, Keep spawn**

### Why?

1. **Simplicity**: One keyword (`spawn`) that does one thing
2. **Explicitness**: You see exactly when concurrency happens
3. **No confusion**: spawn always creates a task
4. **Go-style**: Proven design (goroutines)
5. **async is useless**: Currently does nothing, would be complex to make useful

### Comparison with Other Languages

**JavaScript:**
```js
async function compute(n) { return n * n; }
let f = compute(10);  // Returns Promise (lazy)
let r = await f;      // Must await
```
- Confusing: When does compute() run?
- Async contagion: Must mark everything async

**Rust:**
```rust
async fn compute(n: i32) -> i32 { n * n }
let f = compute(10);  // Returns Future (lazy)
let r = f.await;      // Must await or spawn
```
- Lazy evaluation: compute() doesn't run until awaited
- Complex: Need runtime (tokio)

**Go:**
```go
func compute(n int) int { return n * n }
go compute(10)  // Explicit spawn
```
- Simple: One keyword (go)
- Explicit: You see the concurrency
- No async keyword needed

**Wyn (Proposed):**
```wyn
fn compute(n: int) -> int { return n * n }
spawn compute(10)  // Explicit spawn
```
- ✅ Simple like Go
- ✅ Explicit concurrency
- ✅ No confusion

## Implementation Impact

### Remove async Keyword

**Files to modify:**
1. `src/lexer.c` - Remove TOKEN_ASYNC
2. `src/parser.c` - Remove async parsing
3. `src/ast.h` - Remove is_async flag
4. Documentation - Update to spawn-only

**Effort:** ~30 minutes

**Benefit:** Simpler language, less confusion

### Keep spawn/await

**Already working:**
- `spawn func()` - Creates Future
- `future.await()` - Gets result
- Lock-free scheduler
- No memory leaks

**No changes needed**

## Final Design

### Proposed Wyn Concurrency Model

```wyn
// Regular function
fn compute(n: int) -> int {
    return n * n;
}

// Explicit spawn returns Future
var f = spawn compute(42);

// Explicit await blocks
var result = f.await();

// Fire-and-forget (no await)
spawn log_event("User logged in");

// Multiple concurrent tasks
var f1 = spawn task1();
var f2 = spawn task2();
var r1 = f1.await();
var r2 = f2.await();
```

**Characteristics:**
- ✅ Explicit concurrency (spawn keyword)
- ✅ No async contagion
- ✅ Simple mental model
- ✅ Works with any function
- ✅ Optional await (fire-and-forget)

## Comparison Table

| Feature | JavaScript | Rust | Go | Wyn (Proposed) |
|---------|-----------|------|-----|----------------|
| Async keyword | Required | Required | None | None |
| Spawn keyword | None | spawn/tokio | go | spawn |
| Lazy evaluation | Yes | Yes | No | No |
| Async contagion | Yes | Yes | No | No |
| Explicit concurrency | No | Partial | Yes | Yes |
| Simplicity | ⭐⭐ | ⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |

## Conclusion

**REMOVE `async` keyword. Keep `spawn` only.**

### Rationale

1. `async` currently does nothing (no-op)
2. Making it useful would add complexity (lazy evaluation, contagion)
3. `spawn` is explicit and simple
4. Go proves this model works at scale
5. Wyn's philosophy: explicit > implicit

### Result

```wyn
// Simple, explicit, no confusion
fn compute(n: int) -> int { return n * n }
var f = spawn compute(42);
var r = f.await();
```

**One keyword. One behavior. No confusion.**

This is the right design for Wyn.
