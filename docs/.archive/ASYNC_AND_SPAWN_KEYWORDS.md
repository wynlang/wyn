# Wyn's Concurrency Model: spawn vs async

## Current Status

**Implemented**: ✅ `spawn` keyword  
**Partially Implemented**: ⏳ `async` keyword (token exists, not fully functional)  
**Implemented**: ✅ `await` expression

## What Wyn Has Now

### spawn - Task Creation (Working ✅)

```wyn
fn compute(n: int) -> int {
    return n * n;
}

// spawn creates a concurrent task
spawn compute(10);  // Runs on worker thread immediately
```

**Purpose**: Create a new concurrent task that runs in parallel

### await - Wait for Result (Partially Working ⏳)

```wyn
// Future syntax (not yet fully working)
let future = spawn compute(10);
let result = future.await();  // Wait for result
```

**Purpose**: Block until a Future completes and get its value

## What async Could Add

The `async` keyword is **partially implemented** but could be used for:

### Option 1: Mark Functions That Return Futures

```wyn
// Without async (current)
fn fetch_data(url: string) -> Future<Data> {
    return spawn {
        // ... fetch logic
    };
}

// With async (cleaner)
async fn fetch_data(url: string) -> Data {
    // Automatically returns Future<Data>
    // ... fetch logic
}
```

**Benefit**: Cleaner syntax, automatic Future wrapping

### Option 2: Enable await Inside Functions

```wyn
// Without async (error)
fn process() -> int {
    let data = fetch_data().await();  // Error: can't await here
    return process(data);
}

// With async (works)
async fn process() -> int {
    let data = fetch_data().await();  // OK: async function can await
    return process(data);
}
```

**Benefit**: Only async functions can use await (prevents blocking in sync code)

## Proposed Design: Both Keywords

### spawn - Explicit Parallelism

**Use**: When you want to explicitly create a new task

```wyn
fn compute(n: int) -> int {
    return n * n;
}

// Explicit: "I'm creating a parallel task"
let future = spawn compute(10);
```

**Characteristics**:
- Works with any function
- Creates a new task on the scheduler
- Returns Future<T>
- Explicit parallelism

### async - Implicit Futures

**Use**: When a function naturally returns a Future

```wyn
// Mark function as async
async fn fetch_data(url: string) -> Data {
    let response = http_get(url).await();
    return parse(response);
}

// Calling async function returns Future automatically
let future = fetch_data("http://api.com");
let data = future.await();
```

**Characteristics**:
- Marks function as returning Future
- Allows await inside
- No explicit spawn needed
- Implicit Future wrapping

## Use Cases

### Use spawn When:

1. **Explicit parallelism needed**
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

3. **Spawning regular functions**
```wyn
fn regular_function() -> int { return 42; }
spawn regular_function();  // Works!
```

### Use async When:

1. **Function naturally async**
```wyn
async fn fetch_user(id: int) -> User {
    let data = db_query(id).await();
    return parse(data);
}
```

2. **Need to await inside**
```wyn
async fn process_pipeline() -> Result {
    let data = fetch_data().await();
    let processed = transform(data).await();
    let saved = save(processed).await();
    return saved;
}
```

3. **Composing async operations**
```wyn
async fn complex_operation() -> Data {
    let a = operation1().await();
    let b = operation2(a).await();
    let c = operation3(b).await();
    return c;
}
```

## Comparison with Other Languages

### JavaScript
```javascript
// Has both concepts
async function fetchData() {  // async marks function
    return await fetch(url);
}

// But spawn is implicit (Promise.resolve)
Promise.resolve().then(() => compute());  // Like spawn
```

### Rust
```rust
// Has both
async fn fetch_data() -> Data {  // async marks function
    http_get(url).await
}

tokio::spawn(async {  // spawn creates task
    fetch_data().await
});
```

### Go
```go
// Only has spawn (go keyword)
go fetchData()  // No async keyword needed
```

### Wyn (Proposed)
```wyn
// Both available
async fn fetch_data() -> Data {  // async marks function
    http_get(url).await()
}

spawn fetch_data()  // spawn creates task
```

## Recommendation

**Keep both keywords with clear purposes**:

1. **spawn** = "Create a new parallel task NOW"
   - Explicit parallelism
   - Works with any function
   - Like Go's `go`

2. **async** = "This function returns a Future"
   - Marks async functions
   - Enables await inside
   - Like Rust's `async fn`

## Example: Using Both Together

```wyn
// async function (returns Future)
async fn fetch_user(id: int) -> User {
    let data = db_query(id).await();
    return parse(data);
}

// Regular function
fn process_user(user: User) -> Result {
    return validate(user);
}

fn main() -> int {
    // Spawn async function
    let f1 = spawn fetch_user(1);
    
    // Spawn regular function
    let f2 = spawn process_user(default_user());
    
    // Wait for results
    let user = f1.await();
    let result = f2.await();
    
    return 0;
}
```

## Current Implementation Status

| Feature | Status | Notes |
|---------|--------|-------|
| `spawn` keyword | ✅ Working | Creates tasks |
| `spawn` returns Future | ✅ Working | For functions with returns |
| `async` keyword | ⏳ Partial | Token exists, not functional |
| `async fn` syntax | ❌ Not implemented | Would mark functions |
| `.await()` method | ⏳ Partial | Needs Future expression support |
| `await` inside async | ❌ Not implemented | Needs async context |

## Summary

**Current**: Wyn has `spawn` (working) and `async` token (not functional)

**Proposed**: 
- `spawn` = Create parallel tasks (like Go's `go`)
- `async` = Mark functions that return Futures (like Rust's `async fn`)

**They serve different purposes and can work together!**
