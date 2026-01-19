# Wyn Language Guide

![Version](https://img.shields.io/badge/version-1.2.2-blue.svg)
**Latest: v1.2.2**

Complete reference for the Wyn programming language syntax and features.

## Table of Contents

1. [Basic Syntax](#basic-syntax)
2. [Types](#types)
3. [Functions](#functions)
4. [Control Flow](#control-flow)
5. [Structs](#structs)
6. [Generics](#generics)
7. [Async/Await](#asyncawait)
8. [Modules](#modules)
9. [Memory Management](#memory-management)
10. [Best Practices](#best-practices)

## Basic Syntax

### Comments
```wyn
// Single line comment
```

### Variables
```wyn
let x = 42;              // Immutable by default
let name = "Alice";      // String
let flag = true;         // Boolean
```

### Type Annotations
```wyn
let num: int = 42;
let pi: float = 3.14;
let text: string = "hello";
let flag: bool = true;
```

## Types

### Primitive Types

| Type | Description | Example |
|------|-------------|---------|
| `int` | Integer | `42` |
| `float` | Floating point | `3.14` |
| `bool` | Boolean | `true`, `false` |
| `string` | String | `"hello"` |

### Composite Types

- **`struct`** - User-defined structures
- **`array`** - Fixed-size arrays
- **`Optional<T>`** - Optional values
- **`Result<T>`** - Result type for error handling

### Type Aliases
```wyn
type UserId = int;
type Point2D = Point;

let id: UserId = 123;
```

## Functions

### Basic Functions
```wyn
fn add(a: int, b: int) -> int {
    return a + b;
}
```

### No Return Value
```wyn
fn print_hello() -> void {
    print_str("Hello!");
}
```

### Multiple Parameters
```wyn
fn calculate(a: int, b: int, c: int) -> int {
    return a + b * c;
}
```

### Function Overloading
```wyn
fn process(x: int) -> int {
    return x * 2;
}

fn process(x: string) -> string {
    return str_concat(x, "_processed");
}
```

## Control Flow

### If Statements
```wyn
if x > 0 {
    print_str("Positive");
} else if x < 0 {
    print_str("Negative");
} else {
    print_str("Zero");
}
```

### While Loops
```wyn
let mut i = 0;
while i < 10 {
    print(i);
    i = i + 1;
}
```

### For Loops
```wyn
for i in 0..10 {
    print(i);
}
```

### Pattern Matching
```wyn
match value {
    Some(x) => print(x),
    None => print_str("No value")
}
```

## Structs

### Definition
```wyn
struct Point {
    x: int,
    y: int
}
```

### Instantiation
```wyn
let p = Point { x: 10, y: 20 };
let x_val = p.x;
```

### Methods (Implementation Blocks)
```wyn
impl Point {
    fn distance(self) -> int {
        return self.x * self.x + self.y * self.y;
    }
    
    fn translate(self, dx: int, dy: int) -> Point {
        return Point { x: self.x + dx, y: self.y + dy };
    }
}
```

### Using Methods
```wyn
let p = Point { x: 3, y: 4 };
let dist = p.distance();
let moved = p.translate(5, 5);
```

## Generics

### Generic Functions
```wyn
fn identity<T>(x: T) -> T {
    return x;
}

fn main() -> int {
    let num = identity(42);
    let text = identity("hello");
    return num;
}
```

### Generic Structs
```wyn
struct Box<T> {
    value: T
}

impl<T> Box<T> {
    fn new(value: T) -> Box<T> {
        return Box { value: value };
    }
    
    fn get(self) -> T {
        return self.value;
    }
}
```

### Using Generic Types
```wyn
let int_box = Box::new(42);
let string_box = Box::new("hello");
let value = int_box.get();
```

## Async/Await

### Async Functions
```wyn
async fn fetch_data() -> int {
    return 42;
}
```

### Await Operations
```wyn
fn main() -> int {
    let future = fetch_data();
    let result = await future;
    return result;
}
```

### Multiple Async Operations
```wyn
async fn step1() -> int {
    return 10;
}

async fn step2(x: int) -> int {
    return x * 2;
}

fn main() -> int {
    let f1 = step1();
    let val = await f1;
    let f2 = step2(val);
    let result = await f2;
    return result;
}
```

### Concurrent Execution
```wyn
async fn parallel_work() -> int {
    let f1 = fetch_data();
    let f2 = fetch_data();
    
    let result1 = await f1;
    let result2 = await f2;
    
    return result1 + result2;
}
```

## Modules

### Exporting Functions
```wyn
// math.wyn
export fn add(a: int, b: int) -> int {
    return a + b;
}

export fn multiply(a: int, b: int) -> int {
    return a * b;
}
```

### Importing Modules
```wyn
// main.wyn
import math;

fn main() -> int {
    return math::add(1, 2);
}
```

### Multiple Imports
```wyn
import math;
import utils;

fn main() -> int {
    let sum = math::add(1, 2);
    let product = utils::multiply(3, 4);
    return sum + product;
}
```

### Selective Imports
```wyn
import math::{add, multiply};

fn main() -> int {
    return add(1, multiply(2, 3));
}
```

## Memory Management

### Automatic Reference Counting (ARC)
```wyn
let data = Box::new(42);
// Automatically freed when out of scope
```

### Manual Memory Management
```wyn
let map = hashmap_new();
hashmap_insert(map, "key", 42);
// ... use map ...
hashmap_free(map);  // Manual cleanup required
```

### Memory Safety Rules
1. **No null pointer dereferences** - Use `Optional<T>` instead
2. **No use after free** - ARC prevents this automatically
3. **No memory leaks** - Automatic cleanup for most types

## Best Practices

### Code Style
1. **Use descriptive names** - `calculate_total` not `calc`
2. **Keep functions small** - One responsibility per function
3. **Use type annotations** - Make intent clear
4. **Handle errors explicitly** - Use Result type for fallible operations

### Performance Tips
1. **Prefer immutable data** - Easier to reason about
2. **Use references when possible** - Avoid unnecessary copying
3. **Profile before optimizing** - Measure actual performance
4. **Consider async for I/O** - Don't block on network/disk operations

### Error Handling Patterns
```wyn
// Simple error indication
fn divide(a: int, b: int) -> int {
    if b == 0 {
        return -1;  // Error indicator
    }
    return a / b;
}

// Using Optional for missing values
fn find_value(key: string) -> Optional<int> {
    // Return Some(value) or None
}

// Using Result for detailed errors
fn parse_number(text: string) -> Result<int, string> {
    // Return Ok(number) or Err(error_message)
}
```

### Common Patterns

#### Builder Pattern
```wyn
struct Config {
    host: string,
    port: int,
    debug: bool
}

impl Config {
    fn new() -> Config {
        return Config { 
            host: "localhost", 
            port: 8080, 
            debug: false 
        };
    }
    
    fn with_host(self, host: string) -> Config {
        return Config { 
            host: host, 
            port: self.port, 
            debug: self.debug 
        };
    }
    
    fn with_port(self, port: int) -> Config {
        return Config { 
            host: self.host, 
            port: port, 
            debug: self.debug 
        };
    }
}

// Usage
let config = Config::new()
    .with_host("example.com")
    .with_port(9000);
```

#### Iterator Pattern
```wyn
fn process_array(arr: array<int>) -> int {
    let mut sum = 0;
    let mut i = 0;
    
    while i < arr.len() {
        sum = sum + arr[i];
        i = i + 1;
    }
    
    return sum;
}
```

## Language Features Summary

| Feature | Status | Example |
|---------|--------|---------|
| Variables | ✅ | `let x = 42;` |
| Functions | ✅ | `fn add(a: int, b: int) -> int` |
| Structs | ✅ | `struct Point { x: int, y: int }` |
| Enums | ✅ | `enum Color { Red, Green, Blue }` |
| Generics | ✅ | `fn identity<T>(x: T) -> T` |
| Pattern Matching | ✅ | `match value { Some(x) => x, None => 0 }` |
| Async/Await | ✅ | `async fn fetch() -> int` |
| Modules | ✅ | `import math; math::add(1, 2)` |
| Type Aliases | ✅ | `type UserId = int;` |
| Method Chaining | ✅ | `"hello".upper().len()` |

## See Also

- [**Getting Started Guide**](getting-started.md) - Installation and first steps
- [**Standard Library Reference**](stdlib-reference.md) - Built-in functions and methods
- [**Examples**](examples.md) - Code examples and tutorials
- [**FAQ**](faq.md) - Common questions and troubleshooting

---

*This guide covers Wyn v1.2.2. For the latest updates, see the [GitHub repository](https://github.com/wyn-lang/wyn).*