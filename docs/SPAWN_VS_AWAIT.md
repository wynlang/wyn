# async/await vs spawn - Key Differences

## Quick Answer

**spawn** = Create a new concurrent task (like Go's `go` keyword)  
**await** = Wait for an async operation to complete (like JavaScript's `await`)

They work **together**: spawn creates tasks, await waits for results.

## Detailed Comparison

### spawn - Task Creation

**Purpose**: Start a new concurrent task that runs in parallel

```wyn
// Spawn creates a new task
spawn compute(42);  // Returns immediately, task runs in background

// With return value
let future = spawn compute(42);  // Returns Future<int>
```

**Characteristics**:
- Creates a new task on the scheduler
- Returns immediately (non-blocking)
- Task runs in parallel on worker threads
- Returns a Future<T> if function has return value

**Similar to**:
- Go: `go func()`
- Rust: `tokio::spawn()`
- JavaScript: `Promise.resolve().then()`

### await - Waiting for Results

**Purpose**: Block until an async operation completes

```wyn
// Await waits for result
let future = spawn compute(42);
let result = future.await();  // Blocks here until ready
print(result);  // 1764
```

**Characteristics**:
- Blocks current execution
- Waits for Future to complete
- Returns the value from the Future
- Can only be used on Future<T>

**Similar to**:
- JavaScript: `await promise`
- Rust: `.await`
- C#: `await task`

## How They Work Together

### Pattern 1: Spawn and Forget
```wyn
fn log_event(msg: string) -> int {
    print(msg);
    return 0;
}

spawn log_event("Task started");  // Fire and forget
// Continues immediately, don't care about result
```

### Pattern 2: Spawn and Await
```wyn
fn compute(n: int) -> int {
    return n * n;
}

let future = spawn compute(10);
// Do other work here...
let result = future.await();  // Wait for result
print(result);  // 100
```

### Pattern 3: Parallel Execution
```wyn
// Spawn multiple tasks
let f1 = spawn compute(10);
let f2 = spawn compute(20);
let f3 = spawn compute(30);

// All running in parallel now...

// Collect results
let r1 = f1.await();  // 100
let r2 = f2.await();  // 400
let r3 = f3.await();  // 900
```

### Pattern 4: Concurrent Pipeline
```wyn
fn fetch_data(url: string) -> Data { ... }
fn process(data: Data) -> Result { ... }
fn save(result: Result) -> int { ... }

// Spawn first task
let data_future = spawn fetch_data("http://api.com");

// Wait for data
let data = data_future.await();

// Spawn processing
let result_future = spawn process(data);

// Wait for result
let result = result_future.await();

// Spawn save
spawn save(result);  // Fire and forget
```

## Key Differences

| Feature | spawn | await |
|---------|-------|-------|
| **Purpose** | Create task | Wait for result |
| **Blocking** | No (returns immediately) | Yes (blocks until ready) |
| **Returns** | Future<T> | T (the value) |
| **Parallelism** | Creates parallelism | Synchronizes |
| **Use case** | Start work | Get results |

## Analogy

Think of it like ordering food:

**spawn** = Placing an order (you get a receipt/Future)
```wyn
let order = spawn make_pizza("pepperoni");  // Order placed, returns receipt
// You can do other things now
```

**await** = Waiting for your order to be ready
```wyn
let pizza = order.await();  // Wait at counter until ready
eat(pizza);  // Now you have the pizza
```

## Current Wyn Implementation

### What Works Now ✅

```wyn
// Spawn creates tasks
spawn compute(10);  // Creates Future<int> internally

// Multiple spawns run in parallel
spawn task1();
spawn task2();
spawn task3();
```

### What's Coming ⏳

```wyn
// Expression syntax
let future = spawn compute(10);

// Await method
let result = future.await();

// Chaining
let result = spawn compute(10)
    .map(|x| x * 2)
    .await();
```

## async Functions (Future Enhancement)

In many languages, `async` marks a function that returns a Future:

```wyn
// Future syntax
async fn fetch_data(url: string) -> Data {
    let response = http_get(url).await();
    return parse(response);
}

// Calling async function returns Future
let future = fetch_data("http://api.com");
let data = future.await();
```

This is equivalent to:
```wyn
fn fetch_data(url: string) -> Future<Data> {
    return spawn {
        let response = http_get(url).await();
        return parse(response);
    };
}
```

## Real-World Example

### Without spawn/await (Sequential)
```wyn
fn main() -> int {
    let r1 = compute(10);   // Takes 100ms
    let r2 = compute(20);   // Takes 100ms
    let r3 = compute(30);   // Takes 100ms
    // Total: 300ms
    return r1 + r2 + r3;
}
```

### With spawn/await (Parallel)
```wyn
fn main() -> int {
    let f1 = spawn compute(10);  // Start all three
    let f2 = spawn compute(20);  // at the same time
    let f3 = spawn compute(30);
    
    let r1 = f1.await();  // Wait for results
    let r2 = f2.await();
    let r3 = f3.await();
    // Total: ~100ms (parallel execution)
    
    return r1 + r2 + r3;
}
```

## Summary

**spawn**:
- Creates a new concurrent task
- Returns immediately
- Gives you a Future<T>
- Enables parallelism

**await**:
- Waits for a Future to complete
- Blocks until ready
- Gives you the actual value T
- Synchronizes parallel work

**Together**: spawn creates parallelism, await collects results. They're complementary operations for concurrent programming!
