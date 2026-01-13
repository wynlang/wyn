# Wyn Programming Tutorial

Welcome to Wyn! This tutorial will guide you through the basics of the Wyn programming language, from your first "Hello, World!" program to more advanced concepts.

## Table of Contents

1. [Getting Started](#getting-started)
2. [Your First Program](#your-first-program)
3. [Variables and Types](#variables-and-types)
4. [Functions](#functions)
5. [Control Flow](#control-flow)
6. [Collections](#collections)
7. [Structs and Methods](#structs-and-methods)
8. [Error Handling](#error-handling)
9. [Memory Management](#memory-management)
10. [Next Steps](#next-steps)

## Getting Started

### Installation

First, make sure you have Wyn installed on your system. If you haven't installed it yet:

```bash
# Clone the repository
git clone https://github.com/wyn-lang/wyn.git
cd wyn

# Build the compiler
make

# Install system-wide (optional)
sudo make install
```

### Verifying Installation

Test that Wyn is working correctly:

```bash
wyn --version
```

## Your First Program

Let's start with the traditional "Hello, World!" program.

Create a file called `hello.wyn`:

```wyn
fn main() -> int {
    print("Hello, World!")
    return 0
}
```

Compile and run it:

```bash
wyn hello.wyn
./hello
```

You should see:
```
Hello, World!
```

### Understanding the Code

- `fn main() -> int` declares the main function that returns an integer
- `print("Hello, World!")` outputs text to the console
- `return 0` indicates successful program execution

## Variables and Types

### Declaring Variables

In Wyn, you can declare variables using `var` for mutable variables or `let` for immutable ones:

```wyn
fn main() -> int {
    var age = 25           // Mutable variable, type inferred
    let name = "Alice"     // Immutable variable
    var height: float = 5.8 // Explicit type annotation
    
    print("Name: " + name)
    print("Age: ")
    print(age)
    print("Height: ")
    print(height)
    
    age = 26  // This works because age is mutable
    // name = "Bob"  // This would cause an error - name is immutable
    
    return 0
}
```

### Basic Types

Wyn has several built-in types:

```wyn
fn main() -> int {
    // Integer types
    var small_number: int = 42
    var big_number: int = 1000000
    
    // Floating point
    var pi: float = 3.14159
    var temperature: float = 98.6
    
    // Boolean
    var is_sunny: bool = true
    var is_raining: bool = false
    
    // String
    var message: string = "Welcome to Wyn!"
    var empty_string: string = ""
    
    return 0
}
```

### Type Conversion

```wyn
fn main() -> int {
    var integer_value = 42
    var float_value = 3.14
    
    // Convert int to float
    var converted_float: float = integer_value
    
    // Convert float to int (truncates)
    var converted_int: int = float_value  // Results in 3
    
    print(converted_int)
    return 0
}
```

## Functions

Functions are the building blocks of Wyn programs. Let's explore how to create and use them.

### Basic Functions

```wyn
fn greet(name: string) -> void {
    print("Hello, " + name + "!")
}

fn add(a: int, b: int) -> int {
    return a + b
}

fn main() -> int {
    greet("World")
    
    var result = add(5, 3)
    print("5 + 3 = ")
    print(result)
    
    return 0
}
```

### Functions with Multiple Return Values

```wyn
fn divide_with_remainder(dividend: int, divisor: int) -> (int, int) {
    var quotient = dividend / divisor
    var remainder = dividend % divisor
    return (quotient, remainder)
}

fn main() -> int {
    var (q, r) = divide_with_remainder(17, 5)
    print("17 ÷ 5 = ")
    print(q)
    print(" remainder ")
    print(r)
    
    return 0
}
```

### Default Parameters

```wyn
fn greet_with_title(name: string, title: string = "Mr./Ms.") -> void {
    print("Hello, " + title + " " + name + "!")
}

fn main() -> int {
    greet_with_title("Smith")           // Uses default title
    greet_with_title("Johnson", "Dr.")  // Uses custom title
    return 0
}
```

## Control Flow

### Conditional Statements

```wyn
fn check_temperature(temp: int) -> void {
    if temp > 30 {
        print("It's hot!")
    } else if temp > 20 {
        print("It's warm.")
    } else if temp > 10 {
        print("It's cool.")
    } else {
        print("It's cold!")
    }
}

fn main() -> int {
    check_temperature(35)
    check_temperature(25)
    check_temperature(15)
    check_temperature(5)
    return 0
}
```

### Loops

#### While Loops

```wyn
fn count_down() -> void {
    var i = 5
    while i > 0 {
        print(i)
        i = i - 1
    }
    print("Blast off!")
}

fn main() -> int {
    count_down()
    return 0
}
```

#### For Loops

```wyn
fn print_numbers() -> void {
    for var i = 1; i <= 5; i = i + 1 {
        print("Number: ")
        print(i)
    }
}

fn main() -> int {
    print_numbers()
    return 0
}
```

#### Loop Control

```wyn
fn find_first_even() -> void {
    for var i = 1; i <= 10; i = i + 1 {
        if i % 2 == 0 {
            print("First even number: ")
            print(i)
            break  // Exit the loop
        }
    }
}

fn print_odd_numbers() -> void {
    for var i = 1; i <= 10; i = i + 1 {
        if i % 2 == 0 {
            continue  // Skip even numbers
        }
        print(i)
    }
}

fn main() -> int {
    find_first_even()
    print("Odd numbers from 1 to 10:")
    print_odd_numbers()
    return 0
}
```

## Collections

### Arrays

Arrays in Wyn can be fixed-size or dynamic:

```wyn
fn array_examples() -> void {
    // Fixed-size array
    var numbers = [1, 2, 3, 4, 5]
    
    // Access elements
    print("First number: ")
    print(numbers[0])
    print("Third number: ")
    print(numbers[2])
    
    // Modify elements
    numbers[1] = 10
    print("Modified second number: ")
    print(numbers[1])
}

fn main() -> int {
    array_examples()
    return 0
}
```

### Iterating Over Arrays

```wyn
fn sum_array(arr: []int) -> int {
    var total = 0
    for var i = 0; i < 5; i = i + 1 {  // Assuming array length is 5
        total = total + arr[i]
    }
    return total
}

fn main() -> int {
    var numbers = [10, 20, 30, 40, 50]
    var sum = sum_array(numbers)
    print("Sum of array: ")
    print(sum)
    return 0
}
```

## Structs and Methods

Structs allow you to group related data together:

### Defining Structs

```wyn
struct Person {
    name: string,
    age: int,
    email: string
}

struct Point {
    x: int,
    y: int
}
```

### Creating and Using Structs

```wyn
fn main() -> int {
    // Create a Person
    var person = Person {
        name: "Alice",
        age: 30,
        email: "alice@example.com"
    }
    
    // Access fields
    print("Name: " + person.name)
    print("Age: ")
    print(person.age)
    
    // Modify fields
    person.age = 31
    print("New age: ")
    print(person.age)
    
    return 0
}
```

### Methods

You can add methods to structs using `impl` blocks:

```wyn
struct Rectangle {
    width: int,
    height: int
}

impl Rectangle {
    fn new(width: int, height: int) -> Rectangle {
        return Rectangle { width: width, height: height }
    }
    
    fn area(self) -> int {
        return self.width * self.height
    }
    
    fn perimeter(self) -> int {
        return 2 * (self.width + self.height)
    }
    
    fn scale(mut self, factor: int) -> void {
        self.width = self.width * factor
        self.height = self.height * factor
    }
}

fn main() -> int {
    var rect = Rectangle::new(5, 3)
    
    print("Area: ")
    print(rect.area())
    
    print("Perimeter: ")
    print(rect.perimeter())
    
    rect.scale(2)
    print("After scaling - Area: ")
    print(rect.area())
    
    return 0
}
```

## Error Handling

Wyn uses a Result type for error handling, similar to Rust:

### Basic Error Handling

```wyn
enum Result<T, E> {
    Ok(T),
    Err(E)
}

fn divide(a: int, b: int) -> Result<int, string> {
    if b == 0 {
        return Err("Cannot divide by zero")
    }
    return Ok(a / b)
}

fn main() -> int {
    var result1 = divide(10, 2)
    match result1 {
        Ok(value) => {
            print("10 / 2 = ")
            print(value)
        },
        Err(error) => {
            print("Error: " + error)
        }
    }
    
    var result2 = divide(10, 0)
    match result2 {
        Ok(value) => {
            print("10 / 0 = ")
            print(value)
        },
        Err(error) => {
            print("Error: " + error)
        }
    }
    
    return 0
}
```

### Optional Values

```wyn
enum Option<T> {
    Some(T),
    None
}

fn find_in_array(arr: []int, target: int) -> Option<int> {
    for var i = 0; i < 5; i = i + 1 {  // Assuming array length is 5
        if arr[i] == target {
            return Some(i)
        }
    }
    return None
}

fn main() -> int {
    var numbers = [10, 20, 30, 40, 50]
    
    var index = find_in_array(numbers, 30)
    match index {
        Some(i) => {
            print("Found 30 at index: ")
            print(i)
        },
        None => {
            print("30 not found in array")
        }
    }
    
    return 0
}
```

## Memory Management

Wyn uses Automatic Reference Counting (ARC) for memory management, which means you don't need to manually allocate and deallocate memory in most cases.

### Understanding Ownership

```wyn
fn ownership_example() -> void {
    var message = "Hello, Wyn!"  // message owns the string
    print_message(message)       // message is passed to function
    print(message)               // message is still valid here
}

fn print_message(msg: string) -> void {
    print("Message: " + msg)
}  // msg goes out of scope, but original message is unaffected

fn main() -> int {
    ownership_example()
    return 0
}
```

### References

```wyn
fn modify_number(num: &mut int) -> void {
    *num = *num + 10
}

fn read_number(num: &int) -> void {
    print("Number is: ")
    print(*num)
}

fn main() -> int {
    var value = 42
    
    read_number(&value)      // Pass immutable reference
    modify_number(&mut value) // Pass mutable reference
    read_number(&value)      // Value has been modified
    
    return 0
}
```

## Next Steps

Congratulations! You've learned the basics of Wyn programming. Here are some suggestions for continuing your journey:

### Practice Projects

1. **Calculator**: Build a simple calculator that can perform basic arithmetic operations
2. **Todo List**: Create a command-line todo list application
3. **File Reader**: Write a program that reads and processes text files
4. **Number Guessing Game**: Implement a game where the user guesses a random number

### Advanced Topics to Explore

1. **Generics**: Learn how to write generic functions and structs
2. **Traits**: Understand how to define and implement traits for shared behavior
3. **Modules**: Organize your code into modules and packages
4. **Concurrency**: Explore Wyn's concurrency features for parallel programming
5. **FFI**: Learn how to interface with C libraries

### Example Calculator Project

Here's a simple calculator to get you started:

```wyn
fn add(a: float, b: float) -> float {
    return a + b
}

fn subtract(a: float, b: float) -> float {
    return a - b
}

fn multiply(a: float, b: float) -> float {
    return a * b
}

fn divide(a: float, b: float) -> Result<float, string> {
    if b == 0.0 {
        return Err("Division by zero")
    }
    return Ok(a / b)
}

fn main() -> int {
    var a = 10.0
    var b = 3.0
    
    print("a = ")
    print(a)
    print("b = ")
    print(b)
    
    print("a + b = ")
    print(add(a, b))
    
    print("a - b = ")
    print(subtract(a, b))
    
    print("a * b = ")
    print(multiply(a, b))
    
    var div_result = divide(a, b)
    match div_result {
        Ok(result) => {
            print("a / b = ")
            print(result)
        },
        Err(error) => {
            print("Error: " + error)
        }
    }
    
    return 0
}
```

### Resources

- **Language Reference**: See `docs/language-reference.md` for complete syntax details
- **Standard Library**: Check `docs/stdlib-reference.md` for available functions and types
- **Best Practices**: Read `docs/best-practices.md` for coding guidelines
- **Examples**: Explore the `examples/` directory for more code samples

Happy coding with Wyn!