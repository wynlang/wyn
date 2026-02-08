# Future API Design for Wyn (OOP Style)

## Syntax Design

### Basic Spawn with Future
```wyn
// Spawn returns a Future<T>
let future = spawn compute(42);

// Wait for result
let result = future.await();

// Or chain methods
let result = spawn compute(42).await();
```

### Multiple Futures
```wyn
// Spawn multiple tasks
let futures = [];
for i in 0..1000 {
    futures.push(spawn process_chunk(i));
}

// Collect all results
let results = futures.map(|f| f.await());

// Or use Future.all()
let results = Future.all(futures).await();
```

### Non-blocking Check
```wyn
let future = spawn long_task();

// Poll without blocking
if future.is_ready() {
    let result = future.get();  // Non-blocking get
}

// Or wait with timeout
match future.await_timeout(1000) {
    Some(result) => print(result),
    None => print("Timeout!")
}
```

### Chaining and Transformation
```wyn
// Map over future result
let future = spawn compute(42)
    .map(|x| x * 2)
    .map(|x| x + 1);

let result = future.await();  // (42 * 2) + 1 = 85

// Flat map for chaining async operations
let result = spawn fetch_data(url)
    .then(|data| spawn process(data))
    .await();
```

### Error Handling
```wyn
// Future<Result<T, E>>
let future = spawn risky_operation();

match future.await() {
    Ok(value) => print(value),
    Err(e) => print("Error: " + e)
}

// Or use combinators
let result = spawn risky_operation()
    .await()
    .unwrap_or(default_value);
```

### Parallel Patterns

#### Map-Reduce
```wyn
// Chunk data and process in parallel
let chunks = data.chunks(100);
let results = chunks
    .map(|chunk| spawn process(chunk))
    .collect::<Vec<Future<int>>>();

let total = Future.all(results)
    .await()
    .sum();
```

#### Race (first to complete)
```wyn
let f1 = spawn fetch_from_server1();
let f2 = spawn fetch_from_server2();
let f3 = spawn fetch_from_server3();

// Get first result
let result = Future.race([f1, f2, f3]).await();
```

#### Select (wait for any)
```wyn
let futures = [spawn task1(), spawn task2(), spawn task3()];

while !futures.is_empty() {
    let (result, index) = Future.select(futures).await();
    print("Task " + index + " completed: " + result);
    futures.remove(index);
}
```

## Type Signatures

```wyn
class Future<T> {
    // Wait for result (blocking)
    fn await(self) -> T;
    
    // Non-blocking check
    fn is_ready(self) -> bool;
    fn get(self) -> Option<T>;
    
    // Timeout
    fn await_timeout(self, ms: int) -> Option<T>;
    
    // Transformation
    fn map<U>(self, f: fn(T) -> U) -> Future<U>;
    fn then<U>(self, f: fn(T) -> Future<U>) -> Future<U>;
    
    // Combinators
    static fn all(futures: Vec<Future<T>>) -> Future<Vec<T>>;
    static fn race(futures: Vec<Future<T>>) -> Future<T>;
    static fn select(futures: Vec<Future<T>>) -> Future<(T, int)>;
}
```

## Examples

### Example 1: Parallel Fibonacci
```wyn
fn fib(n: int) -> int {
    if n < 2 { return n; }
    
    let f1 = spawn fib(n - 1);
    let f2 = spawn fib(n - 2);
    
    return f1.await() + f2.await();
}
```

### Example 2: Parallel Map
```wyn
fn parallel_map<T, U>(items: Vec<T>, f: fn(T) -> U) -> Vec<U> {
    let futures = items.map(|item| spawn f(item));
    return Future.all(futures).await();
}

// Usage
let numbers = [1, 2, 3, 4, 5];
let squares = parallel_map(numbers, |x| x * x);
```

### Example 3: Pipeline with Chaining
```wyn
let result = spawn fetch_data(url)
    .map(|data| parse_json(data))
    .map(|json| extract_field(json, "value"))
    .map(|value| value * 2)
    .await();
```

### Example 4: Chunked Processing
```wyn
fn process_large_dataset(data: Vec<int>) -> int {
    let chunk_size = 1000;
    let chunks = data.chunks(chunk_size);
    
    let futures = chunks.map(|chunk| {
        spawn chunk.sum()
    });
    
    return Future.all(futures)
        .await()
        .sum();
}
```

### Example 5: Timeout Pattern
```wyn
let future = spawn expensive_computation();

match future.await_timeout(5000) {
    Some(result) => {
        print("Completed: " + result);
    },
    None => {
        print("Timeout after 5 seconds");
        // Future is automatically cancelled
    }
}
```

## Implementation Notes

### Current C API
```c
Future* wyn_spawn_async(TaskFuncWithReturn func, void* arg);
void* future_get(Future* f);
int future_is_ready(Future* f);
void future_free(Future* f);
```

### Wyn Language Integration
```wyn
// Compiler generates:
// 1. Wrapper function for spawned code
// 2. Future allocation
// 3. Automatic cleanup (RAII style)

let future = spawn compute(42);
// Generates:
//   Future* _future = wyn_spawn_async(compute_wrapper, arg);
//   WynFuture wyn_future = { _future };

let result = future.await();
// Generates:
//   void* _result = future_get(wyn_future._ptr);
//   int result = *(int*)_result;
//   free(_result);
//   future_free(wyn_future._ptr);
```

## Syntax Sugar

### Async/Await Style
```wyn
// Traditional
let f = spawn compute(42);
let result = f.await();

// Sugar
let result = await spawn compute(42);

// Or even shorter
let result = await compute(42);  // spawn is implicit
```

### Parallel For
```wyn
// Explicit
let futures = [];
for i in 0..1000 {
    futures.push(spawn process(i));
}
let results = Future.all(futures).await();

// Sugar
let results = parallel for i in 0..1000 {
    process(i)
};
```

## Design Principles

1. **Chainable**: Methods return Future for fluent API
2. **Type-safe**: Future<T> preserves type information
3. **Ergonomic**: Common patterns have sugar syntax
4. **Zero-cost**: Compiles to efficient C calls
5. **RAII**: Automatic cleanup via destructors
6. **Composable**: Futures can be combined and transformed

## Next Steps

1. Implement Future wrapper class in Wyn
2. Add spawn expression to parser
3. Generate wrapper functions for spawned code
4. Implement Future methods (map, then, all, race)
5. Add async/await syntax sugar
6. Integrate with type system (Future<T>)
