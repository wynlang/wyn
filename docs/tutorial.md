# Wyn Tutorial

A step-by-step guide to learning Wyn.

## 1. Variables and Types

```wyn
var name = "Wyn"           // string (mutable)
var age = 1                // int (64-bit)
var pi = 3.14159           // float
var debug = true           // bool
const VERSION = 42         // constant (immutable)
```

## 2. Functions

```wyn
fn add(a: int, b: int) -> int {
    return a + b
}

fn greet(name: string) {
    println("Hello, " + name + "!")
}

fn main() -> int {
    greet("World")
    println(add(2, 3).to_string())
    return 0
}
```

## 3. Control Flow

```wyn
// If/else
if score >= 90 {
    println("A")
} else if score >= 80 {
    println("B")
} else {
    println("C")
}

// For loops
for i in 0..10 {
    println(i.to_string())
}

// Array iteration
var names = ["Alice", "Bob", "Charlie"]
for name in names {
    println(name)
}

// While
var n = 0
while n < 5 {
    n = n + 1
}
```

## 4. Structs and Methods

```wyn
struct Point {
    x: int
    y: int

    fn mag_sq(self) -> int {
        return self.x * self.x + self.y * self.y
    }
}

fn main() -> int {
    var p = Point{x: 3, y: 4}
    println(p.mag_sq().to_string())  // 25
    p.x = 10                        // field mutation
    return 0
}
```

## 5. Enums and Pattern Matching

```wyn
enum Color { Red, Green, Blue }

fn describe(c: Color) -> string {
    match c {
        Color::Red => return "red"
        Color::Green => return "green"
        Color::Blue => return "blue"
    }
    return "unknown"
}
```

## 6. Error Handling

```wyn
fn divide(a: int, b: int) -> ResultInt {
    if b == 0 { return Err("division by zero") }
    return Ok(a / b)
}

fn main() -> int {
    var result = divide(10, 3)
    if result.is_ok() {
        println("Result: " + result.unwrap().to_string())
    }
    if result.is_err() {
        println("Error: " + result.unwrap_err())
    }
    return 0
}
```

## 7. File I/O

```wyn
fn main() -> int {
    // Write
    File.write("/tmp/hello.txt", "Hello from Wyn!\n")

    // Read
    var content = File.read("/tmp/hello.txt")
    println(content)

    // Check existence
    if File.exists("/tmp/hello.txt") {
        File.delete("/tmp/hello.txt")
    }
    return 0
}
```

## 8. Concurrency

```wyn
fn compute(n: int) -> int {
    var sum = 0
    for i in 0..n { sum = sum + i }
    return sum
}

fn main() -> int {
    // Spawn parallel work
    var f1 = spawn compute(1000)
    var f2 = spawn compute(2000)

    // Await results
    var r1 = await f1
    var r2 = await f2
    println("Total: " + (r1 + r2).to_string())

    // Shared state between spawns
    var counter = Task.value(0)
    spawn worker(counter)
    spawn worker(counter)
    // Task.get(counter) reads the shared value
    return 0
}
```

## 9. Testing

```wyn
fn add(a: int, b: int) -> int { return a + b }

fn main() -> int {
    Test.init("Math Tests")

    Test.assert_eq_int(add(2, 3), 5, "2 + 3 = 5")
    Test.assert_eq_int(add(0, 0), 0, "0 + 0 = 0")
    Test.assert(add(1, 1) > 0, "positive result")

    Test.summary()
    return 0
}
```

## 10. Packages

Create `wyn.toml`:
```toml
[package]
name = "myapp"
version = "0.1.0"

[dependencies]
utils = "/path/to/utils"
```

Install and use:
```bash
./wyn install                    # install from wyn.toml
./wyn install /path/to/package   # install single package
./wyn install https://github.com/user/repo.git  # from git
```

```wyn
import utils
// use functions from utils module
```

## Python Libraries

Wyn can compile your code into shared libraries that Python can call directly:

```wyn
// mathlib.wyn
fn add(a: int, b: int) -> int { return a + b }
fn greet(name: string) -> string { return "Hello, " + name + "!" }
```

```bash
wyn run mathlib.wyn --python
# ✓ Built shared library: libmathlib.dylib
# ✓ Generated Python wrapper: mathlib.py
```

```python
from mathlib import add, greet
print(add(2, 3))        # 5
print(greet("World"))    # Hello, World!
```

The generated wrapper includes Python type hints and handles string encoding automatically. See [Python Libraries](python-libraries.md) for the full guide.

## Next Steps

- [Language Guide](language-guide.md) — Complete reference
- [Standard Library](stdlib-reference.md) — All modules
- [Python Libraries](python-libraries.md) — Build Python packages with Wyn
- [Sample Apps](../../sample-apps/) — 31 real-world programs
