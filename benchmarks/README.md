# Wyn v1.7.0 Benchmarks

All benchmarks run on the same machine. Numbers are median of 5 runs.
Source code included — reproduce with `./benchmarks/run.sh`.

## Results (Apple M4, macOS 15)

### Fibonacci(35) — Recursive

| Language | Time | Memory |
|----------|------|--------|
| Wyn (--release) | 60 ms | 1.4 MB |
| Go 1.24 | 40 ms | 3.8 MB |
| Python 3.12 | ~3,200 ms | ~9 MB |

Wyn is ~1.5× slower than Go on raw compute, 60× faster than Python.

### Binary Size — Hello World

| Language | Size |
|----------|------|
| **Wyn** | **256 KB** |
| Go | 2,300 KB |

Wyn produces 9× smaller binaries.

### Startup Time

| Language | Time |
|----------|------|
| Wyn | ~10 ms |
| Go | ~10 ms |
| Python | ~45 ms |

### Compile Time

| Mode | Time |
|------|------|
| `wyn run` (TCC) | ~260 ms |
| `wyn run --release` (system cc) | ~930 ms |
| Go build | ~300 ms |
| Rust (cargo build) | 5–30 s |

### Spawn 10K Tasks

| Language | Time | Memory |
|----------|------|--------|
| Wyn | 20 ms | 1.8 MB |
| Go (goroutines) | 18 ms | 2.1 MB |

### 1M Concurrent Tasks (from PERFORMANCE.md)

| Language | Memory |
|----------|--------|
| **Wyn** | **153 MB** |
| Go | 2,636 MB |

Wyn uses 17× less memory at scale.

## Running

```bash
cd wyn
./benchmarks/run.sh
```

Requires: `wyn` built (`make`), optionally `go` for comparison.

## Files

| File | What it tests |
|------|---------------|
| `fib35.wyn` | Recursive function calls, integer arithmetic |
| `spawn_10k.wyn` | Task creation, scheduler throughput |
| `strings.wyn` | String methods, interpolation, allocation |
| `startup.wyn` | Minimal program — startup overhead |
| `binary_size.wyn` | Minimal binary footprint |
| `fib35.go` / `startup.go` | Go equivalents for comparison |
| `run.sh` | Automated benchmark runner |
