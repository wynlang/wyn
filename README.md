# Wyn Programming Language

**1 language for everything — CLI, web, desktop, mobile, games.**

Wyn compiles to C, runs everywhere, and ships with 27 official packages — zero external dependencies.

---

## Quick Start

```bash
make                    # Build the compiler
./wyn install           # Install to PATH (optional)
```

```wyn
// hello.wyn — no main() needed
println("Hello, World!")
```

```bash
wyn run hello.wyn       # Compile and run
wyn run -e 'println("or inline")'
```

## Features

```wyn
// Variables and types
var name = "Wyn"
const version = 2
var pi = 3.14

// Full string interpolation
var arr = [10, 20, 30]
println("${name} v${version}")
println("${arr[1]}")           // 20
println("${name.upper()}")     // WYN

// Pipe operator
fn double(x: int) -> int { return x * 2 }
var result = 5 |> double       // 10

// Enums with data + destructuring match
enum Shape { Circle(float), Point }
var s = Shape.Circle(5.0)
var r = match s {
    Shape.Circle(radius) => radius
    Shape.Point => 0.0
}

// Structs with methods
struct User {
    name: string
    fn greet(self) -> string { return "Hi ${self.name}" }
}

// Closures and higher-order functions
var nums = [1, 2, 3, 4, 5]
var doubled = nums.map(fn(x) => x * 2)
var squares = [x * x for x in 1..=5]

// Spawn/await concurrency
var f1 = spawn compute(1000)
var f2 = spawn compute(2000)
var r1 = await f1

// Defer for cleanup
fn process() {
    var f = File.open("data.txt", "r")
    defer File.close(f)
    // file auto-closed on return
}

// Result/Option with ? operator
fn divide(a: int, b: int) -> ResultInt {
    if b == 0 { return Err("division by zero") }
    return Ok(a / b)
}

// Template engine for web
var html = Template.render("page.html", ctx)

// Deploy in one command
// wyn deploy prod
```

## Standard Library — 27 Modules

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

See [awesome-wyn](https://github.com/wynlang/awesome-wyn) for all official packages.

## CLI

```
Develop:
  wyn run <file>             Compile and run
  wyn check <file>           Type-check without compiling
  wyn fmt <file>             Format source file
  wyn test                   Run project tests
  wyn watch <file>           Watch and auto-rebuild
  wyn repl                   Interactive REPL
  wyn bench <file>           Benchmark with timing
  wyn doc <file>             Generate documentation

Build:
  wyn build <file|dir>             Build binary
  wyn build <file|dir> --shared    Build shared library (.so/.dylib/.dll)
  wyn build <file|dir> --python    Build shared library + Python wrapper
  wyn cross <target> <file>        Cross-compile (linux/macos/windows/ios/android)
  wyn build-runtime                Precompile runtime for fast builds
  wyn clean                        Remove build artifacts

Packages:
  wyn init [name]            Create new project
  wyn init [name] --api      REST API with SQLite
  wyn init [name] --web      Web app with HTML + JSON API
  wyn init [name] --cli      CLI tool with arg parsing
  wyn init [name] --lib wyn  Wyn package (installable)
  wyn init [name] --lib python  Python extension module
  wyn init [name] --lib node    Node.js native addon
  wyn init [name] --lib c       C shared library
  wyn pkg install <url>      Install a package
  wyn pkg list               List installed packages
  wyn pkg search <query>     Search official packages

Tools:
  wyn lsp                    Start language server (for editors)
  wyn install                Install wyn to system PATH
  wyn uninstall              Remove wyn from system PATH
  wyn version                Show version
  wyn help                   Show help

Flags:
  --fast                     Skip optimizations (fastest compile)
  --release                  Full optimizations (-O2)
  --debug                    Keep .c and .out artifacts
```

## Performance

- Hello world binary: 222KB, runs in 12ms
- Compilation: 1.4s with precompiled runtime
- Spawn: 2μs per task (matches Go goroutines)
- Memory: 180 bytes/task
- 64-bit integers throughout

## Editor Support

- **VS Code**: `vscode-wyn/` — syntax highlighting, all 27 modules
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
- [Official Packages](https://github.com/wynlang/awesome-wyn)
- [Best Practices](docs/best-practices.md)
- [Spawn Performance](docs/spawn-performance.md)
- [Examples](docs/examples.md)

## License

MIT

---

**<https://wynlang.com>**
