# Wyn Standard Library Reference

![Version](https://img.shields.io/badge/version-1.4.0-blue.svg)
**Latest: v1.4.0**

Complete reference for all standard library functions and methods available in Wyn.

## Object-Oriented API

Wyn v1.4.0 provides a fully object-oriented API. All operations use method syntax:

### String Methods
```wyn
var s = "hello";
s.len()                    // Length
s.upper()                  // Uppercase
s.lower()                  // Lowercase
s.trim()                   // Remove whitespace
s.concat(" world")         // Concatenate
s.contains("ell")          // Check substring
s.split(" ")               // Split into array
s.parse_int()              // Parse to integer
s.parse_float()            // Parse to float
```

### Number Methods
```wyn
var n = 42;
n.to_string()              // Convert to string
n.abs()                    // Absolute value
n.to_float()               // Convert to float

var f = 3.14;
f.to_string()              // Convert to string
f.to_int()                 // Convert to int
```

### Array Methods
```wyn
var arr = [1, 2, 3];
arr.len()                  // Length
arr.push(4)                // Add element
arr.pop()                  // Remove last
arr.contains(2)            // Check element
arr.sort()                 // Sort in place
```

### Standard Library Modules

All modules use `::` syntax and are available without import:

```wyn
// File operations
File::read(path)           // Read file
File::write(path, data)    // Write file
File::exists(path)         // Check existence
File::delete(path)         // Delete file

// System operations
System::exec(cmd)          // Execute command
System::exit(code)         // Exit program

// Math operations
Math::pow(x, y)            // Power
Math::sqrt(x)              // Square root
```

## Table of Contents

1. [I/O Functions](#io-functions)
2. [String Functions](#string-functions)
3. [Math Functions](#math-functions)
4. [Collections](#collections)
5. [File Operations](#file-operations)
6. [JSON](#json)
7. [Concurrency](#concurrency)
8. [Async](#async)
9. [Utility Functions](#utility-functions)

## I/O Functions

### Console Output

#### `print(value: any)`
Polymorphic print function that prints any value followed by newline.
Automatically detects type (int, float, string, bool, array).

```wyn
print(42);          // Output: 42
print(3.14);        // Output: 3.14
print("Hello");     // Output: Hello
print(true);        // Output: true
print([1, 2, 3]);   // Output: [1, 2, 3]
```

### Console Input

#### `input() -> int`
Reads an integer from standard input.
```wyn
print("Enter a number: ");
let num = input();
print(num);
```

#### `input_str() -> string`
Reads a string from standard input.
```wyn
print("Enter your name: ");
let name = input_str();
print(name);
```

## String Functions

### Basic String Operations

#### `str_len(s: string) -> int`
Returns the length of a string.
```wyn
let len = str_len("hello");  // Returns 5
```

#### `str_concat(a: string, b: string) -> string`
Concatenates two strings.
```wyn
let result = str_concat("Hello", " World");  // "Hello World"
```

#### `str_eq(a: string, b: string) -> bool`
Compares two strings for equality.
```wyn
if str_eq(name, "Alice") {
    print("Found Alice");
}
```

### String Manipulation

#### `str_upper(s: string) -> string`
Converts string to uppercase.
```wyn
let upper = str_upper("hello");  // "HELLO"
```

#### `str_lower(s: string) -> string`
Converts string to lowercase.
```wyn
let lower = str_lower("HELLO");  // "hello"
```

#### `str_trim(s: string) -> string`
Removes whitespace from both ends.
```wyn
let trimmed = str_trim("  hello  ");  // "hello"
```

#### `str_split(s: string, delimiter: string) -> array<string>`
Splits string by delimiter.
```wyn
let parts = str_split("a,b,c", ",");  // ["a", "b", "c"]
```

#### `str_substring(s: string, start: int, length: int) -> string`
Extracts substring.
```wyn
let sub = str_substring("hello", 1, 3);  // "ell"
```

### String Queries

#### `str_contains(s: string, substring: string) -> bool`
Checks if string contains substring.
```wyn
if str_contains("hello world", "world") {
    print("Found 'world'");
}
```

#### `str_starts_with(s: string, prefix: string) -> bool`
Checks if string starts with prefix.
```wyn
if str_starts_with("hello", "he") {
    print("Starts with 'he'");
}
```

#### `str_ends_with(s: string, suffix: string) -> bool`
Checks if string ends with suffix.
```wyn
if str_ends_with("hello", "lo") {
    print("Ends with 'lo'");
}
```

## Math Functions

### Basic Math

#### `abs_val(x: int) -> int`
Returns absolute value of integer.
```wyn
let result = abs_val(-5);  // Returns 5
```

#### `abs_float(x: float) -> float`
Returns absolute value of float.
```wyn
let result = abs_float(-3.14);  // Returns 3.14
```

#### `min(a: int, b: int) -> int`
Returns minimum of two integers.
```wyn
let smaller = min(10, 20);  // Returns 10
```

#### `max(a: int, b: int) -> int`
Returns maximum of two integers.
```wyn
let larger = max(10, 20);  // Returns 20
```

### Power and Roots

#### `pow_int(base: int, exp: int) -> int`
Raises integer base to integer power.
```wyn
let result = pow_int(2, 3);  // Returns 8
```

#### `pow_float(base: float, exp: float) -> float`
Raises float base to float power.
```wyn
let result = pow_float(2.0, 3.0);  // Returns 8.0
```

#### `sqrt(x: float) -> float`
Returns square root.
```wyn
let result = sqrt(16.0);  // Returns 4.0
```

### Trigonometry

#### `sin(x: float) -> float`
Returns sine of x (in radians).
```wyn
let result = sin(3.14159 / 2);  // Returns ~1.0
```

#### `cos(x: float) -> float`
Returns cosine of x (in radians).
```wyn
let result = cos(0.0);  // Returns 1.0
```

#### `tan(x: float) -> float`
Returns tangent of x (in radians).
```wyn
let result = tan(0.0);  // Returns 0.0
```

## Collections

### HashMap

Hash maps provide key-value storage with string keys.

#### `hashmap_new() -> WynHashMap*`
Creates a new hash map.
```wyn
let map = hashmap_new();
```

#### `hashmap_insert(map: WynHashMap*, key: string, value: int)`
Inserts a key-value pair.
```wyn
hashmap_insert(map, "age", 25);
hashmap_insert(map, "score", 100);
```

#### `hashmap_get(map: WynHashMap*, key: string) -> int`
Gets a value by key. Returns -1 if not found.
```wyn
let age = hashmap_get(map, "age");
if age != -1 {
    print(age);
}
```

#### `hashmap_has(map: WynHashMap*, key: string) -> bool`
Checks if key exists in map.
```wyn
if hashmap_has(map, "age") {
    print("Age is set");
}
```

#### `hashmap_remove(map: WynHashMap*, key: string)`
Removes a key-value pair.
```wyn
hashmap_remove(map, "age");
```

#### `hashmap_size(map: WynHashMap*) -> int`
Returns number of key-value pairs.
```wyn
let count = hashmap_size(map);
print(count);
```

#### `hashmap_free(map: WynHashMap*)`
Frees the hash map memory.
```wyn
hashmap_free(map);
```

### HashSet

Hash sets provide unique string storage.

#### `hashset_new() -> WynHashSet*`
Creates a new hash set.
```wyn
let set = hashset_new();
```

#### `hashset_add(set: WynHashSet*, key: string)`
Adds an element to the set.
```wyn
hashset_add(set, "apple");
hashset_add(set, "banana");
```

#### `hashset_contains(set: WynHashSet*, key: string) -> int`
Checks if element exists. Returns 1 if found, 0 otherwise.
```wyn
if hashset_contains(set, "apple") == 1 {
    print("Found apple");
}
```

#### `hashset_remove(set: WynHashSet*, key: string)`
Removes an element from the set.
```wyn
hashset_remove(set, "apple");
```

#### `hashset_size(set: WynHashSet*) -> int`
Returns number of elements in set.
```wyn
let count = hashset_size(set);
print(count);
```

#### `hashset_free(set: WynHashSet*)`
Frees the hash set memory.
```wyn
hashset_free(set);
```

### Arrays

#### `array_len(arr: array<T>) -> int`
Returns array length.
```wyn
let numbers = [1, 2, 3, 4, 5];
let len = array_len(numbers);  // Returns 5
```

#### `array_get(arr: array<T>, index: int) -> T`
Gets element at index.
```wyn
let first = array_get(numbers, 0);  // Returns 1
```

#### `array_set(arr: array<T>, index: int, value: T)`
Sets element at index.
```wyn
array_set(numbers, 0, 10);  // numbers[0] = 10
```

## File Operations

### File I/O

#### `file_read(path: string) -> string`
Reads entire file contents as string.
```wyn
let content = file_read("/tmp/data.txt");
if str_len(content) > 0 {
    print("File loaded successfully");
}
```

#### `file_write(path: string, content: string) -> int`
Writes content to file. Returns 1 on success, 0 on failure.
```wyn
let success = file_write("/tmp/output.txt", "Hello World");
if success == 1 {
    print("File written successfully");
}
```

#### `file_append(path: string, content: string) -> int`
Appends content to file. Returns 1 on success, 0 on failure.
```wyn
file_append("/tmp/log.txt", "New entry\n");
```

### File System

#### `file_exists(path: string) -> int`
Checks if file exists. Returns 1 if exists, 0 otherwise.
```wyn
if file_exists("/tmp/config.txt") == 1 {
    let config = file_read("/tmp/config.txt");
    // Process config...
}
```

#### `file_size(path: string) -> int`
Returns file size in bytes. Returns -1 if file doesn't exist.
```wyn
let size = file_size("/tmp/data.txt");
if size > 0 {
    print(size);
}
```

#### `file_delete(path: string) -> int`
Deletes file. Returns 1 on success, 0 on failure.
```wyn
let deleted = file_delete("/tmp/temp.txt");
if deleted == 1 {
    print("File deleted");
}
```

## JSON

### JSON Parsing

#### `json_parse(text: string) -> WynJson*`
Parses JSON string into object.
```wyn
let json_text = "{\"name\":\"Alice\",\"age\":30}";
let json = json_parse(json_text);
```

#### `json_get_string(json: WynJson*, key: string) -> string`
Gets string value from JSON object.
```wyn
let name = json_get_string(json, "name");  // "Alice"
```

#### `json_get_int(json: WynJson*, key: string) -> int`
Gets integer value from JSON object.
```wyn
let age = json_get_int(json, "age");  // 30
```

#### `json_get_float(json: WynJson*, key: string) -> float`
Gets float value from JSON object.
```wyn
let score = json_get_float(json, "score");
```

#### `json_get_bool(json: WynJson*, key: string) -> bool`
Gets boolean value from JSON object.
```wyn
let active = json_get_bool(json, "active");
```

#### `json_has_key(json: WynJson*, key: string) -> bool`
Checks if JSON object has key.
```wyn
if json_has_key(json, "email") {
    let email = json_get_string(json, "email");
}
```

#### `json_free(json: WynJson*)`
Frees JSON object memory.
```wyn
json_free(json);
```

## Concurrency

### Threading

#### `spawn(fn: function) -> thread`
Spawns a new thread to execute function.
```wyn
fn worker() -> int {
    print("Worker thread running");
    return 42;
}

let thread = spawn(worker);
```

#### `thread_join(thread: thread) -> int`
Waits for thread to complete and returns result.
```wyn
let result = thread_join(thread);
print(result);  // 42
```

### Channels

#### `channel_new() -> Channel`
Creates a new channel for thread communication.
```wyn
let ch = channel_new();
```

#### `channel_send(ch: Channel, value: int)`
Sends value through channel.
```wyn
channel_send(ch, 42);
```

#### `channel_recv(ch: Channel) -> int`
Receives value from channel (blocks until available).
```wyn
let value = channel_recv(ch);
print(value);
```

## Async

### Async Functions

#### `async fn`
Declares an asynchronous function that returns a Future.
```wyn
async fn fetch_data() -> int {
    // Simulate async work
    return 42;
}
```

#### `await`
Waits for async operation to complete and returns result.
```wyn
fn main() -> int {
    let future = fetch_data();
    let result = await future;
    return result;
}
```

### Future Combinators

#### `future_map<T, U>(future: Future<T>, fn: T -> U) -> Future<U>`
Transforms future result.
```wyn
let future = fetch_data();
let mapped = future_map(future, |x| x * 2);
let result = await mapped;  // 84
```

#### `future_then<T, U>(future: Future<T>, fn: T -> Future<U>) -> Future<U>`
Chains futures together.
```wyn
let future1 = fetch_data();
let future2 = future_then(future1, |x| fetch_more_data(x));
let result = await future2;
```

## Utility Functions

### Assertions and Testing

#### `assert_eq(a: int, b: int)`
Asserts two integers are equal. Panics if not.
```wyn
let result = add(2, 3);
assert_eq(result, 5);  // Passes
```

#### `assert_eq_str(a: string, b: string)`
Asserts two strings are equal.
```wyn
let result = str_upper("hello");
assert_eq_str(result, "HELLO");
```

#### `assert_true(condition: bool)`
Asserts condition is true.
```wyn
assert_true(5 > 3);  // Passes
```

#### `assert_false(condition: bool)`
Asserts condition is false.
```wyn
assert_false(3 > 5);  // Passes
```

### Program Control

#### `panic(message: string)`
Terminates program with error message.
```wyn
if critical_error {
    panic("Critical error occurred");
}
```

#### `exit_program(code: int)`
Exits program with status code.
```wyn
if success {
    exit_program(0);  // Success
} else {
    exit_program(1);  // Error
}
```

### Memory and Performance

#### `gc_collect()`
Forces garbage collection (for debugging).
```wyn
// Create lots of objects...
gc_collect();  // Clean up
```

#### `memory_usage() -> int`
Returns current memory usage in bytes.
```wyn
let usage = memory_usage();
print(usage);
```

#### `benchmark_start() -> int`
Starts timing benchmark.
```wyn
let start = benchmark_start();
// ... code to benchmark ...
let elapsed = benchmark_end(start);
```

#### `benchmark_end(start: int) -> int`
Ends timing benchmark and returns elapsed microseconds.
```wyn
print(elapsed);  // Microseconds elapsed
```

## Type Conversion

### String Conversions

#### `int_to_string(x: int) -> string`
Converts integer to string.
```wyn
let text = int_to_string(42);  // "42"
```

#### `float_to_string(x: float) -> string`
Converts float to string.
```wyn
let text = float_to_string(3.14);  // "3.14"
```

#### `bool_to_string(x: bool) -> string`
Converts boolean to string.
```wyn
let text = bool_to_string(true);  // "true"
```

### Numeric Conversions

#### `string_to_int(s: string) -> int`
Converts string to integer. Returns 0 if invalid.
```wyn
let num = string_to_int("42");  // 42
```

#### `string_to_float(s: string) -> float`
Converts string to float. Returns 0.0 if invalid.
```wyn
let num = string_to_float("3.14");  // 3.14
```

#### `int_to_float(x: int) -> float`
Converts integer to float.
```wyn
let f = int_to_float(42);  // 42.0
```

#### `float_to_int(x: float) -> int`
Converts float to integer (truncates).
```wyn
let i = float_to_int(3.14);  // 3
```

## Error Handling

### Result Type

#### `Ok<T>(value: T) -> Result<T, E>`
Creates successful result.
```wyn
fn divide(a: int, b: int) -> Result<int, string> {
    if b == 0 {
        return Err("Division by zero");
    }
    return Ok(a / b);
}
```

#### `Err<E>(error: E) -> Result<T, E>`
Creates error result.
```wyn
return Err("Something went wrong");
```

#### `result_is_ok<T, E>(result: Result<T, E>) -> bool`
Checks if result is Ok.
```wyn
let result = divide(10, 2);
if result_is_ok(result) {
    let value = result_unwrap(result);
    print(value);
}
```

#### `result_unwrap<T, E>(result: Result<T, E>) -> T`
Extracts value from Ok result. Panics if Err.
```wyn
let value = result_unwrap(result);
```

### Optional Type

#### `Some<T>(value: T) -> Optional<T>`
Creates optional with value.
```wyn
fn find_user(id: int) -> Optional<string> {
    if id == 1 {
        return Some("Alice");
    }
    return None();
}
```

#### `None<T>() -> Optional<T>`
Creates empty optional.
```wyn
return None();
```

#### `option_is_some<T>(opt: Optional<T>) -> bool`
Checks if optional has value.
```wyn
let user = find_user(1);
if option_is_some(user) {
    let name = option_unwrap(user);
    print(name);
}
```

#### `option_unwrap<T>(opt: Optional<T>) -> T`
Extracts value from Some. Panics if None.
```wyn
let name = option_unwrap(user);
```

## See Also

- [**Language Guide**](language-guide.md) - Complete language syntax reference
- [**Getting Started**](getting-started.md) - Installation and first steps
- [**Examples**](examples.md) - Code examples and tutorials
- [**FAQ**](faq.md) - Common questions and troubleshooting

---

*This reference covers Wyn v1.4.0. For the latest updates, see the [GitHub repository](https://github.com/wynlang/wyn).*