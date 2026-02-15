# spawn vs async - Important Distinction

## TL;DR

**No, they're different!**

- **async** = Marks a function that *can* be awaited (function modifier)
- **spawn** = Actually creates a new concurrent task (execution command)

## The Difference

### async - Function Marker

**Purpose**: Declares that a function returns a Future and can use await inside

```javascript
// JavaScript
async function fetchData() {  // async marks the function
    let response = await fetch(url);
    return response;
}
```

```rust
// Rust
async fn fetch_data() -> Data {  // async marks the function
    let response = http_get(url).await;
    response
}
```

**Key point**: `async` doesn't create a task! It just marks the function.

### spawn - Task Creator

**Purpose**: Actually creates and schedules a new concurrent task

```go
// Go
go fetchData()  // "go" spawns a new goroutine
```

```rust
// Rust
tokio::spawn(async {  // spawn creates a new task
    fetch_data().await
})
```

```wyn
// Wyn
spawn fetchData()  // spawn creates a new task
```

**Key point**: `spawn` actually creates parallelism!

## Side-by-Side Comparison

### JavaScript (async/await)

```javascript
// async marks the function
async function compute(n) {
    return n * n;
}

// Calling async function returns a Promise (doesn't run yet!)
let promise = compute(10);

// await waits for it
let result = await promise;
```

**No parallelism created!** The function doesn't run until you await it.

### Wyn (spawn/await)

```wyn
// Regular function
fn compute(n: int) -> int {
    return n * n;
}

// spawn creates a task (runs immediately!)
let future = spawn compute(10);

// await waits for it
let result = future.await();
```

**Parallelism created!** The task starts running immediately on a worker thread.

## Real Difference in Action

### JavaScript - No Parallelism Without Explicit Promise.all

```javascript
// These run SEQUENTIALLY (one after another)
async function main() {
    let r1 = await compute(10);  // Wait for first
    let r2 = await compute(20);  // Then wait for second
    let r3 = await compute(30);  // Then wait for third
    // Total: 300ms
}

// Need Promise.all for parallelism
async function mainParallel() {
    let promises = [
        compute(10),
        compute(20),
        compute(30)
    ];
    let results = await Promise.all(promises);  // Now parallel
    // Total: 100ms
}
```

### Wyn - Parallelism by Default

```wyn
// These run IN PARALLEL automatically
fn main() -> int {
    let f1 = spawn compute(10);  // Starts immediately
    let f2 = spawn compute(20);  // Starts immediately
    let f3 = spawn compute(30);  // Starts immediately
    
    let r1 = f1.await();  // Wait for results
    let r2 = f2.await();
    let r3 = f3.await();
    // Total: 100ms (parallel by default!)
}
```

## What async Really Does

In languages with `async`:

```javascript
// This:
async function foo() {
    return 42;
}

// Is sugar for:
function foo() {
    return Promise.resolve(42);
}
```

The `async` keyword just:
1. Makes the function return a Promise/Future
2. Allows using `await` inside
3. **Doesn't create any tasks or threads!**

## What spawn Really Does

```wyn
// This:
spawn compute(10)

// Actually does:
// 1. Create a new task
// 2. Schedule it on a worker thread
// 3. Return a Future
// 4. Task runs immediately in parallel
```

## Equivalent Concepts Across Languages

| Wyn | JavaScript | Rust | Go | Purpose |
|-----|------------|------|-----|---------|
| `spawn` | `Promise.resolve().then()` | `tokio::spawn()` | `go` | Create task |
| `.await()` | `await` | `.await` | N/A | Wait for result |
| N/A | `async` | `async fn` | N/A | Mark function |

## Why Wyn Uses spawn Instead of async

**Clarity**: `spawn` makes it explicit that you're creating a new concurrent task.

```wyn
// Clear: "I'm spawning a new task"
spawn compute(10)
```

vs

```javascript
// Unclear: "Am I creating a task? Or just marking a function?"
async function compute(n) { ... }
```

**Simplicity**: You don't need to mark functions as `async`. Any function can be spawned.

```wyn
// Any function can be spawned
fn regular_function() -> int { return 42; }
spawn regular_function()  // Works!
```

vs

```javascript
// Must mark function as async first
function regular_function() { return 42; }
await regular_function()  // Error! Not async
```

**Explicit Parallelism**: It's clear when parallelism is created.

```wyn
spawn task1()  // Parallel
spawn task2()  // Parallel
spawn task3()  // Parallel
```

vs

```javascript
async function task1() {}  // Not parallel yet
async function task2() {}  // Not parallel yet
async function task3() {}  // Not parallel yet

// Need to explicitly create parallelism
Promise.all([task1(), task2(), task3()])
```

## Summary

| Feature | async (JS/Rust) | spawn (Wyn/Go) |
|---------|-----------------|----------------|
| **What it is** | Function modifier | Task creator |
| **Creates parallelism** | No | Yes |
| **When it runs** | When awaited | Immediately |
| **Returns** | Promise/Future | Future |
| **Use case** | Mark async functions | Create concurrent tasks |

**Bottom line**: 
- `async` = "This function can be awaited" (passive)
- `spawn` = "Create a new task NOW" (active)

They serve different purposes! Wyn's `spawn` is more like Go's `go` or Rust's `tokio::spawn()`, not like JavaScript's `async`.
