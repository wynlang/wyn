# Wyn Documentation

**Version 1.7.0**

Welcome to the Wyn programming language documentation.

## What is Wyn?

Wyn is a modern programming language that compiles to C, producing fast native binaries. It combines the simplicity of scripting languages with the performance of systems programming.

```wyn
fn main() -> int {
    println("Hello from Wyn!")
    return 0
}
```

## Quick Start

```bash
# Compile and run
cd wyn
./wyn run hello.wyn

# Compile only
./wyn hello.wyn

# Install a package
./wyn install /path/to/package
```

## Documentation

- [Language Guide](language-guide.md) — Complete syntax and feature reference
- [Standard Library](stdlib-reference.md) — All modules and methods
- [Getting Started](getting-started.md) — Installation and first program
- [Tutorial](tutorial.md) — Step-by-step learning path
- [Examples](examples.md) — Real-world code examples
- [Best Practices](best-practices.md) — Idiomatic Wyn patterns
- [FAQ](faq.md) — Common questions

## Key Features

- **Fast** — Compiles to C, runs at native speed
- **Simple** — Clean syntax, no semicolons, everything is an object
- **64-bit** — Native 64-bit integers, no overflow
- **Concurrent** — `spawn`/`await` with work-stealing scheduler
- **Complete stdlib** — File I/O, networking, JSON, regex, terminal UI, testing
- **Package manager** — `wyn install` from local paths or git

## Sample Apps

See [sample-apps/](../../sample-apps/) for 23 real-world programs including:
- System monitors (htop-lite, disk usage, process viewer)
- DevOps tools (Docker monitor, Git dashboard, service checker)
- CLI utilities (file browser, port scanner, log analyzer)
