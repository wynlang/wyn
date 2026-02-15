# Wyn Programming Language

**1 language for everything — CLI, web, desktop, mobile, games.**

Wyn compiles to C, runs everywhere, and ships with 26 stdlib modules out of the box.

---

## Quick Start

```bash
make                    # Build the compiler
./wyn install           # Install to PATH (optional)
wyn run hello.wyn       # Compile and run
```

```wyn
fn main() -> int {
    println("Hello, World!")
    return 0
}
```

## Features

```wyn
// Variables and types
var name = "Wyn"
const version = 2
var pi = 3.14

// String interpolation
println("${name} v${version}")

// Structs with methods
struct Point { x: int, y: int }
impl Point {
    fn distance(self) -> int { return self.x + self.y }
}

// Enums with pattern matching
enum Color { Red, Green, Blue }
var c = Color.Green
var label = match c {
    Color.Red => "red"
    Color.Green => "green"
    Color.Blue => "blue"
}

// Traits with dynamic dispatch
trait Shape {
    fn area(self) -> int
}
struct Square { side: int }
impl Shape for Square {
    fn area(self) -> int { return self.side * self.side }
}
fn describe(s: Shape) -> int { return s.area() }

// Closures and higher-order functions
var nums = [1, 2, 3, 4, 5]
var evens = nums.filter(fn(x: int) -> int { return x % 2 == 0 })
var doubled = nums.map(fn(x: int) -> int { return x * 2 })

// Spawn/await concurrency (M:N scheduler, 2μs spawn)
var f1 = spawn compute(1000)
var f2 = spawn compute(2000)
var r1 = await f1
var r2 = await f2

// Result/Option types with ? operator
fn divide(a: int, b: int) -> ResultInt {
    if b == 0 { return Err("division by zero") }
    return Ok(a / b)
}
```

## Standard Library — 26 Modules

| Category | Modules |
|----------|---------|
| Core | String (29 methods), Array (19), HashMap (12), Math (20) |
| Data | Json (16), Csv (7), Encoding (4), Regex (5) |
| I/O | File (22), System (5), Terminal (4), Net (6), Http (9), Db (9) |
| Concurrency | Task (7), StringBuilder (7) |
| Crypto | Crypto (4), Uuid (1) |
| Platform | Os (6), Path (4), DateTime (16), Process (2), Log (5), Url (2) |
| GUI | Gui (30+, SDL2), Audio (5, SDL2_mixer) |
| Testing | Test (12) |

See [docs/stdlib-reference.md](docs/stdlib-reference.md) for the full API.

## CLI

```
wyn run <file>          Compile and run
wyn run <file> --shared Build as shared library (.so/.dylib/.dll)
wyn run <file> --python Build shared library + Python wrapper
wyn check <file>        Type-check without compiling
wyn test                Run project tests
wyn repl                Interactive REPL
wyn bench <file>        Benchmark with timing
wyn doc <file>          Generate documentation
wyn build <dir>         Build project
wyn cross <target> <f>  Cross-compile (linux/macos/windows/ios/android)
wyn init [name]         Create new project
wyn pkg install <name>  Install package
wyn lsp                 Start language server
wyn install             Install wyn to system PATH
```

## Performance

- Spawn: 2μs (matches Go goroutines)
- Memory: 180 bytes/task (15x better than Go)
- Compilation: 62ms cached, 1.1s first run
- 64-bit integers throughout (long long)

## Editor Support

- **VS Code**: `vscode-wyn/` — syntax highlighting, all 26 modules
- **Neovim**: `nvim-wyn/` — syntax highlighting, all keywords
- **LSP**: `wyn lsp` — completions, hover, go-to-definition, rename, format

## Building

```bash
make                    # Build compiler
make runtime            # Precompile runtime library
wyn build-runtime       # Same, via CLI
```

## Cross-Compilation

```bash
wyn cross linux app.wyn     # Linux x86_64
wyn cross macos app.wyn     # macOS
wyn cross windows app.wyn   # Windows (needs mingw)
wyn cross ios app.wyn       # iOS arm64
wyn cross android app.wyn   # Android arm64 (needs NDK)
```

## Project Structure

```
wyn/
├── src/            # Compiler source (C)
├── runtime/        # Precompiled runtime library
├── tests/          # Test suite (268+ assertions)
├── docs/           # User documentation
├── Makefile        # Build system
├── README.md       # This file
└── CHANGELOG.md    # Version history
```

## Documentation

- [Language Tutorial](docs/tutorial.md)
- [Standard Library Reference](docs/stdlib-reference.md)
- [Best Practices](docs/best-practices.md)
- [Spawn Performance](docs/spawn-performance.md)
- [Examples](docs/examples.md)

## License

MIT

---

**https://wynlang.com**
