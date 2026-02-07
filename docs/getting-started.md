# Getting Started with Wyn

![Version](https://img.shields.io/badge/version-1.6.0-blue.svg)
**Latest: v1.6.0**

Welcome to Wyn! This guide will help you install Wyn and write your first program.

## Philosophy: Everything is an Object

Wyn follows a simple principle: **everything is an object**. Instead of calling functions on data, you call methods on objects:

```wyn
// The Wyn way - method syntax
var clean = text.trim().lower()
var response = url.http_get()
var exists = path.exists()

// NOT the Wyn way - avoid function syntax
var clean = lower(trim(text))  // Don't do this!
```

## Installation

### Prerequisites

- **C Compiler** (GCC or Clang)
- **Make** build system
- **Git** (for cloning the repository)

### Build from Source

1. **Clone the repository:**
   ```bash
   git clone https://github.com/wyn-lang/wyn.git
   cd wyn
   ```

2. **Build the compiler:**
   ```bash
   make wyn
   ```

3. **Verify installation:**
   ```bash
   ./wyn --version
   ```

### System Requirements

- **Operating System:** Linux, macOS, or Windows (with WSL)
- **Memory:** 512MB RAM minimum
- **Disk Space:** 50MB for compiler and examples

## Your First Program

### Hello World

Create a file called `hello.wyn`:

```wyn
fn main() -> int {
    print("Hello, World!");
    return 0;
}
```

### Compile and Run

```bash
# Compile the program
./wyn hello.wyn

# Run the compiled program
./hello.wyn.out

# Check the exit code
echo $?
```

**Expected Output:**
```
Hello, World!
0
```

## Basic Concepts

### Variables and Types

Wyn has four primitive types and supports type inference:

```wyn
fn main() -> int {
    // Type inference
    let name = "Alice";        // string
    let age = 25;              // int
    let height = 5.8;          // float
    let active = true;         // bool
    
    // Explicit types
    let score: int = 100;
    let pi: float = 3.14159;
    
    print(name);
    print(age);
    print(height);
    
    return 0;
}
```

### Functions

Functions are defined with the `fn` keyword:

```wyn
fn add(a: int, b: int) -> int {
    return a + b;
}

fn greet(name: string) -> void {
    print("Hello, " + name + "!");
}

fn main() -> int {
    let result = add(5, 3);
    print(result);  // Output: 8
    
    greet("World"); // Output: Hello, World!
    
    return 0;
}
```

### Control Flow

#### If Statements
```wyn
fn main() -> int {
    let x = 10;
    
    if x > 0 {
        print("Positive");
    } else if x < 0 {
        print("Negative");
    } else {
        print("Zero");
    }
    
    return 0;
}
```

#### Loops
```wyn
fn main() -> int {
    // While loop
    let mut i = 0;
    while i < 5 {
        print(i);
        i = i + 1;
    }
    
    // For loop
    for j in 0..3 {
        print(j);
    }
    
    return 0;
}
```

### Structs

Define custom data types with structs:

```wyn
struct Person {
    name: string,
    age: int
}

fn main() -> int {
    let person = Person {
        name: "Alice",
        age: 30
    };
    
    print(person.name);
    print(person.age);
    
    return 0;
}
```

### Arrays

Work with collections of data:

```wyn
fn main() -> int {
    let numbers = [1, 2, 3, 4, 5];
    
    // Access elements
    print(numbers[0]);  // Output: 1
    print(numbers[4]);  // Output: 5
    
    // Array length
    let len = array_len(numbers);
    print(len);  // Output: 5
    
    return 0;
}
```

## Common Patterns

### Error Handling

```wyn
fn divide(a: int, b: int) -> int {
    if b == 0 {
        print("Error: Division by zero");
        return -1;  // Error indicator
    }
    return a / b;
}

fn main() -> int {
    let result = divide(10, 2);
    if result != -1 {
        print(result);  // Output: 5
    }
    
    let error = divide(10, 0);
    // Output: Error: Division by zero
    
    return 0;
}
```

### Working with Strings

```wyn
fn main() -> int {
    var text = "  Hello World  ";
    
    // String methods (Wyn way!)
    var len = text.len();
    var upper = text.upper();
    var trimmed = text.trim();
    
    print(len);           // Output: 15
    print(upper);         // Output:   HELLO WORLD  
    print(trimmed);       // Output: Hello World
    
    // Method chaining
    var clean = text.trim().lower();
    print(clean);         // Output: hello world
    
    // String formatting
    var msg = "Hello %s, you have %d messages".format("Alice", 5);
    print(msg);           // Output: Hello Alice, you have 5 messages
    
    return 0;
}
```

### File I/O

```wyn
fn main() -> int {
    var path = "/tmp/test.txt";
    
    // Check if file exists (method on path string!)
    if path.exists() {
        print("File already exists");
    }
    
    // Write to file
    var content = "Hello from Wyn!";
    File::write(path, content);
    
    // Read from file
    var data = File::read(path);
    print(data);          // Output: Hello from Wyn!
    
    // Check file type
    if path.is_file() {
        print("It's a file!");
    }
    
    return 0;
}
```

### HTTP Requests

```wyn
fn main() -> int {
    // HTTP GET (method on URL string!)
    var url = "http://httpbin.org/get";
    var response = url.http_get();
    print(response);
    
    return 0;
}
```

## Development Workflow

### Project Structure

```
my-project/
├── src/
│   ├── main.wyn
│   ├── utils.wyn
│   └── models.wyn
├── examples/
│   └── demo.wyn
└── README.md
```

### Compilation

```bash
# Compile single file
./wyn src/main.wyn

# Run compiled program
./src/main.wyn.out
```

### Testing Your Code

Create test functions to verify your code works:

```wyn
fn test_add() -> void {
    let result = add(2, 3);
    if result == 5 {
        print("✓ add test passed");
    } else {
        print("✗ add test failed");
    }
}

fn add(a: int, b: int) -> int {
    return a + b;
}

fn main() -> int {
    test_add();
    return 0;
}
```

### Debugging Tips

1. **Use print statements** to trace execution:
   ```wyn
   fn debug_function(x: int) -> int {
       print("Entering function with x =");
       print(x);
       let result = x * 2;
       print("Result =");
       print(result);
       return result;
   }
   ```

2. **Check return values** for error conditions:
   ```wyn
   let result = some_function();
   if result == -1 {
       print("Function failed");
       return 1;  // Exit with error
   }
   ```

3. **Use meaningful variable names**:
   ```wyn
   // Good
   let user_count = get_user_count();
   let is_valid = validate_input(data);
   
   // Avoid
   let x = get_user_count();
   let flag = validate_input(data);
   ```

## Next Steps

### Learn More

1. **Read the [Language Guide](language-guide.md)** - Complete syntax reference
2. **Explore [Examples](examples.md)** - Real-world code examples
3. **Check the [Standard Library](stdlib-reference.md)** - Available functions and methods

### Try These Examples

1. **Calculator Program** - Build a simple calculator
2. **File Processor** - Read and process text files
3. **Data Structures** - Implement lists, trees, or graphs
4. **Web Server** - Create a basic HTTP server (advanced)

### Join the Community

- **GitHub Issues** - Report bugs or request features
- **Discussions** - Ask questions and share projects
- **Contributing** - Help improve Wyn

## Troubleshooting

### Common Issues

#### Compilation Errors

**Problem:** `./wyn: command not found`
**Solution:** Make sure you've built the compiler with `make wyn`

**Problem:** `error: undefined function 'some_function'`
**Solution:** Check function names and ensure they're defined before use

#### Runtime Errors

**Problem:** Segmentation fault
**Solution:** Check array bounds and pointer usage

**Problem:** Unexpected output
**Solution:** Verify logic with print statements and test with simple inputs

### Getting Help

1. **Check the [FAQ](faq.md)** for common questions
2. **Search existing GitHub issues**
3. **Create a new issue** with:
   - Your code
   - Expected behavior
   - Actual behavior
   - System information

## Performance Tips

1. **Minimize memory allocations** in loops
2. **Use appropriate data structures** (arrays vs hash maps)
3. **Profile your code** to find bottlenecks
4. **Consider async/await** for I/O operations

---

**Congratulations!** You're now ready to start programming in Wyn. Check out the [examples](examples.md) for more inspiration, or dive into the [language guide](language-guide.md) for advanced features.

*This guide covers Wyn v1.6.0. For the latest updates, see the [GitHub repository](https://github.com/wyn-lang/wyn).*