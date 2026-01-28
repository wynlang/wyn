# Wyn Type System Specification

This document defines the type system of the Wyn programming language, including primitive types, composite types, generics, and type inference rules.

For the formal grammar definition, see [Grammar Specification](grammar.md).
For memory layout and management details, see [Memory Model Specification](memory-model.md).

## Primitive Types

### Integer Types
- **`int`**: 64-bit signed integer
  - Range: -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
  - Default value: `0`

### Floating Point Types
- **`float`**: 64-bit IEEE 754 double precision
  - Range: ±1.7976931348623157E+308
  - Default value: `0.0`

### Boolean Type
- **`bool`**: Boolean value
  - Values: `true`, `false`
  - Default value: `false`

### String Type
- **`string`**: UTF-8 encoded string
  - Immutable by default
  - Default value: `""`

### Void Type
- **`void`**: Represents absence of a value
  - Used as return type for functions that don't return a value

## Composite Types

### Array Types
Arrays are homogeneous collections with fixed size known at compile time.

```wyn
// Array type syntax
[T]         // Array of type T
[int]       // Array of integers
[string]    // Array of strings

// Array literals
let numbers: [int] = [1, 2, 3, 4, 5];
let names: [string] = ["Alice", "Bob", "Charlie"];
```

### Struct Types
Structs are user-defined composite types with named fields.

```wyn
struct Point {
    x: int,
    y: int
}

struct Person {
    name: string,
    age: int,
    active: bool
}

// Struct instantiation
let p = Point { x: 10, y: 20 };
let person = Person { name: "Alice", age: 30, active: true };
```

### Tuple Types
Tuples are ordered collections of heterogeneous types.

```wyn
// Tuple type syntax
(T1, T2, ..., Tn)

// Examples
let coord: (int, int) = (10, 20);
let info: (string, int, bool) = ("Alice", 30, true);
```

## Enum Types

### Simple Enums
Enums without associated data represent a finite set of named values.

```wyn
enum Status {
    Active,
    Inactive,
    Pending
}

// Usage
let current_status = Status::Active;
```

### Enums with Data
Enums can carry associated data with each variant.

```wyn
enum Result<T, E> {
    Ok(T),
    Err(E)
}

enum Message {
    Quit,
    Move { x: int, y: int },
    Write(string),
    ChangeColor(int, int, int)
}

// Usage
let success: Result<int, string> = Result::Ok(42);
let failure: Result<int, string> = Result::Err("Something went wrong");
let msg = Message::Move { x: 10, y: 20 };
```

## Generic Types

### Generic Type Parameters
Generic types allow code reuse across different types while maintaining type safety.

```wyn
// Generic struct
struct Container<T> {
    value: T
}

// Generic enum
enum Option<T> {
    Some(T),
    None
}

// Generic function
fn identity<T>(x: T) -> T {
    return x;
}
```

### Type Constraints
Generic types can be constrained using trait bounds.

```wyn
trait Comparable {
    fn compare(self, other: Self) -> int;
}

fn max<T: Comparable>(a: T, b: T) -> T {
    if a.compare(b) > 0 {
        return a;
    }
    return b;
}
```

### Built-in Generic Types

#### HashMap<K, V>
Hash map with key type K and value type V.

```wyn
let ages: HashMap<string, int> = {
    "Alice": 25,
    "Bob": 30,
    "Charlie": 35
};

// Type inference
let scores = {"player1": 100, "player2": 85}; // HashMap<string, int>
```

#### Option<T>
Optional value that can be either Some(T) or None.

```wyn
fn find_user(id: int) -> Option<string> {
    if id == 1 {
        return Option::Some("Alice");
    }
    return Option::None;
}
```

#### Result<T, E>
Result type for error handling.

```wyn
fn divide(a: int, b: int) -> Result<int, string> {
    if b == 0 {
        return Result::Err("Division by zero");
    }
    return Result::Ok(a / b);
}
```

## Type Inference Rules

### Variable Declaration
The compiler infers types from initializer expressions.

```wyn
let x = 42;        // x: int
let y = 3.14;      // y: float
let name = "Alice"; // name: string
let flag = true;   // flag: bool
```

