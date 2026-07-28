# Wyn v1.7.0 Benchmarks

All benchmarks run on the same machine. Numbers are median of 5 runs.
Source code included - reproduce with `./benchmarks/run.sh`.

## Results (Apple M4, macOS 15)

### Fibonacci(35) - Recursive

| Language | Time | Memory |
|----------|------|--------|
| Wyn (--release) | 60 ms | 1.4 MB |
| Go 1.24 | 40 ms | 3.8 MB |
| Python 3.12 | ~3,200 ms | ~9 MB |

Wyn is ~1.5× slower than Go on raw compute, 60× faster than Python.

### Binary Size - Hello World

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

### HTTP throughput (Apple M3 Pro, 12 cores, loopback)

| Scenario | Throughput |
|----------|------------|
| HTTP/1.1 keep-alive, c=100 | ~23,700 req/s, 0 failed |
| HTTP/1.1 keep-alive, c=200 | ~22,900 req/s, 0 failed |
| Connection per request, c=200 | ~6,600 req/s, 0 failed |

Regenerate with `./benchmarks/http_load.sh`. Two caveats the script prints and
the docs repeat, because both have produced misleading numbers before:

- **Warm up, or you measure pool startup.** The scheduler's workers spin up
  lazily; the same config reads ~15,000 req/s cold and ~23,000 warm. The harness
  discards a warmup run per configuration.
- **Keep-alive needs a handler that loops.** `while true { req =
  web.read_request(conn) ... }`. A handler that reads once and returns cannot
  serve a second request on the connection, so it measures the ~6,600 row no
  matter what the client asks for.

## Running

```bash
cd wyn
./benchmarks/run.sh          # compute / size / spawn / startup
./benchmarks/http_load.sh    # HTTP throughput (add --quick for one config)
```

Requires: `wyn` built (`make`), optionally `go` for comparison. `http_load.sh`
uses `ab` when installed and falls back to a bundled python load generator
(whose numbers are NOT directly comparable to `ab`'s - the output says which ran).
Run it on an idle machine: a parallel build or test run roughly halves the
result, and the script warns when the load average looks busy.

## Files

| File | What it tests |
|------|---------------|
| `fib35.wyn` | Recursive function calls, integer arithmetic |
| `spawn_10k.wyn` | Task creation, scheduler throughput |
| `strings.wyn` | String methods, interpolation, allocation |
| `startup.wyn` | Minimal program - startup overhead |
| `binary_size.wyn` | Minimal binary footprint |
| `fib35.go` / `startup.go` | Go equivalents for comparison |
| `run.sh` | Automated benchmark runner |
| `http_load.sh` | HTTP req/s - the source of every published req/s figure |

Correctness of the HTTP path under concurrent load is a separate, always-on gate:
`tests/errors/run_http_server_load_test.sh` (run by `make test`) asserts that every
request completes and no fd leaks. It deliberately asserts completion rather than
a rate, so it is not flaky on a shared CI box.
