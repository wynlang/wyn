# Wyn Programming Language

**Status:** Production Ready ✓  
**Type:** A modern systems programming language where everything is an object  
**Version:** See [VERSION](VERSION) file

---

## Quick Start

### Build

```bash
cd wyn
make
```

### Hello World

```wyn
fn main() -> int {
    print("Hello, World!\n");
    return 0;
}
```

```bash
./wyn hello.wyn
./hello.wyn.out
```

---

## Features

### Everything is an Object

```wyn
42.abs()              // Methods on integers
"hello".upper()       // Methods on strings
[1,2,3].len()         // Methods on arrays
{"a": 1}.get("a")     // Methods on hashmaps
```

### Clean Collection Syntax

```wyn
var arr = [1, 2, 3];                    // Array
var hmap = {"key": 10, "key2": 20};     // HashMap
var hset = {:"item1", "item2"};         // HashSet
```

### Simple Variable Declaration

```wyn
var mutable = 10;      // Mutable
const immutable = 20;  // Immutable
```

### Core Features

- **Type aliases:** `type UserId = int`
- **Extension methods:** `impl Point { fn sum(self) -> int }`
- **String interpolation:** `"Hello {name}!"`
- **Pattern matching:** Exhaustive enum matching
- **Generics:** `Option<T>`, `Result<T, E>`
- **Async/await:** Future-based concurrency
- **Memory safety:** Automatic Reference Counting (ARC)
- **150+ methods:** Across all primitive and collection types

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
    var alice_score = scores.get("alice");
    
    print(alice_score);  // 95
    
    return 0;
}
```

---

## Documentation

- **User Guide:** See `docs/` directory
- **Examples:** See `examples/` directory (21 examples)
- **Changelog:** See [CHANGELOG.md](CHANGELOG.md)

---

## What's New

See [CHANGELOG.md](CHANGELOG.md) for version history and release notes.

### Latest Features

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

## Project Structure

```
wyn/
├── src/           # Compiler source code
├── examples/      # 21 example programs
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

## License

See LICENSE file for details.

---

## Links

- **Changelog:** `CHANGELOG.md`

---

**Wyn - A modern systems programming language where everything is an object.**
