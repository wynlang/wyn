# Frequently Asked Questions

![Version](https://img.shields.io/badge/version-1.6.0-blue.svg)
**Latest: v1.6.0**

Common questions and answers about the Wyn programming language.

## General Questions

### What is Wyn?

Wyn is a modern systems programming language with an "everything is an object" philosophy. This means you can call methods on any value, including primitives like integers and strings. Wyn compiles to native code and focuses on memory safety, performance, and developer productivity.

### How is Wyn different from other languages?

**Key differences:**
- **Everything is an object** - Call methods on any value: `42.abs()`, `"hello".len()`
- **Memory safety** - Automatic Reference Counting (ARC) prevents memory leaks
- **Simple syntax** - Easy to learn, similar to Rust and Go
- **Fast compilation** - Compiles to C, then to native code
- **No runtime** - Minimal overhead, suitable for systems programming

### Is Wyn production ready?

Yes! Wyn v1.6.0 is production ready with:
- ✅ Complete language implementation
- ✅ 150+ built-in methods
- ✅ Comprehensive standard library
- ✅ Memory safety guarantees
- ✅ Extensive test suite
- ✅ Active development and maintenance

### What can I build with Wyn?

Wyn is suitable for:
- **Command-line tools** - System utilities and automation
- **Web servers** - HTTP services and APIs
- **Data processing** - File parsing and transformation
- **Games** - Simple games and simulations
- **System software** - Low-level system components
- **Embedded software** - Resource-constrained environments

## Installation and Setup

### How do I install Wyn?

1. **Clone the repository:**
   ```bash
   git clone https://github.com/wynlang/wyn.git
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

### What are the system requirements?

**Minimum requirements:**
- **OS:** Linux, macOS, or Windows (with WSL)
- **RAM:** 512MB
- **Disk:** 50MB for compiler and examples
- **Dependencies:** C compiler (GCC/Clang), Make, Git

### Why do I get "command not found" when running `wyn`?

Make sure you've built the compiler first:
```bash
make wyn
```

Then run it from the wyn directory:
```bash
./wyn your_file.wyn
```

Or add it to your PATH:
```bash
export PATH=$PATH:/path/to/wyn
```

## Language Features

### Does Wyn have garbage collection?

No, Wyn uses **Automatic Reference Counting (ARC)** instead of garbage collection. This provides:
- **Deterministic memory management** - Objects are freed immediately when no longer referenced
- **No GC pauses** - Better for real-time applications
- **Lower memory overhead** - No need for a garbage collector runtime

### How do I handle errors in Wyn?

Wyn provides several error handling patterns:

**1. Return codes:**
```wyn
fn divide(a: int, b: int) -> int {
    if b == 0 {
        return -1;  // Error indicator
    }
    return a / b;
}
```

**2. Optional types:**
```wyn
fn find_user(id: int) -> Optional<string> {
    if id == 1 {
        return Some("Alice");
    }
    return None();
}
```

**3. Result types:**
```wyn
fn parse_number(text: string) -> Result<int, string> {
    // Return Ok(number) or Err(error_message)
}
```

### Does Wyn support object-oriented programming?

Yes, through structs and implementation blocks:

```wyn
struct Point {
    x: int,
    y: int
}

impl Point {
    fn new(x: int, y: int) -> Point {
        return Point { x: x, y: y };
    }
    
    fn distance(self) -> float {
        return sqrt(self.x * self.x + self.y * self.y);
    }
}

let p = Point::new(3, 4);
let dist = p.distance();
```

### How do generics work in Wyn?

Generics allow you to write code that works with multiple types:

```wyn
fn identity<T>(x: T) -> T {
    return x;
}

fn max<T>(a: T, b: T) -> T {
    if a > b {
        return a;
    } else {
        return b;
    }
}

let num = identity(42);      // T = int
let text = identity("hi");   // T = string
let larger = max(10, 20);    // T = int
```

### Does Wyn support async/await?

Yes! Wyn has built-in async/await support:

```wyn
async fn fetch_data() -> int {
    // Simulate async work
    return 42;
}

async fn process_data(data: int) -> int {
    return data * 2;
}

fn main() -> int {
    let future1 = fetch_data();
    let data = await future1;
    
    let future2 = process_data(data);
    let result = await future2;
    
    return result;  // 84
}
```

## Development and Debugging

### How do I debug Wyn programs?

**1. Use print statements:**
```wyn
fn debug_function(x: int) -> int {
    print("Input:");
    print(x);
    
    let result = x * 2;
    
    print("Output:");
    print(result);
    
    return result;
}
```

**2. Check return values:**
```wyn
let result = some_function();
if result == -1 {
    print("Function failed");
    return 1;
}
```

**3. Use assertions:**
```wyn
let result = add(2, 3);
assert_eq(result, 5);  // Will panic if not equal
```

### Why is my program crashing?

Common causes and solutions:

**1. Array bounds errors:**
```wyn
let arr = [1, 2, 3];
// Don't do this:
let x = arr[5];  // Out of bounds!

// Do this instead:
if index < array_len(arr) {
    let x = arr[index];
}
```

**2. Null pointer dereference:**
```wyn
// Use Optional types instead of null pointers
fn find_value(key: string) -> Optional<int> {
    // Return Some(value) or None()
}
```

**3. Memory management:**
```wyn
let map = hashmap_new();
// ... use map ...
hashmap_free(map);  // Don't forget to free!
```

### How do I optimize Wyn programs?

**1. Profile first:**
```wyn
let start = benchmark_start();
// ... code to measure ...
let elapsed = benchmark_end(start);
print(elapsed);
```

**2. Minimize allocations in loops:**
```wyn
// Good: allocate once
let map = hashmap_new();
for i in 0..1000 {
    hashmap_insert(map, key, value);
}

// Bad: allocate every iteration
for i in 0..1000 {
    let map = hashmap_new();  // Expensive!
    // ...
    hashmap_free(map);
}
```

**3. Use appropriate data structures:**
- Arrays for indexed access
- Hash maps for key-value lookups
- Hash sets for membership testing

## Compilation and Deployment

### How does Wyn compilation work?

Wyn uses a two-stage compilation process:

1. **Wyn → C:** The Wyn compiler translates your code to C
2. **C → Native:** A C compiler creates the final executable

```bash
./wyn program.wyn        # Creates program.wyn.c and program.wyn.out
./program.wyn.out        # Run the compiled program
```

### Can I see the generated C code?

Yes! The C code is saved as `filename.wyn.c`:

```bash
./wyn hello.wyn
cat hello.wyn.c  # View generated C code
```

This is useful for:
- Understanding how Wyn features work
- Debugging compilation issues
- Performance analysis

### How do I distribute Wyn programs?

**Option 1: Distribute the executable**
```bash
./wyn myprogram.wyn
# Distribute myprogram.wyn.out
```

**Option 2: Distribute source + compiler**
```bash
# Include wyn compiler and source files
tar -czf myapp.tar.gz wyn myprogram.wyn
```

**Option 3: Static linking (advanced)**
```bash
# Compile with static linking
gcc -static -o myprogram myprogram.wyn.c
```

## Performance and Limitations

### How fast is Wyn?

Wyn performance is comparable to C because:
- Compiles to optimized C code
- No garbage collection overhead
- Minimal runtime system
- Direct memory access

**Typical performance:**
- **Compilation:** <100ms for small programs
- **Runtime:** Near-native C performance
- **Binary size:** ~50KB for hello world
- **Memory usage:** Minimal overhead

### What are Wyn's current limitations?

**Language features:**
- No macros or metaprogramming
- Limited standard library compared to mature languages
- No package manager yet
- Single-threaded compilation

**Platform support:**
- Requires C compiler
- Limited Windows support (use WSL)
- No cross-compilation yet

**Ecosystem:**
- Small community
- Few third-party libraries
- Limited IDE support

### Is Wyn suitable for large projects?

Wyn is suitable for medium-sized projects (1K-10K lines). For larger projects, consider:

**Advantages:**
- Fast compilation keeps build times reasonable
- Strong type system prevents many bugs
- Memory safety reduces debugging time
- Simple module system

**Considerations:**
- Limited tooling compared to mature languages
- Smaller ecosystem of libraries
- Less community support

## Community and Contributing

### How can I contribute to Wyn?

**Ways to contribute:**
1. **Report bugs** - Create GitHub issues with reproducible examples
2. **Suggest features** - Propose new language features or improvements
3. **Write examples** - Add examples to the examples directory
4. **Improve documentation** - Fix typos, add explanations
5. **Write tests** - Add test cases for edge cases
6. **Code contributions** - Fix bugs or implement features

### Where can I get help?

**Resources:**
1. **Documentation** - Start with this FAQ and the language guide
2. **GitHub Issues** - Search existing issues or create new ones
3. **Examples** - Check the examples directory for code samples
4. **Source code** - The compiler source is well-commented

**When asking for help:**
- Include your code
- Describe expected vs actual behavior
- Mention your operating system
- Include error messages

### What's the future of Wyn?

**Planned features:**
- Package manager and module system
- Better IDE support (LSP server)
- Cross-compilation support
- Performance improvements
- Expanded standard library

**Long-term goals:**
- Self-hosting compiler (written in Wyn)
- WebAssembly target
- Embedded systems support
- Growing ecosystem of libraries

---

## Still have questions?

If you can't find the answer here:

1. **Check the documentation:**
   - [Getting Started](getting-started.md)
   - [Language Guide](language-guide.md)
   - [Standard Library](stdlib-reference.md)
   - [Examples](examples.md)

2. **Search GitHub issues** for similar questions

3. **Create a new issue** with:
   - Clear description of your question
   - Code examples if relevant
   - What you've already tried

*This FAQ covers Wyn v1.6.0. For the latest updates, see the [GitHub repository](https://github.com/wynlang/wyn).*