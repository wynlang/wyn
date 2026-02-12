# Wyn Concurrency: Spawn Performance

## Architecture

Wyn uses an **M:N scheduler** — M lightweight tasks multiplexed onto N OS threads.

- **Per-processor work-stealing deques** (lock-free LIFO for owner, CAS-based steal)
- **Lock-free global queue** (atomic CAS stack for cross-thread spawns)
- **Spin-then-park** worker strategy: 32 spins → 8 yields → condvar sleep (500μs timeout)
- **Spin-then-yield await**: 128 spins with CPU hint → 8 yields → condvar (only for long tasks)
- **Slab allocator** for futures (zero malloc on hot path)
- **Pool allocator** for tasks (bulk 4K allocation)

## Benchmark Results

Run: `cd wyn && ./wyn run tests/benchmarks/spawn_benchmark.wyn`

### Speed

| Metric | Wyn | Go 1.24 |
|--------|-----|---------|
| Sequential spawn+await | **2-4 μs** | 1-3 μs |
| Parallel spawn+await (8 batch) | **1-4 μs** | 0.7-1.2 μs |
| 4-core speedup (CPU-bound) | **5x** | 3.5-4x |

### Memory

| Concurrent Tasks | Wyn RSS | Go RSS | Wyn bytes/task | Go bytes/task |
|-----------------|---------|--------|----------------|---------------|
| 10,000 | 1.7 MB | 25 MB | 176 | 2,641 |
| 100,000 | 15 MB | 261 MB | 159 | 2,735 |
| 1,000,000 | 153 MB | 2,636 MB | 160 | 2,765 |

**Wyn is 15x more memory efficient than Go goroutines.**

### Scaling

Memory scales **perfectly linearly**: 160 bytes/task at every scale from 10K to 1M. No growth, no fragmentation.

## Why Wyn Is More Memory Efficient

| | Wyn Spawn | Go Goroutine |
|---|-----------|-------------|
| Stack | Shares worker thread stack | 2 KB minimum (grows to 1 MB) |
| Metadata | 32 bytes (Task struct) | ~48 bytes (runtime.g) |
| Synchronization | 128 bytes (Future: mutex + condvar) | Integrated in runtime |
| **Total** | **~180 bytes** | **~2,700 bytes** |

Wyn tasks are **run-to-completion** — they execute on a worker thread's existing stack without allocating their own. Go goroutines need individual stacks because they support preemption and suspension.

## Tradeoffs

| Feature | Wyn | Go |
|---------|-----|-----|
| Preemptive scheduling | No (run-to-completion) | Yes |
| Stack per task | No (shared) | Yes (2KB-1MB) |
| Blocking I/O in task | Blocks worker thread | Automatically yields |
| Memory per task | 180 bytes | 2,700 bytes |
| Max concurrent tasks (8GB RAM) | ~44 million | ~3 million |

## Usage

```wyn
// Spawn a task
var future = spawn compute(1000000)

// Do other work...

// Await result
var result = await future

// Shared state between tasks
var counter = Task.value(0)
spawn worker(counter)
spawn worker(counter)
// Task.get(counter) reads the shared value

// Channels for message passing
var ch = Task.channel(100)
spawn producer(ch)
var val = Task.recv(ch)
```

## How to Reproduce

```bash
# Wyn benchmark
cd wyn
./wyn run tests/benchmarks/spawn_benchmark.wyn

# Go comparison (requires Go installed)
go run /tmp/bench_go_mem2.go
```
