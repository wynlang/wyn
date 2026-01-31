# async/await vs spawn - Implementation & Benchmarks

## Implementation Approach

Since full type system changes are complex, I'll implement async/await with a **simpler, practical approach**:

### async fn - Auto-spawning Functions

```wyn
// async function automatically spawns when called
async fn compute(n: int) -> int {
    return n * n;
}

// Calling it returns Future immediately
var future = compute(10);  // Spawns automatically
var result = future.await();  // Wait for result
```

**Implementation**: When calling an async function, automatically wrap it in spawn.

### spawn - Explicit Parallelism

```wyn
// Regular function
fn compute(n: int) -> int {
    return n * n;
}

// Explicitly spawn it
var future = spawn compute(10);
var result = future.await();
```

**Implementation**: Already working!

## Key Differences

| Feature | async fn | spawn |
|---------|----------|-------|
| **Declaration** | `async fn foo()` | `fn foo()` |
| **Calling** | Auto-spawns | Must use `spawn` |
| **Use case** | Naturally async operations | Explicit parallelism |
| **Syntax** | Cleaner for async code | More explicit |

## Comparison with Other Languages

### JavaScript

```javascript
// async function
async function fetchData() {
    let response = await fetch(url);
    return response;
}

// Calling returns Promise
let promise = fetchData();  // Doesn't run yet!
let data = await promise;   // Runs when awaited
```

**Key**: async functions return Promise but don't run until awaited.

### Rust

```rust
// async function
async fn fetch_data() -> Data {
    let response = http_get(url).await;
    response
}

// Must spawn explicitly
tokio::spawn(async {
    fetch_data().await
});
```

**Key**: async functions are lazy, must be spawned or awaited.

### Go

```go
// No async keyword
func fetchData() Data {
    return httpGet(url)
}

// Explicit spawn
go fetchData()
```

**Key**: No async/await, just explicit `go` keyword.

### Wyn (Proposed)

```wyn
// Option 1: async (auto-spawns)
async fn fetch_data() -> Data {
    return http_get(url);
}

var future = fetch_data();  // Spawns immediately
var data = future.await();

// Option 2: spawn (explicit)
fn fetch_data() -> Data {
    return http_get(url);
}

var future = spawn fetch_data();
var data = future.await();
```

**Key**: async functions auto-spawn (eager), spawn is explicit.

## Use Cases

### Use async fn When:

1. **Function is naturally async**
```wyn
async fn fetch_user(id: int) -> User {
    return db_query(id);
}

// Cleaner than:
fn fetch_user(id: int) -> User {
    return db_query(id);
}
spawn fetch_user(id);  // Extra boilerplate
```

2. **Building async APIs**
```wyn
async fn api_get(url: string) -> Response {
    return http_get(url);
}

async fn api_post(url: string, data: Data) -> Response {
    return http_post(url, data);
}

// Users don't need to remember to spawn
var response = api_get("/users").await();
```

3. **Async by default**
```wyn
async fn process_pipeline() -> Result {
    var data = fetch_data().await();
    var processed = transform(data).await();
    var saved = save(processed).await();
    return saved;
}
```

### Use spawn When:

1. **Explicit parallelism**
```wyn
// I want these to run in parallel
spawn task1();
spawn task2();
spawn task3();
```

2. **Fire-and-forget**
```wyn
spawn log_event("User logged in");
// Don't care about result
```

3. **Converting sync to async**
```wyn
fn expensive_computation() -> int {
    // CPU-intensive work
    return result;
}

// Make it async on-demand
var future = spawn expensive_computation();
```

## Performance Comparison

### Benchmark Setup

```wyn
// Test 1: Overhead
async fn async_noop() -> int { return 0; }
fn sync_noop() -> int { return 0; }

// Test 2: Parallel execution
async fn async_compute(n: int) -> int { return n * n; }
fn sync_compute(n: int) -> int { return n * n; }
```

### Expected Results

| Test | async fn | spawn | Regular |
|------|----------|-------|---------|
| Overhead | ~150ns | ~150ns | ~5ns |
| 10k calls | ~1.5ms | ~1.5ms | ~50μs |
| Parallel (3 tasks) | ~100ms | ~100ms | ~300ms |

**Key Insight**: async and spawn have same overhead (both use scheduler), but much faster than sequential for parallel work.

## Recommendations

### For Wyn

**Use both**:
- `async fn` for naturally async operations (I/O, network)
- `spawn` for explicit parallelism (CPU-bound work)

```wyn
// Async I/O
async fn fetch_data(url: string) -> Data {
    return http_get(url);
}

// Explicit parallelism
fn compute(n: int) -> int {
    return fib(n);
}

var f1 = spawn compute(40);
var f2 = spawn compute(41);
var f3 = spawn compute(42);
```

### Comparison Summary

| Language | async | spawn | Philosophy |
|----------|-------|-------|------------|
| **JavaScript** | Lazy | Manual | async/await everywhere |
| **Rust** | Lazy | tokio::spawn | Explicit async |
| **Go** | None | go keyword | Explicit only |
| **Wyn** | Eager | spawn keyword | Both available |

**Wyn's approach**: Combines Go's simplicity (spawn) with Rust's expressiveness (async), but makes async eager (auto-spawns) for simplicity.

## Current Status

✅ **spawn**: Fully working  
⏳ **async**: Parsed, needs codegen to auto-spawn  
⏳ **await**: Needs method syntax  

## Next Steps

1. Make async functions auto-spawn at call site
2. Implement .await() method syntax
3. Benchmark async vs spawn vs sequential
4. Document best practices

## Conclusion

**async fn** and **spawn** serve different purposes:
- async = "This function is async by nature"
- spawn = "Make this parallel explicitly"

Both use the same underlying scheduler, so performance is identical. The choice is about **API design** and **code clarity**.
