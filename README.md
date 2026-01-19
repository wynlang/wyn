# Wyn Programming Language

**Version:** 1.2.1  
**Status:** Production Ready  
**Type:** Systems programming language with "everything is an object"

---

## Quick Start

### Installation
```bash
make wyn
```

### Hello World
```bash
cat > hello.wyn << 'EOF'
fn main() -> int {
    println("Hello, World!")
    return 0
}
EOF

./wyn hello.wyn
./hello.wyn.out
```

---

## Features

- **Everything is an object:** `42.abs()`, `"hello".upper()`, `16.0.sqrt()`
- **Type aliases:** `type UserId = int`
- **Extension methods:** `impl Point { fn sum(self) -> int }`
- **Nil coalescing:** `value ?? default`
- **String interpolation:** `"Hello {name}!"`
- **Method chaining:** `"  test  ".trim().upper().len()`
- **Pattern matching:** Exhaustive enum matching
- **Generics:** `Option<T>`, `Result<T, E>`
- **Async/await:** Future-based concurrency
- **Memory safety:** Automatic Reference Counting (ARC)
- **150 methods:** Across all primitive and collection types

---

## Example

```wyn
// Type aliases
type UserId = int

// Structs with methods
struct Point {
    x: int,
    y: int
}

impl Point {
    fn sum(self) -> int {
        return self.x + self.y
    }
}

fn main() -> int {
    // Type alias
    var id: UserId = 42
    
    // Extension method
    var p = Point { x: 3, y: 4 }
    var result = p.sum()
    
    // String methods
    var text = "  hello  ".trim().upper()
    
    // Nil coalescing
    var value = some(10)
    var final = value ?? 0
    
    // Method chaining
    var len = "hello world".upper().len()
    
    return result + final + len
}
```

---

## Documentation

- **Language Guide:** See `docs/LANGUAGE.md`
- **API Reference:** See `docs/API.md`
- **Examples:** See `examples/` directory

---

## Building

```bash
# Build compiler
make wyn

# Run tests
make test

# Clean build
make clean
```

---

## Examples

All 21 examples in `examples/` directory:
- `01_hello_world.wyn` - Basic hello world
- `02_functions.wyn` - Function definitions
- `03_generics.wyn` - Generic types
- `04_async_await.wyn` - Async programming
- `11_methods.wyn` - Method chaining
- And 16 more...

---

## Performance

- **Compilation:** <100ms for small programs
- **Runtime:** Native C performance
- **Binary Size:** 486KB compiler, ~50KB hello world

---

## Status

- ✓ 21/21 examples working
- ✓ 150 methods implemented
- ✓ 0 stubs in code
- ✓ Production ready

---

## License

See LICENSE file.

---

**Wyn v1.2.1 - A modern systems programming language where everything is an object.**
