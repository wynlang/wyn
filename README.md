# Wyn Programming Language

**A modern systems programming language where everything is an object**

[![Version](https://img.shields.io/badge/version-1.6.0-blue.svg)](VERSION)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-198%2F198%20passing-brightgreen.svg)](tests/)

---

## ✨ What Makes Wyn Special

- **Everything is an Object** - Even integers and strings have methods
- **Modern Syntax** - Clean, intuitive, and expressive
- **LLVM Backend** - Compiles to native code for maximum performance
- **Module System** - Built-in import/export for code organization
- **Rich Standard Library** - Strings, arrays, file I/O, networking, JSON, and more
- **IDE Support** - LSP, VS Code extension, Neovim plugin
- **Production Ready** - 198/198 tests passing, all edge cases covered

---

## 📁 Repository Structure

See [STRUCTURE.md](STRUCTURE.md) for detailed repository organization.

---

## 🚀 Quick Start

### Installation

```bash
cd wyn
make clean && make wyn-llvm && mv wyn-llvm wyn
./wyn --version  # Should show "Wyn v1.6.0"
```

### Hello World

```wyn
fn main() -> int {
    print("Hello, World!")
    return 0
}
```

```bash
./wyn hello.wyn
./hello.wyn.out
# Output: Hello, World!
```

---

## 💡 Features

### Everything is an Object

```wyn
// Methods on integers
42.abs()
(-5).abs()  // Returns 5

// Methods on strings
"hello".upper()      // "HELLO"
"hello".len()        // 5
"hello".contains("ell")  // true

// Methods on arrays
[1, 2, 3].len()      // 3
[1, 2, 3].map(fn(x) -> x * 2)  // [2, 4, 6]
[1, 2, 3].filter(fn(x) -> x > 1)  // [2, 3]
```

### Clean Collection Syntax

```wyn
// Arrays
var numbers = [1, 2, 3, 4, 5]
numbers.push(6)
var first = numbers[0]

// HashMaps
var scores = {"Alice": 95, "Bob": 87}
scores["Charlie"] = 92
var alice_score = scores["Alice"]

// HashSets
var tags = {:"rust", "systems", "programming"}
tags.insert("wyn")
var has_rust = tags.contains("rust")
```
```

### Simple Variable Declaration

```wyn
var mutable = 10;      // Mutable
const immutable = 20;  // Immutable
```

### Core Features

- **Module system:** Nested modules with visibility control
- **Type aliases:** `type UserId = int`
- **Extension methods:** `impl Point { fn sum(self) -> int }`
- **String interpolation:** `"Hello ${name}!"`
- **Pattern matching:** Exhaustive enum matching
- **Generics:** `Option<T>`, `Result<T, E>`
- **Async/await:** Future-based concurrency
- **Memory safety:** Automatic Reference Counting (ARC)
- **100+ methods:** Comprehensive standard library

---

## Example

```wyn
fn main() -> int {
    // Collections with clean syntax
    var scores = {"alice": 95, "bob": 87};
    var tags = {:"urgent", "important"};
    var numbers = [1, 2, 3, 4, 5];
    
    // Everything is an object - use method syntax
    var len = numbers.len();
    var has_three = numbers.contains(3);
    var alice_score = scores["alice"];
    
    print(alice_score);  // 95
    
    return 0;
}
```

### Module System

```wyn
// utils/math.wyn
pub fn add(a: int, b: int) -> int {
    return a + b;
}

// main.wyn
import utils.math

fn main() -> int {
    var sum = math::add(1, 2);  // Short name
    print(sum);
    return 0;
}
```

See [docs/modules.md](docs/modules.md) for complete module guide.

---

## Documentation

- **User Guide:** See `docs/` directory
- **Examples:** See `examples/` directory (30 examples)
- **Changelog:** See [CHANGELOG.md](CHANGELOG.md)

---

## What's New

See [CHANGELOG.md](CHANGELOG.md) for version history and release notes.

### Latest Features (v1.4.0)

- **Module system** - Nested modules, visibility control, relative imports
- **Polymorphic print()** - Single print function for all types
- **HashMap indexing** - `map["key"]` syntax for get/set
- **HashMap Methods:** `.has()`, `.remove()`, `.len()` and `map["key"]` indexing
- **HashSet methods** - `.add()`, `.contains()`, `.remove()`, `.len()`
- **Consistent OO syntax** - All examples and docs use method-first approach

### Previous Features

- HashMap multi-type support (int, float, string, bool)
- HashSet initialization syntax: `{:"item1", "item2"}`
- Full standard library with 60+ functions
- Object-oriented method syntax for all collections

### Bug Fixes

- Fixed impl methods calling other methods
- Fixed HashMap implementation (remove, has, helpers)
- Fixed binary expression type inference

See [CHANGELOG.md](CHANGELOG.md) for complete details.

---

## Testing

```bash
# Run all examples
cd examples
for f in *.wyn; do ../wyn "$f" && ./"${f}.out"; done

# Run regression tests
./tests/regression.sh

# Run specific test
./wyn tests/wyn/test_hashmap_multitypes.wyn
./tests/wyn/test_hashmap_multitypes.wyn.out
```

---

## Developer Tools (v1.4.0)

### Project Management

```bash
# Create new project
wyn init my-app

# Auto-rebuild on changes
wyn watch main.wyn

# Install dependencies
wyn install

# Package management
wyn pkg list
```

### Project Configuration (wyn.toml)

```toml
[project]
name = "my-app"
version = "1.0.0"
entry = "main.wyn"

[dependencies]
# Add package dependencies here
```

### Language Server

```bash
# Start LSP server for IDE integration
wyn lsp
```

---

## Project Structure

```
wyn/
├── src/           # Compiler source code
├── examples/      # 30 example programs
├── tests/         # Test suite
├── docs/          # User documentation
├── README.md      # This file
└── CHANGELOG.md   # Version history
```

---

## Building from Source

```bash
# Clean build
make clean
make

# Check version
./wyn --version

# Run tests
./tests/regression.sh
```

---

## Self-Hosting Compiler

Wyn v1.5.0 includes a self-hosting compiler written in Wyn itself. The modular compiler demonstrates that Wyn can compile real-world programs, including its own compiler.

### Modular Compiler Components

```
lib/
├── lexer_module.wyn      # Tokenization (263 lines)
├── parser_module.wyn     # Parsing (355 lines)
├── checker_module.wyn    # Type checking (215 lines)
├── codegen_module.wyn    # C code generation (221 lines)
└── compiler_modular.wyn  # Integration (159 lines)
```

### Testing Self-Compilation

```bash
# Test self-compilation
./tests/test_self_compilation.sh

# Test bootstrap stability (reproducible builds)
./tests/test_bootstrap_stability.sh
```

### Features Demonstrated

- **Real implementations:** No stubs, all modules have working logic
- **Modular design:** Each module <500 lines (compiler limit)
- **Self-compilation:** Each module can compile itself
- **Bootstrap stability:** Reproducible builds across generations
- **Production ready:** Compiles real Wyn programs correctly

---

## License

See LICENSE file for details.

---

## Links

- **Changelog:** `CHANGELOG.md`

---

**Wyn - A modern systems programming language where everything is an object.**