### Function Return Types
Return types can be inferred from return statements.

```wyn
fn add(a: int, b: int) {  // Return type inferred as int
    return a + b;
}
```

### Generic Type Inference
Generic type parameters are inferred from usage context.

```wyn
fn identity<T>(x: T) -> T {
    return x;
}

let num = identity(42);    // T inferred as int
let text = identity("hi"); // T inferred as string
```

### Collection Type Inference
Array and HashMap types are inferred from their elements.

```wyn
let numbers = [1, 2, 3];           // [int]
let mixed = [1, 2.0];              // Error: inconsistent types
let map = {"key": "value"};        // HashMap<string, string>
```

## Pattern Matching Type Rules

### Exhaustiveness Checking
Pattern matching must be exhaustive for all possible values.

```wyn
enum Color {
    Red,
    Green,
    Blue
}

fn describe_color(c: Color) -> string {
    match c {
        Color::Red => return "red",
        Color::Green => return "green",
        Color::Blue => return "blue"
        // All variants covered - exhaustive
    }
}
```

### Type Refinement
Pattern matching refines types within match arms.

```wyn
fn handle_option(opt: Option<int>) -> int {
    match opt {
        Option::Some(value) => {
            // value has type int here
            return value * 2;
        },
        Option::None => {
            return 0;
        }
    }
}
```

### Destructuring Patterns
Patterns can destructure composite types.

```wyn
struct Point { x: int, y: int }

fn process_point(p: Point) -> int {
    match p {
        Point { x: 0, y: 0 } => return 0,
        Point { x, y } => return x + y  // Destructure into variables
    }
}
```

## Type Coercion Rules

### Numeric Coercion
Limited automatic coercion between numeric types.

```wyn
let i: int = 42;
let f: float = i;      // int to float (allowed)
let i2: int = f;       // float to int (error - explicit cast required)
let i3: int = f as int; // Explicit cast (allowed)
```

### String Coercion
No automatic coercion to/from strings.

```wyn
let num = 42;
let text = num.to_string(); // Explicit conversion required
```

## Memory Layout

### Value Types
Primitive types and structs are value types stored on the stack.

```wyn
struct Point { x: int, y: int }  // 16 bytes (8 + 8)
let p = Point { x: 1, y: 2 };    // Stored on stack
```

### Reference Types
Arrays and strings are reference types with heap allocation.

```wyn
let arr = [1, 2, 3, 4, 5];  // Array data on heap, reference on stack
let text = "Hello, World!"; // String data on heap, reference on stack
```

## Type Safety Guarantees

### Null Safety
Wyn prevents null pointer dereferences through the type system.

```wyn
let name: string = null;        // Error: null not assignable to string
let opt_name: Option<string> = Option::None; // Correct way to represent absence
```

### Memory Safety
The type system prevents:
- Use after free
- Double free
- Buffer overflows
- Data races (in concurrent code)

### Type Soundness
The type system guarantees that well-typed programs cannot:
- Access uninitialized memory
- Perform invalid type casts
- Violate memory safety invariants

## Examples

### Generic Container with Constraints
```wyn
trait Display {
    fn to_string(self) -> string;
}

struct Container<T: Display> {
    items: [T]
}

impl<T: Display> Container<T> {
    fn new() -> Container<T> {
        return Container { items: [] };
    }
    
    fn add(&mut self, item: T) {
        self.items.push(item);
    }
    
    fn display_all(&self) {
        for item in self.items {
            print(item.to_string());
        }
    }
}
```

### Error Handling with Result Types
```wyn
fn parse_int(s: string) -> Result<int, string> {
    // Implementation would parse string to int
    if s.is_empty() {
        return Result::Err("Empty string");
    }
    // ... parsing logic
    return Result::Ok(42); // Placeholder
}

fn main() -> int {
    let input = "123";
    match parse_int(input) {
        Result::Ok(value) => {
            print("Parsed: " + value.to_string());
            return 0;
        },
        Result::Err(error) => {
            print("Error: " + error);
            return 1;
        }
    }
}
```