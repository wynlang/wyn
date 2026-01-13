# Wyn Standard Library Reference

The Wyn standard library provides essential functionality for common programming tasks. This reference covers all available modules, types, and functions.

## Table of Contents

1. [Core Types](#core-types)
2. [Collections](#collections)
3. [I/O Operations](#io-operations)
4. [String Operations](#string-operations)
5. [Math Functions](#math-functions)
6. [Memory Management](#memory-management)
7. [Error Handling](#error-handling)
8. [Networking](#networking)
9. [Concurrency](#concurrency)
10. [File System](#file-system)

## Core Types

### Option<T>

Represents an optional value that may or may not be present.

```wyn
enum Option<T> {
    Some(T),
    None
}

impl<T> Option<T> {
    fn is_some(self) -> bool
    fn is_none(self) -> bool
    fn unwrap(self) -> T  // Panics if None
    fn unwrap_or(self, default: T) -> T
    fn map<U>(self, f: fn(T) -> U) -> Option<U>
}
```

**Example:**
```wyn
fn find_item(items: []string, target: string) -> Option<int> {
    for var i = 0; i < items.len(); i = i + 1 {
        if items[i] == target {
            return Some(i)
        }
    }
    return None
}

fn main() -> int {
    var fruits = ["apple", "banana", "orange"]
    var index = find_item(fruits, "banana")
    
    match index {
        Some(i) => print("Found at index: " + i),
        None => print("Not found")
    }
    return 0
}
```

### Result<T, E>

Represents the result of an operation that may succeed or fail.

```wyn
enum Result<T, E> {
    Ok(T),
    Err(E)
}

impl<T, E> Result<T, E> {
    fn is_ok(self) -> bool
    fn is_err(self) -> bool
    fn unwrap(self) -> T  // Panics if Err
    fn unwrap_or(self, default: T) -> T
    fn map<U>(self, f: fn(T) -> U) -> Result<U, E>
    fn map_err<F>(self, f: fn(E) -> F) -> Result<T, F>
}
```

**Example:**
```wyn
fn parse_int(s: string) -> Result<int, string> {
    // Implementation would parse string to int
    if s == "42" {
        return Ok(42)
    } else {
        return Err("Invalid number format")
    }
}

fn main() -> int {
    var result = parse_int("42")
    match result {
        Ok(num) => print("Parsed: " + num),
        Err(error) => print("Error: " + error)
    }
    return 0
}
```

## Collections

### Vec<T>

Dynamic array that can grow and shrink at runtime.

```wyn
struct Vec<T> {
    // Internal implementation
}

impl<T> Vec<T> {
    fn new() -> Vec<T>
    fn with_capacity(capacity: int) -> Vec<T>
    fn len(self) -> int
    fn capacity(self) -> int
    fn is_empty(self) -> bool
    fn push(mut self, item: T)
    fn pop(mut self) -> Option<T>
    fn get(self, index: int) -> Option<T>
    fn set(mut self, index: int, value: T) -> bool
    fn insert(mut self, index: int, item: T)
    fn remove(mut self, index: int) -> Option<T>
    fn clear(mut self)
    fn contains(self, item: T) -> bool
    fn reverse(mut self)
    fn sort(mut self) where T: Comparable
}
```

**Example:**
```wyn
fn main() -> int {
    var numbers = Vec::new()
    numbers.push(1)
    numbers.push(2)
    numbers.push(3)
    
    print("Length: " + numbers.len())
    
    var first = numbers.get(0)
    match first {
        Some(value) => print("First: " + value),
        None => print("Empty vector")
    }
    
    var popped = numbers.pop()
    match popped {
        Some(value) => print("Popped: " + value),
        None => print("Nothing to pop")
    }
    
    return 0
}
```

### HashMap<K, V>

Hash table for key-value storage.

```wyn
struct HashMap<K, V> {
    // Internal implementation
}

impl<K, V> HashMap<K, V> where K: Hash + Eq {
    fn new() -> HashMap<K, V>
    fn with_capacity(capacity: int) -> HashMap<K, V>
    fn len(self) -> int
    fn is_empty(self) -> bool
    fn insert(mut self, key: K, value: V) -> Option<V>
    fn get(self, key: K) -> Option<V>
    fn remove(mut self, key: K) -> Option<V>
    fn contains_key(self, key: K) -> bool
    fn clear(mut self)
    fn keys(self) -> Vec<K>
    fn values(self) -> Vec<V>
}
```

**Example:**
```wyn
fn main() -> int {
    var scores = HashMap::new()
    scores.insert("Alice", 95)
    scores.insert("Bob", 87)
    scores.insert("Charlie", 92)
    
    var alice_score = scores.get("Alice")
    match alice_score {
        Some(score) => print("Alice's score: " + score),
        None => print("Alice not found")
    }
    
    if scores.contains_key("Bob") {
        print("Bob is in the map")
    }
    
    return 0
}
```

### HashSet<T>

Hash set for storing unique values.

```wyn
struct HashSet<T> {
    // Internal implementation
}

impl<T> HashSet<T> where T: Hash + Eq {
    fn new() -> HashSet<T>
    fn with_capacity(capacity: int) -> HashSet<T>
    fn len(self) -> int
    fn is_empty(self) -> bool
    fn insert(mut self, value: T) -> bool
    fn remove(mut self, value: T) -> bool
    fn contains(self, value: T) -> bool
    fn clear(mut self)
    fn union(self, other: HashSet<T>) -> HashSet<T>
    fn intersection(self, other: HashSet<T>) -> HashSet<T>
    fn difference(self, other: HashSet<T>) -> HashSet<T>
}
```

## I/O Operations

### Standard Input/Output

```wyn
// Print functions
fn print(value: int) -> void
fn print(value: float) -> void  
fn print(value: string) -> void
fn print(value: bool) -> void
fn println(value: string) -> void  // Print with newline

// Input functions
fn read_line() -> Result<string, string>
fn read_int() -> Result<int, string>
fn read_float() -> Result<float, string>
```

**Example:**
```wyn
fn main() -> int {
    print("Enter your name: ")
    var name_result = read_line()
    
    match name_result {
        Ok(name) => {
            println("Hello, " + name + "!")
        },
        Err(error) => {
            println("Error reading input: " + error)
        }
    }
    
    return 0
}
```

### File Operations

```wyn
struct File {
    // Internal implementation
}

impl File {
    fn open(path: string) -> Result<File, string>
    fn create(path: string) -> Result<File, string>
    fn read_to_string(mut self) -> Result<string, string>
    fn read_bytes(mut self) -> Result<Vec<u8>, string>
    fn write_string(mut self, content: string) -> Result<void, string>
    fn write_bytes(mut self, data: Vec<u8>) -> Result<void, string>
    fn close(mut self) -> Result<void, string>
}

// Convenience functions
fn read_file(path: string) -> Result<string, string>
fn write_file(path: string, content: string) -> Result<void, string>
```

**Example:**
```wyn
fn main() -> int {
    // Write to file
    var write_result = write_file("hello.txt", "Hello, World!")
    match write_result {
        Ok(_) => print("File written successfully"),
        Err(error) => print("Write error: " + error)
    }
    
    // Read from file
    var read_result = read_file("hello.txt")
    match read_result {
        Ok(content) => print("File content: " + content),
        Err(error) => print("Read error: " + error)
    }
    
    return 0
}
```

## String Operations

### String Type

```wyn
struct String {
    // Internal implementation
}

impl String {
    fn new() -> String
    fn from(s: string) -> String
    fn len(self) -> int
    fn is_empty(self) -> bool
    fn push(mut self, ch: char)
    fn push_str(mut self, s: string)
    fn pop(mut self) -> Option<char>
    fn clear(mut self)
    fn to_uppercase(self) -> String
    fn to_lowercase(self) -> String
    fn trim(self) -> String
    fn split(self, delimiter: string) -> Vec<String>
    fn replace(self, from: string, to: string) -> String
    fn starts_with(self, prefix: string) -> bool
    fn ends_with(self, suffix: string) -> bool
    fn contains(self, substring: string) -> bool
    fn find(self, pattern: string) -> Option<int>
    fn substring(self, start: int, end: int) -> String
}
```

**Example:**
```wyn
fn main() -> int {
    var text = String::from("  Hello, World!  ")
    
    var trimmed = text.trim()
    print("Trimmed: '" + trimmed + "'")
    
    var uppercase = text.to_uppercase()
    print("Uppercase: " + uppercase)
    
    var words = text.split(" ")
    print("Word count: " + words.len())
    
    if text.contains("World") {
        print("Found 'World' in text")
    }
    
    return 0
}
```

## Math Functions

### Basic Math Operations

```wyn
// Constants
const PI: float = 3.141592653589793
const E: float = 2.718281828459045

// Basic functions
fn abs(x: int) -> int
fn abs(x: float) -> float
fn min(a: int, b: int) -> int
fn min(a: float, b: float) -> float
fn max(a: int, b: int) -> int
fn max(a: float, b: float) -> float
fn pow(base: float, exp: float) -> float
fn sqrt(x: float) -> float
fn cbrt(x: float) -> float

// Trigonometric functions
fn sin(x: float) -> float
fn cos(x: float) -> float
fn tan(x: float) -> float
fn asin(x: float) -> float
fn acos(x: float) -> float
fn atan(x: float) -> float
fn atan2(y: float, x: float) -> float

// Logarithmic functions
fn log(x: float) -> float      // Natural logarithm
fn log10(x: float) -> float    // Base-10 logarithm
fn log2(x: float) -> float     // Base-2 logarithm
fn exp(x: float) -> float      // e^x

// Rounding functions
fn floor(x: float) -> float
fn ceil(x: float) -> float
fn round(x: float) -> float
fn trunc(x: float) -> float

// Random number generation
fn random() -> float           // Random float between 0.0 and 1.0
fn random_int(min: int, max: int) -> int
fn random_float(min: float, max: float) -> float
```

**Example:**
```wyn
fn main() -> int {
    var angle = PI / 4.0  // 45 degrees in radians
    
    print("sin(45°) = " + sin(angle))
    print("cos(45°) = " + cos(angle))
    print("tan(45°) = " + tan(angle))
    
    var number = 16.7
    print("sqrt(" + number + ") = " + sqrt(number))
    print("floor(" + number + ") = " + floor(number))
    print("ceil(" + number + ") = " + ceil(number))
    
    var random_num = random_int(1, 100)
    print("Random number: " + random_num)
    
    return 0
}
```

## Memory Management

### Reference Counting

```wyn
struct Arc<T> {
    // Atomic reference counter
}

impl<T> Arc<T> {
    fn new(value: T) -> Arc<T>
    fn clone(self) -> Arc<T>
    fn strong_count(self) -> int
    fn get(self) -> T
}

struct Weak<T> {
    // Weak reference (doesn't affect reference count)
}

impl<T> Weak<T> {
    fn upgrade(self) -> Option<Arc<T>>
    fn strong_count(self) -> int
}
```

**Example:**
```wyn
struct Node {
    value: int,
    next: Option<Arc<Node>>
}

fn create_linked_list() -> Arc<Node> {
    var node3 = Arc::new(Node { value: 3, next: None })
    var node2 = Arc::new(Node { value: 2, next: Some(Arc::clone(&node3)) })
    var node1 = Arc::new(Node { value: 1, next: Some(Arc::clone(&node2)) })
    return node1
}

fn main() -> int {
    var list = create_linked_list()
    print("First node value: " + list.get().value)
    return 0
}
```

## Error Handling

### Panic Functions

```wyn
fn panic(message: string) -> !  // Never returns
fn assert(condition: bool, message: string) -> void
fn unreachable() -> !
```

### Error Propagation

```wyn
// The ? operator for error propagation
fn read_and_parse_file(path: string) -> Result<int, string> {
    var content = read_file(path)?  // Propagates error if read fails
    var number = parse_int(content)?  // Propagates error if parse fails
    return Ok(number)
}
```

## Networking

### HTTP Client

```wyn
struct HttpClient {
    // Internal implementation
}

struct HttpResponse {
    status: int,
    headers: HashMap<string, string>,
    body: string
}

impl HttpClient {
    fn new() -> HttpClient
    fn get(mut self, url: string) -> Result<HttpResponse, string>
    fn post(mut self, url: string, body: string) -> Result<HttpResponse, string>
    fn put(mut self, url: string, body: string) -> Result<HttpResponse, string>
    fn delete(mut self, url: string) -> Result<HttpResponse, string>
    fn set_header(mut self, key: string, value: string)
    fn set_timeout(mut self, seconds: int)
}
```

**Example:**
```wyn
fn main() -> int {
    var client = HttpClient::new()
    client.set_header("User-Agent", "Wyn/1.0")
    
    var response = client.get("https://api.example.com/data")
    match response {
        Ok(resp) => {
            print("Status: " + resp.status)
            print("Body: " + resp.body)
        },
        Err(error) => {
            print("HTTP Error: " + error)
        }
    }
    
    return 0
}
```

### TCP Sockets

```wyn
struct TcpListener {
    // Internal implementation
}

struct TcpStream {
    // Internal implementation
}

impl TcpListener {
    fn bind(addr: string) -> Result<TcpListener, string>
    fn accept(mut self) -> Result<TcpStream, string>
}

impl TcpStream {
    fn connect(addr: string) -> Result<TcpStream, string>
    fn read(mut self, buffer: &mut [u8]) -> Result<int, string>
    fn write(mut self, data: &[u8]) -> Result<int, string>
    fn close(mut self) -> Result<void, string>
}
```

## Concurrency

### Threads

```wyn
struct Thread {
    // Internal implementation
}

impl Thread {
    fn spawn<F>(f: F) -> Thread where F: Fn() -> void + Send
    fn join(self) -> Result<void, string>
    fn sleep(duration_ms: int)
    fn current_id() -> int
}
```

### Synchronization

```wyn
struct Mutex<T> {
    // Internal implementation
}

struct MutexGuard<T> {
    // Internal implementation
}

impl<T> Mutex<T> {
    fn new(value: T) -> Mutex<T>
    fn lock(self) -> Result<MutexGuard<T>, string>
    fn try_lock(self) -> Option<MutexGuard<T>>
}

impl<T> MutexGuard<T> {
    fn get(self) -> &T
    fn get_mut(mut self) -> &mut T
}
```

**Example:**
```wyn
fn main() -> int {
    var counter = Arc::new(Mutex::new(0))
    var handles = Vec::new()
    
    for var i = 0; i < 10; i = i + 1 {
        var counter_clone = Arc::clone(&counter)
        var handle = Thread::spawn(|| {
            var guard = counter_clone.lock().unwrap()
            var value = guard.get_mut()
            *value = *value + 1
        })
        handles.push(handle)
    }
    
    for handle in handles {
        handle.join().unwrap()
    }
    
    var final_count = counter.lock().unwrap().get()
    print("Final count: " + *final_count)
    
    return 0
}
```

## File System

### Path Operations

```wyn
struct Path {
    // Internal implementation
}

impl Path {
    fn new(path: string) -> Path
    fn exists(self) -> bool
    fn is_file(self) -> bool
    fn is_dir(self) -> bool
    fn parent(self) -> Option<Path>
    fn file_name(self) -> Option<string>
    fn extension(self) -> Option<string>
    fn join(self, component: string) -> Path
    fn canonicalize(self) -> Result<Path, string>
}

// Directory operations
fn create_dir(path: string) -> Result<void, string>
fn create_dir_all(path: string) -> Result<void, string>
fn remove_dir(path: string) -> Result<void, string>
fn remove_dir_all(path: string) -> Result<void, string>
fn read_dir(path: string) -> Result<Vec<Path>, string>

// File operations
fn copy_file(from: string, to: string) -> Result<void, string>
fn move_file(from: string, to: string) -> Result<void, string>
fn remove_file(path: string) -> Result<void, string>
fn file_size(path: string) -> Result<int, string>
```

**Example:**
```wyn
fn main() -> int {
    var path = Path::new("example.txt")
    
    if path.exists() {
        if path.is_file() {
            print("It's a file")
            var size = file_size("example.txt")
            match size {
                Ok(bytes) => print("Size: " + bytes + " bytes"),
                Err(error) => print("Error getting size: " + error)
            }
        } else {
            print("It's a directory")
        }
    } else {
        print("Path doesn't exist")
    }
    
    return 0
}
```

This standard library reference provides the essential building blocks for Wyn programming. Each module is designed to be safe, efficient, and easy to use, following Wyn's principles of memory safety and performance.