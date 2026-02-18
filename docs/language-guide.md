# Wyn Language Guide

![Version](https://img.shields.io/badge/version-1.6.0-blue)

Wyn is a modern systems programming language that transpiles to C, combining the safety and expressiveness of modern languages with the performance and portability of C.

## Getting Started

Wyn compiles your `.wyn` files to C code, then uses your system's C compiler to create native binaries:

```bash
./wyn run hello.wyn
```

## Variables

Wyn has two types of variables:

```wyn
var x = 42        // mutable variable
const y = 100     // immutable constant
```

## Types

### Basic Types

```wyn
var age: int = 25           // 64-bit integer
var price: float = 19.99    // double precision float
var name: string = "Alice"  // string
var active: bool = true     // boolean
```

### Arrays

```wyn
var numbers = [1, 2, 3, 4, 5]
var names: [string] = ["Alice", "Bob", "Charlie"]

// Access elements
var first = numbers[0]
```

## Functions

```wyn
fn add(a: int, b: int) -> int {
    return a + b
}

fn greet(name: string) {
    Terminal.print("Hello, " + name + "!")
}

// Call functions
var sum = add(5, 3)
greet("World")
```

## Control Flow

### If Statements

```wyn
var score = 85

if score >= 90 {
    Terminal.print("A grade")
} else if score >= 80 {
    Terminal.print("B grade")
} else {
    Terminal.print("Try harder")
}
```

### Loops

```wyn
// For loop with range
for i in 0..5 {
    Terminal.print("Count: " + i)
}

// For loop with array
var fruits = ["apple", "banana", "orange"]
for fruit in fruits {
    Terminal.print("Fruit: " + fruit)
}

// While loop
var count = 0
while count < 3 {
    Terminal.print("Count: " + count)
    count = count + 1
}
```

## Structs

```wyn
struct Person {
    name: string
    age: int
}

// Create instance
var person = Person { name: "Alice", age: 30 }

// Access fields
Terminal.print(person.name)
```

### Methods

```wyn
fn Person.greet(self) {
    Terminal.print("Hi, I'm " + self.name)
}

fn Person.have_birthday(mut self) {
    self.age = self.age + 1
}

// Use methods
person.greet()
person.have_birthday()
```

## Enums

```wyn
enum Color {
    Red,
    Green,
    Blue
}

var favorite = Color::Red

match favorite {
    Color::Red => Terminal.print("Passionate!")
    Color::Green => Terminal.print("Natural!")
    Color::Blue => Terminal.print("Calm!")
}
```

## Pattern Matching

```wyn
enum Result {
    Success,
    Error
}

var result = Result::Success

match result {
    Result::Success => Terminal.print("It worked!")
    Result::Error => Terminal.print("Something went wrong")
}
```

## Traits

```wyn
trait Drawable {
    fn draw(self)
}

struct Circle {
    radius: float
}

impl Drawable for Circle {
    fn draw(self) {
        Terminal.print("Drawing circle with radius: " + self.radius)
    }
}

var circle = Circle { radius: 5.0 }
circle.draw()
```

## Generics

```wyn
fn max<T>(a: T, b: T) -> T {
    if a > b {
        return a
    } else {
        return b
    }
}

var bigger_int = max(10, 20)
var bigger_float = max(3.14, 2.71)
```

## Error Handling

Wyn provides Result and Option types for safe error handling:

```wyn
fn divide(a: int, b: int) -> ResultInt {
    if b == 0 {
        return Err("Division by zero")
    }
    return Ok(a / b)
}

var result = divide(10, 2)
match result {
    Ok(value) => Terminal.print("Result: " + value)
    Err(error) => Terminal.print("Error: " + error)
}
```

```wyn
fn find_item(items: [string], target: string) -> OptionString {
    for item in items {
        if item == target {
            return Some(item)
        }
    }
    return None()
}

var found = find_item(["apple", "banana"], "banana")
match found {
    Some(item) => Terminal.print("Found: " + item)
    None() => Terminal.print("Not found")
}
```

## Closures

```wyn
var double = fn(x: int) -> int { return x * 2 }
var result = double(5)  // result is 10

var numbers = [1, 2, 3, 4, 5]
var doubled = numbers.map(fn(x: int) -> int { return x * 2 })
```

## Global Variables

```wyn
var global_counter = 0

fn increment() {
    global_counter = global_counter + 1
}

fn get_count() -> int {
    return global_counter
}
```

## Mutable References

```wyn
fn increment(mut x: int) {
    x = x + 1
}

var n = 5
increment(&n)  // n is now 6
```

## Modules

### Exporting

```wyn
// math.wyn
export fn add(a: int, b: int) -> int {
    return a + b
}

export const PI = 3.14159
```

### Importing

```wyn
// main.wyn
import math
import { add } from math

var sum = math.add(5, 3)
var sum2 = add(5, 3)
var circle_area = math.PI * radius * radius
```

## Concurrency

### Spawning Tasks

```wyn
fn heavy_computation(n: int) -> int {
    var result = 0
    for i in 0..n {
        result = result + i
    }
    return result
}

var future = spawn heavy_computation(1000)
var result = await future
Terminal.print("Result: " + result)
```

### Shared State

```wyn
var counter = Task.value(0)

fn worker() {
    for i in 0..10 {
        counter.add(1)
    }
}

var f1 = spawn worker()
var f2 = spawn worker()

await f1
await f2

Terminal.print("Final count: " + counter.get())
```

### Channels

```wyn
var ch = Task.channel(10)

fn producer() {
    for i in 0..5 {
        ch.send("Message " + i)
    }
    ch.close()
}

fn consumer() {
    while true {
        var msg = ch.receive()
        match msg {
            Some(value) => Terminal.print("Received: " + value)
            None() => break
        }
    }
}

spawn producer()
spawn consumer()
```

## Standard Library

### File Operations

```wyn
var content = File.read("data.txt")
File.write("output.txt", "Hello, World!")
```

### System Commands

```wyn
var result = System.exec("ls -la")
Terminal.print(result)
```

### Terminal Operations

```wyn
Terminal.print("Hello, World!")
Terminal.clear()
var input = Terminal.input("Enter your name: ")
```

### Math Functions

```wyn
var abs_value = Math.abs(-42)
var sqrt_value = Math.sqrt(16.0)
var max_value = Math.max(10, 20)
```

### Hash Maps

```wyn
// Inline literal (preferred)
var map = { "name": "Alice", "age": "30", "active": "true" }
println(map.get("name"))  // Alice

// Or build incrementally
var config = HashMap.new()
config.insert("host", "localhost")
config.insert("port", "8080")

// Save/load to JSON file
Data.save("config.json", config)
var loaded = Data.load("config.json")
println(loaded.get("host"))  // localhost
```

### Testing

```wyn
fn test_addition() {
    Test.init()
    Test.assert_eq(add(2, 3), 5)
    Test.assert_eq(add(-1, 1), 0)
}
```

## Package Management

### Installing Packages

```bash
# Install from local path
wyn install ./my-package

# Install from git repository
wyn install https://github.com/user/wyn-package
```

### wyn.toml Configuration

```toml
[package]
name = "my-app"
version = "1.0.0"

[packages]
math-utils = { path = "./packages/math-utils" }
http-client = { git = "https://github.com/user/wyn-http" }
```

## Comments

```wyn
// This is a single-line comment
var x = 42  // Comments can be at the end of lines

// Multi-line comments are just multiple single-line comments
// like this block here
```

## String Operations

```wyn
var first = "Hello"
var second = "World"
var greeting = first + ", " + second + "!"  // "Hello, World!"

var name = "Alice"
var message = "Welcome, " + name
```

## Best Practices

1. **Use descriptive variable names**: `user_count` instead of `uc`
2. **Prefer immutable data**: Use `const` when values don't change
3. **Handle errors explicitly**: Always match on Result and Option types
4. **Use traits for shared behavior**: Define common interfaces with traits
5. **Keep functions small**: Break complex logic into smaller functions
6. **Use pattern matching**: Leverage match expressions for clear control flow

## Example Program

```wyn
struct User {
    name: string
    email: string
    age: int
}

fn User.is_adult(self) -> bool {
    return self.age >= 18
}

fn User.greet(self) {
    var status = if self.is_adult() { "adult" } else { "minor" }
    Terminal.print("Hello " + self.name + " (" + status + ")")
}

fn main() {
    var users = [
        User { name: "Alice", email: "alice@example.com", age: 25 },
        User { name: "Bob", email: "bob@example.com", age: 16 }
    ]
    
    for user in users {
        user.greet()
    }
}
```

This guide covers the core features of Wyn v1.6.0. For more examples and advanced topics, explore the other documentation files in this directory.