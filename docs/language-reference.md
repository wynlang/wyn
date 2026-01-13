# Wyn Language Reference

## Table of Contents

1. [Lexical Structure](#lexical-structure)
2. [Types](#types)
3. [Variables and Constants](#variables-and-constants)
4. [Functions](#functions)
5. [Control Flow](#control-flow)
6. [Structs and Enums](#structs-and-enums)
7. [Generics](#generics)
8. [Memory Management](#memory-management)
9. [Modules](#modules)
10. [Standard Library](#standard-library)

## Lexical Structure

### Comments

```wyn
// Single-line comment

/*
 * Multi-line comment
 * Can span multiple lines
 */
```

### Keywords

Reserved keywords in Wyn:

```
fn      return  if      else    while   for     var     let
struct  enum    match   true    false   int     float   string
bool    void    import  export  pub     priv    mut     ref
```

### Identifiers

Identifiers must start with a letter or underscore, followed by letters, digits, or underscores:

```wyn
var myVariable = 42
var _private = "hidden"
var camelCase = true
var snake_case = false
```

### Literals

#### Integer Literals
```wyn
var decimal = 42
var hex = 0xFF
var binary = 0b1010
var octal = 0o755
```

#### Float Literals
```wyn
var pi = 3.14159
var scientific = 1.23e-4
var large = 1.0e10
```

#### String Literals
```wyn
var simple = "Hello, World!"
var escaped = "Line 1\nLine 2\tTabbed"
var raw = r"No escaping here: \n \t"
```

#### Boolean Literals
```wyn
var yes = true
var no = false
```

## Types

### Primitive Types

#### Integer Types
```wyn
var i8_val: i8 = 127
var i16_val: i16 = 32767
var i32_val: i32 = 2147483647
var i64_val: i64 = 9223372036854775807
var int_val: int = 42  // Platform-dependent size
```

#### Unsigned Integer Types
```wyn
var u8_val: u8 = 255
var u16_val: u16 = 65535
var u32_val: u32 = 4294967295
var u64_val: u64 = 18446744073709551615
var uint_val: uint = 42  // Platform-dependent size
```

#### Floating Point Types
```wyn
var f32_val: f32 = 3.14
var f64_val: f64 = 3.141592653589793
var float_val: float = 2.718  // Defaults to f64
```

#### Boolean Type
```wyn
var flag: bool = true
var condition: bool = false
```

#### String Type
```wyn
var message: string = "Hello, Wyn!"
var empty: string = ""
```

### Array Types

#### Fixed-Size Arrays
```wyn
var numbers: [5]int = [1, 2, 3, 4, 5]
var matrix: [3][3]int = [[1, 2, 3], [4, 5, 6], [7, 8, 9]]
```

#### Dynamic Arrays (Slices)
```wyn
var dynamic: []int = [1, 2, 3, 4, 5]
var slice = numbers[1:4]  // Elements 1, 2, 3
```

### Pointer Types
```wyn
var ptr: *int = &value
var null_ptr: *int = null
```

### Function Types
```wyn
var callback: fn(int, int) -> int = add
var closure: fn() -> void = || { print("Hello") }
```

## Variables and Constants

### Variable Declaration

#### Mutable Variables
```wyn
var x = 42              // Type inferred as int
var y: int = 100        // Explicit type
var z: int              // Uninitialized (zero value)
```

#### Immutable Variables (Constants)
```wyn
let pi = 3.14159        // Immutable, type inferred
let name: string = "Wyn" // Immutable with explicit type
```

### Variable Assignment
```wyn
var counter = 0
counter = counter + 1   // Simple assignment
counter += 1            // Compound assignment
counter++               // Increment operator
```

### Scope and Shadowing
```wyn
var x = 10
{
    var x = 20          // Shadows outer x
    print(x)            // Prints 20
}
print(x)                // Prints 10
```

## Functions

### Function Declaration
```wyn
fn add(a: int, b: int) -> int {
    return a + b
}

fn greet(name: string) -> void {
    print("Hello, " + name)
}

fn no_return_type() {
    print("No explicit return type")
}
```

### Function Parameters

#### By Value
```wyn
fn square(x: int) -> int {
    return x * x
}
```

#### By Reference
```wyn
fn increment(x: &mut int) {
    *x = *x + 1
}
```

#### Default Parameters
```wyn
fn greet(name: string, greeting: string = "Hello") -> void {
    print(greeting + ", " + name)
}
```

### Function Overloading
```wyn
fn print(value: int) {
    // Print integer
}

fn print(value: string) {
    // Print string
}

fn print(value: float) {
    // Print float
}
```

### Higher-Order Functions
```wyn
fn apply(f: fn(int) -> int, x: int) -> int {
    return f(x)
}

fn double(x: int) -> int {
    return x * 2
}

var result = apply(double, 5)  // result = 10
```

### Closures
```wyn
fn make_adder(n: int) -> fn(int) -> int {
    return |x| { return x + n }
}

var add5 = make_adder(5)
var result = add5(10)  // result = 15
```

## Control Flow

### Conditional Statements

#### If-Else
```wyn
if condition {
    // Execute if true
} else if other_condition {
    // Execute if other_condition is true
} else {
    // Execute if all conditions are false
}
```

#### If Expressions
```wyn
var max = if a > b { a } else { b }
```

### Loops

#### While Loop
```wyn
var i = 0
while i < 10 {
    print(i)
    i = i + 1
}
```

#### For Loop
```wyn
for var i = 0; i < 10; i = i + 1 {
    print(i)
}
```

#### For-In Loop
```wyn
var numbers = [1, 2, 3, 4, 5]
for num in numbers {
    print(num)
}
```

#### Loop Control
```wyn
for var i = 0; i < 100; i = i + 1 {
    if i == 50 {
        break       // Exit loop
    }
    if i % 2 == 0 {
        continue    // Skip to next iteration
    }
    print(i)
}
```

### Pattern Matching
```wyn
enum Color {
    Red,
    Green,
    Blue,
    RGB(int, int, int)
}

fn describe_color(color: Color) -> string {
    match color {
        Red => "It's red",
        Green => "It's green", 
        Blue => "It's blue",
        RGB(r, g, b) => "RGB(" + r + ", " + g + ", " + b + ")"
    }
}
```

## Structs and Enums

### Struct Definition
```wyn
struct Point {
    x: int,
    y: int
}

struct Person {
    name: string,
    age: int,
    email: string
}
```

### Struct Instantiation
```wyn
var origin = Point { x: 0, y: 0 }
var person = Person {
    name: "Alice",
    age: 30,
    email: "alice@example.com"
}
```

### Struct Methods
```wyn
impl Point {
    fn new(x: int, y: int) -> Point {
        return Point { x: x, y: y }
    }
    
    fn distance_from_origin(self) -> float {
        return sqrt(self.x * self.x + self.y * self.y)
    }
    
    fn move_by(mut self, dx: int, dy: int) {
        self.x += dx
        self.y += dy
    }
}
```

### Enum Definition
```wyn
enum Status {
    Pending,
    InProgress,
    Complete,
    Failed(string)  // Enum with associated data
}

enum Option<T> {
    Some(T),
    None
}
```

### Enum Usage
```wyn
var status = Status::Pending
status = Status::Failed("Network error")

match status {
    Pending => print("Waiting..."),
    InProgress => print("Working..."),
    Complete => print("Done!"),
    Failed(error) => print("Error: " + error)
}
```

## Generics

### Generic Functions
```wyn
fn identity<T>(x: T) -> T {
    return x
}

fn swap<T>(a: &mut T, b: &mut T) {
    var temp = *a
    *a = *b
    *b = temp
}
```

### Generic Structs
```wyn
struct Vec<T> {
    data: *T,
    len: int,
    cap: int
}

impl<T> Vec<T> {
    fn new() -> Vec<T> {
        return Vec<T> { data: null, len: 0, cap: 0 }
    }
    
    fn push(mut self, item: T) {
        // Implementation
    }
    
    fn get(self, index: int) -> Option<T> {
        if index < self.len {
            return Some(self.data[index])
        } else {
            return None
        }
    }
}
```

### Type Constraints
```wyn
trait Comparable {
    fn compare(self, other: Self) -> int
}

fn max<T: Comparable>(a: T, b: T) -> T {
    if a.compare(b) > 0 {
        return a
    } else {
        return b
    }
}
```

## Memory Management

### Automatic Reference Counting (ARC)
```wyn
struct Node {
    value: int,
    next: Option<Arc<Node>>
}

fn create_list() -> Arc<Node> {
    var node1 = Arc::new(Node { value: 1, next: None })
    var node2 = Arc::new(Node { value: 2, next: Some(node1) })
    return node2
}  // Automatic cleanup when Arc goes out of scope
```

### Weak References
```wyn
struct Parent {
    children: Vec<Arc<Child>>
}

struct Child {
    parent: Weak<Parent>  // Prevents cycles
}
```

### Manual Memory Management
```wyn
fn unsafe_operations() {
    var ptr = malloc(1024)
    // Use ptr...
    free(ptr)  // Manual cleanup required
}
```

## Modules

### Module Declaration
```wyn
// math.wyn
pub fn add(a: int, b: int) -> int {
    return a + b
}

fn private_helper() -> int {
    return 42
}

pub struct Point {
    pub x: int,
    pub y: int
}
```

### Module Import
```wyn
// main.wyn
import math
import math::{add, Point}  // Selective import
import math::*             // Import all public items

fn main() -> int {
    var result = math::add(5, 3)
    var point = Point { x: 1, y: 2 }
    return 0
}
```

### Module Hierarchy
```wyn
// geometry/point.wyn
pub struct Point {
    pub x: float,
    pub y: float
}

// geometry/mod.wyn
pub mod point
pub use point::Point

// main.wyn
import geometry::Point
```

## Standard Library

### Collections
```wyn
import std::collections::{Vec, HashMap, HashSet}

var vec = Vec::new()
vec.push(1)
vec.push(2)

var map = HashMap::new()
map.insert("key", "value")

var set = HashSet::new()
set.insert(42)
```

### I/O Operations
```wyn
import std::io::{File, stdin, stdout}

var file = File::open("data.txt")?
var content = file.read_to_string()?
file.close()

print("Enter name: ")
var name = stdin.read_line()?
stdout.write("Hello, " + name)
```

### String Operations
```wyn
import std::string::String

var s = String::new()
s.push_str("Hello")
s.push(' ')
s.push_str("World")

var parts = s.split(' ')
var uppercase = s.to_uppercase()
var length = s.len()
```

### Error Handling
```wyn
enum Result<T, E> {
    Ok(T),
    Err(E)
}

fn divide(a: float, b: float) -> Result<float, string> {
    if b == 0.0 {
        return Err("Division by zero")
    }
    return Ok(a / b)
}

fn main() -> int {
    match divide(10.0, 2.0) {
        Ok(result) => print("Result: " + result),
        Err(error) => print("Error: " + error)
    }
    return 0
}
```

### Concurrency
```wyn
import std::thread::{spawn, sleep}
import std::sync::{Mutex, Arc}

fn concurrent_example() {
    var counter = Arc::new(Mutex::new(0))
    var handles = Vec::new()
    
    for var i = 0; i < 10; i = i + 1 {
        var counter_clone = Arc::clone(&counter)
        var handle = spawn(|| {
            var num = counter_clone.lock().unwrap()
            *num += 1
        })
        handles.push(handle)
    }
    
    for handle in handles {
        handle.join().unwrap()
    }
}
```