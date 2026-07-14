# Wyn Programming Language

**1 language for everything — CLI, web, desktop, mobile, games.**

Compiles to native via C. 5 platforms. Zero dependencies. Now with a C FFI —
call any C library.

## Install

**macOS / Linux**
```bash
curl -fsSL https://wynlang.com/install.sh | sh
```

**Windows**
```powershell
irm https://wynlang.com/install.ps1 | iex
```

**From source**
```bash
make && ./wyn install
```

---

## Quick Start

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
// Variables — bare assignment just works (declares on first use)
name = "Wyn"
count = 0
count = count + 1

// ...or be explicit when you want to be
const version = 2              // immutable
var pi = 3.14                  // mutable

// Full string interpolation
arr = [10, 20, 30]
println("${name} v${version}")
println("${arr[1]}")           // 20
println("${name.upper()}")     // WYN

// Functions — braces or a one-line => body
fn double(x: int) -> int => x * 2
fn even(x: int) -> bool => x % 2 == 0

// Pipe operator
result = 5 |> double           // 10

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

// Closures and higher-order functions — (x) => expr arrow lambdas
nums = [1, 2, 3, 4, 5]
doubled = nums.map((x) => x * 2)
total = nums.reduce((a, b) => a + b, 0)
squares = [x * x for x in 1..=5]

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

// Result/Option types
fn divide(a: int, b: int) -> Result<int, string> {
    if b == 0 { return Err("division by zero") }
    return Ok(a / b)
}

// enum.to_string(), indexed for, string repeat, clean int? / Some
println(Shape.Circle.to_string())  // "Circle"
for i, v in ["a", "b", "c"] { println(i.to_string() + ":" + v) }
println("=" * 40)
var x: int? = Some(42)
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
  wyn run <file>                   Compile and run
  wyn check <file>                 Type-check without compiling
  wyn fmt <file>                   Format source file
  wyn test                         Run project tests
  wyn watch <file>                 Watch and auto-rebuild
  wyn repl                         Interactive REPL
  wyn bench <file>                 Benchmark with timing
  wyn doc <file>                   Generate documentation

Build:
  wyn build <file|dir>             Build binary
  wyn build <file|dir> --shared    Build shared library (.so/.dylib/.dll)
  wyn build <file|dir> --python    Build shared library + Python wrapper
  wyn cross <target> <file>        Cross-compile (linux/macos/windows/ios/android)
  wyn build-runtime                Precompile runtime for fast builds
  wyn clean                        Remove build artifacts

Packages:
  wyn init [name]                  Create new project
  wyn init [name] --api            REST API with SQLite
  wyn init [name] --web            Web app with HTML + JSON API
  wyn init [name] --cli            CLI tool with arg parsing
  wyn init [name] --lib wyn        Wyn package (installable)
  wyn init [name] --lib python     Python extension module
  wyn init [name] --lib node       Node.js native addon
  wyn init [name] --lib c          C shared library
  wyn pkg install <url>            Install a package
  wyn pkg list                     List installed packages
  wyn pkg search <query>           Search official packages

Tools:
  wyn lsp                          Start language server (for editors)
  wyn install                      Install wyn to system PATH
  wyn uninstall                    Remove wyn from system PATH
  wyn version                      Show version
  wyn help                         Show help

Flags:
  --fast                           Skip optimizations (fastest compile)
  --release                        Full optimizations (-O3)
  --debug                          Keep .c and .out artifacts
```

## Performance

- Hello world binary: 33KB (release), runs in <1ms
- Compilation: ~300ms with bundled TCC, ~220ms with system cc --release
- Spawn: ~3μs per coroutine (pooled 16KB stacks, work-stealing scheduler, kqueue/epoll I/O)
- Memory: 180 bytes/task
- 64-bit integers throughout

## Editor Support

- **VS Code**: [wynlang/vscode-wyn](https://github.com/wynlang/vscode-wyn) — syntax highlighting, all keywords and modules
- **Neovim**: [wynlang/nvim-wyn](https://github.com/wynlang/nvim-wyn) — syntax highlighting + LSP wiring
- **LSP**: `wyn lsp` — live diagnostics (via `wyn check`, never runs your code), completions, hover, go-to-definition, find references, rename

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
├── tests/          # Assertion suite (tests/expect + tests/regression) + more
├── examples/       # Example programs
├── Makefile        # Build system
├── README.md       # This file
└── CHANGELOG.md    # Version history
```

## Documentation

- [wynlang.com](https://wynlang.com) — docs, playground, packages
- [Official Packages](https://github.com/wynlang/awesome-wyn)

## License

MIT

---

**<https://wynlang.com>**
