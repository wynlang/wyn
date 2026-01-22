# Wyn Standard Library Reference v1.4.0

**Version:** 1.4.0  
**Date:** 2026-01-21

Complete reference for Wyn's standard library with object-oriented API.

---

## Philosophy

**Everything is an object.** All operations use method syntax (`.method()`) or module functions (`Module::function()`).

---

## Table of Contents

1. [I/O Functions](#io-functions)
2. [String Methods](#string-methods)
3. [Array Methods](#array-methods)
4. [Number Methods](#number-methods)
5. [HashMap Operations](#hashmap-operations)
6. [HashSet Operations](#hashset-operations)
7. [File Module](#file-module)
8. [System Module](#system-module)
9. [Time Module](#time-module)

---

## I/O Functions

### `print(value: any)`
Polymorphic print function that prints any value followed by newline.

```wyn
print(42);              // 42
print(3.14);            // 3.14
print("Hello");         // Hello
print(true);            // true
print([1, 2, 3]);       // [1, 2, 3]
```

**String Interpolation:**
```wyn
var name = "Alice";
var age = 25;
print("${name} is ${age} years old");
// Output: Alice is 25 years old
```

### `println()`
Prints a blank line (no arguments).

```wyn
println();  // Prints: \n
```

### `input() -> int`
Reads an integer from standard input.

```wyn
var num = input();
```

---

## String Methods

All string operations use method syntax on string values.

### Length & Checks

#### `.len() -> int`
Returns string length.
```wyn
var len = "hello".len();  // 5
```

#### `.is_empty() -> bool`
Checks if string is empty.
```wyn
if text.is_empty() { }
```

### Case Conversion

#### `.upper() -> string`
Converts to uppercase.
```wyn
var upper = "hello".upper();  // "HELLO"
```

#### `.lower() -> string`
Converts to lowercase.
```wyn
var lower = "HELLO".lower();  // "hello"
```

#### `.capitalize() -> string`
Capitalizes first letter.
```wyn
var cap = "john".capitalize();  // "John"
```

### Trimming

#### `.trim() -> string`
Removes leading/trailing whitespace.
```wyn
var clean = "  hello  ".trim();  // "hello"
```

#### `.trim_left() -> string`
Removes leading whitespace.

#### `.trim_right() -> string`
Removes trailing whitespace.

### Searching

#### `.contains(substr: string) -> bool`
Checks if string contains substring.
```wyn
if email.contains("@") { }
```

#### `.starts_with(prefix: string) -> bool`
Checks if string starts with prefix.
```wyn
if filename.starts_with("test") { }
```

#### `.ends_with(suffix: string) -> bool`
Checks if string ends with suffix.
```wyn
if filename.ends_with(".wyn") { }
```

#### `.index_of(substr: string) -> int`
Returns index of first occurrence (-1 if not found).
```wyn
var pos = text.index_of("hello");
```

#### `.last_index_of(substr: string) -> int`
Returns index of last occurrence.

### Manipulation

#### `.concat(other: string) -> string`
Concatenates strings.
```wyn
var result = "Hello".concat(" World");  // "Hello World"
```

#### `.replace(old: string, new: string) -> string`
Replaces first occurrence.
```wyn
var fixed = text.replace("foo", "bar");
```

#### `.replace_all(old: string, new: string) -> string`
Replaces all occurrences.
```wyn
var fixed = text.replace_all("foo", "bar");
```

#### `.repeat(count: int) -> string`
Repeats string n times.
```wyn
var stars = "*".repeat(5);  // "*****"
```

#### `.reverse() -> string`
Reverses string.
```wyn
var rev = "hello".reverse();  // "olleh"
```

### Extraction

#### `.slice(start: int, end: int) -> string`
Extracts substring.
```wyn
var sub = "hello".slice(0, 3);  // "hel"
```

#### `.substring(start: int, end: int) -> string`
Alias for slice.

#### `.split(delimiter: string) -> array<string>`
Splits string into array.
```wyn
var parts = "a,b,c".split(",");  // ["a", "b", "c"]
```

#### `.chars() -> array<string>`
Splits into character array.

#### `.lines() -> array<string>`
Splits into lines.

#### `.words() -> array<string>`
Splits into words.

### Character Checks

#### `.is_alpha() -> bool`
Checks if all characters are alphabetic.

#### `.is_digit() -> bool`
Checks if all characters are digits.

#### `.is_alnum() -> bool`
Checks if all characters are alphanumeric.

#### `.is_whitespace() -> bool`
Checks if all characters are whitespace.

#### `.char_at(index: int) -> string`
Returns character at index as a string.
```wyn
var ch = "hello".char_at(1);  // "e"
```

#### `.equals(other: string) -> bool`
Compares two strings for equality.
```wyn
if name.equals("Alice") == 1 {
    print("Hello Alice!");
}
```

#### `.count(substring: string) -> int`
Counts occurrences of substring.
```wyn
var text = "hello world hello";
var count = text.count("hello");  // 2
```

#### `.is_numeric() -> bool`
Checks if string is a valid number (int or float).
```wyn
"123".is_numeric()    // 1 (true)
"12.34".is_numeric()  // 1 (true)
"abc".is_numeric()    // 0 (false)
```

### Conversion

#### `.parse_int() -> int`
Parses string to integer.
```wyn
var num = "42".parse_int();  // 42
```

#### `.parse_float() -> float`
Parses string to float.
```wyn
var num = "3.14".parse_float();  // 3.14
```

#### `.to_int() -> int`
Alias for parse_int.

#### `.to_float() -> float`
Alias for parse_float.

---

## Array Methods

All array operations use method syntax.

### Basic Operations

#### `.len() -> int`
Returns array length.
```wyn
var len = [1, 2, 3].len();  // 3
```

#### `.push(item: T)`
Adds element to end.
```wyn
arr.push(4);
```

#### `.pop() -> T`
Removes and returns last element.
```wyn
var last = arr.pop();
```

### Searching

#### `.contains(item: T) -> bool`
Checks if array contains element.
```wyn
if numbers.contains(5) { }
```

#### `.count(item: T) -> int`
Counts occurrences of element.
```wyn
var arr = [1, 2, 3, 2, 4, 2];
var count = arr.count(2);  // 3
```

#### `.index_of(item: T) -> int`
Returns index of element (-1 if not found).
```wyn
var pos = arr.index_of(42);
```

### Modification

#### `.sort()`
Sorts array in place.
```wyn
numbers.sort();
```

#### `.reverse()`
Reverses array in place.
```wyn
arr.reverse();
```

### Extraction

#### `.slice(start: int, end: int) -> array<T>`
Extracts subarray.
```wyn
var sub = arr.slice(1, 3);
```

#### `.first() -> T`
Returns first element.
```wyn
var first = arr.first();
```

#### `.last() -> T`
Returns last element.
```wyn
var last = arr.last();
```

### Aggregation

#### `.min() -> int`
Returns minimum value.
```wyn
var min = numbers.min();
```

#### `.max() -> int`
Returns maximum value.
```wyn
var max = numbers.max();
```

#### `.sum() -> int`
Returns sum of all elements.
```wyn
var total = numbers.sum();
```

#### `.average() -> int`
Returns average value.
```wyn
var avg = numbers.average();
```

### Combination

#### `.join(separator: string) -> string`
Joins array elements into string.
```wyn
var text = words.join(" ");
```

#### `.concat(other: array<T>) -> array<T>`
Concatenates arrays.
```wyn
var combined = arr1.concat(arr2);
```

---

## Number Methods

### Integer Methods

#### `.to_string() -> string`
Converts to string.
```wyn
var str = 42.to_string();  // "42"
```

#### `.abs() -> int`
Returns absolute value.
```wyn
var abs = (-42).abs();  // 42
```

#### `.to_float() -> float`
Converts to float.
```wyn
var f = 42.to_float();  // 42.0
```

#### `.to_binary() -> string`
Converts to binary string.
```wyn
var bin = 5.to_binary();  // "101"
```

#### `.to_hex() -> string`
Converts to hexadecimal string.
```wyn
var hex = 255.to_hex();  // "ff"
```

#### `.is_even() -> bool`
Checks if even.
```wyn
if num.is_even() == 1 { }
```

#### `.is_odd() -> bool`
Checks if odd.
```wyn
if num.is_odd() == 1 { }
```

### Float Methods

#### `.to_string() -> string`
Converts to string.
```wyn
var str = 3.14.to_string();  // "3.14"
```

#### `.to_int() -> int`
Converts to integer (truncates).
```wyn
var i = 3.14.to_int();  // 3
```

---

## HashMap Operations

HashMaps use indexing syntax for get/set and methods for other operations.

### Indexing

#### `map[key]` (get)
Gets value by key.
```wyn
var scores = {"alice": 95, "bob": 87};
var alice_score = scores["alice"];  // 95
```

#### `map[key] = value` (set)
Sets value by key.
```wyn
scores["charlie"] = 92;
```

### Methods

#### `.has(key: string) -> bool`
Checks if key exists.
```wyn
if scores.has("alice") { }
```

#### `.remove(key: string)`
Removes key-value pair.
```wyn
scores.remove("bob");
```

#### `.len() -> int`
Returns number of entries.
```wyn
var count = scores.len();
```

---

## HashSet Operations

HashSets use method syntax for all operations.

### Methods

#### `.add(item: string)`
Adds element to set.
```wyn
tags.add("urgent");
```

#### `.contains(item: string) -> bool`
Checks if element exists.
```wyn
if tags.contains("urgent") { }
```

#### `.remove(item: string)`
Removes element.
```wyn
tags.remove("urgent");
```

#### `.len() -> int`
Returns number of elements.
```wyn
var count = tags.len();
```

---

## File Module

All file operations use `File::` prefix.

### Reading & Writing

#### `File::read(path: string) -> string`
Reads entire file as string.
```wyn
var content = File::read("/tmp/file.txt");
```

#### `File::write(path: string, data: string) -> bool`
Writes string to file.
```wyn
File::write("/tmp/file.txt", "Hello");
```

### Checking

#### `File::exists(path: string) -> bool`
Checks if file/directory exists.
```wyn
if File::exists("/tmp/file.txt") { }
```

#### `File::is_file(path: string) -> bool`
Checks if path is a file.
```wyn
if File::is_file(path) { }
```

#### `File::is_dir(path: string) -> bool`
Checks if path is a directory.
```wyn
if File::is_dir(path) { }
```

### Directory Operations

#### `File::list_dir(path: string) -> array<string>`
Lists directory contents.
```wyn
var entries = File::list_dir(".");
```

#### `File::create_dir(path: string) -> bool`
Creates directory.
```wyn
File::create_dir("/tmp/mydir");
```

#### `File::get_cwd() -> string`
Gets current working directory.
```wyn
var cwd = File::get_cwd();
```

### File Operations

#### `File::delete(path: string) -> bool`
Deletes file.
```wyn
File::delete("/tmp/file.txt");
```

#### `File::file_size(path: string) -> int`
Gets file size in bytes.
```wyn
var size = File::file_size(path);
```

### Path Operations

#### `File::path_join(a: string, b: string) -> string`
Joins path components safely.
```wyn
var path = File::path_join(dir, filename);
```

#### `File::basename(path: string) -> string`
Extracts filename from path.
```wyn
var name = File::basename("/tmp/file.txt");  // "file.txt"
```

#### `File::dirname(path: string) -> string`
Extracts directory from path.
```wyn
var dir = File::dirname("/tmp/file.txt");  // "/tmp"
```

#### `File::extension(path: string) -> string`
Extracts file extension.
```wyn
var ext = File::extension("file.txt");  // "txt"
```

---

## System Module

All system operations use `System::` prefix.

#### `System::exec(command: string) -> string`
Executes shell command and returns output.
```wyn
var output = System::exec("ls -la");
```

#### `System::exec_code(command: string) -> int`
Executes command and returns exit code.
```wyn
var code = System::exec_code("make test");
if code == 0 {
    print("Success!");
}
```

#### `System::exit(code: int)`
Exits program with status code.
```wyn
System::exit(0);
```

#### `System::args() -> array<string>`
Gets command-line arguments.
```wyn
var args = System::args();
if args.len() > 1 {
    var first_arg = args[1];
}
```

#### `System::env(key: string) -> string`
Gets environment variable.
```wyn
var home = System::env("HOME");
```

#### `System::set_env(key: string, value: string) -> bool`
Sets environment variable.
```wyn
System::set_env("MY_VAR", "value");
```

#### `System::args() -> array<string>`
Gets command line arguments.
```wyn
var args = System::args();
```

---

## Time Module

All time operations use `Time::` prefix.

#### `Time::now() -> int`
Gets current Unix timestamp.
```wyn
var timestamp = Time::now();
```

#### `Time::sleep(milliseconds: int)`
Sleeps for specified milliseconds.
```wyn
Time::sleep(1000);  // Sleep 1 second
```

#### `Time::format(timestamp: int) -> string`
Formats timestamp as human-readable string.
```wyn
var now = Time::now();
var formatted = Time::format(now);  // "2026-01-22 14:30:45"
```

---

## Net Module

Basic TCP networking operations.

#### `Net::listen(port: int) -> int`
Creates TCP server socket. Returns socket descriptor or -1 on error.
```wyn
var server = Net::listen(8080);
if server == -1 {
    print("Error: Could not start server\n");
}
```

#### `Net::connect(host: string, port: int) -> int`
Connects to TCP server. Returns socket descriptor or -1 on error.
```wyn
var client = Net::connect("127.0.0.1", 8080);
if client == -1 {
    print("Error: Could not connect\n");
}
```

#### `Net::send(socket: int, data: string) -> int`
Sends data through socket. Returns bytes sent or -1 on error.
```wyn
var sent = Net::send(socket, "Hello\n");
```

#### `Net::recv(socket: int) -> string`
Receives data from socket. Returns data or empty string on error.
```wyn
var data = Net::recv(socket);
if data.len() > 0 {
    print(data);
}
```

#### `Net::close(socket: int) -> int`
Closes socket. Returns 1 on success, 0 on error.
```wyn
Net::close(socket);
```

---

## Error Handling

Wyn uses return codes for error handling. Functions return special values to indicate errors.

### Error Patterns

#### Return -1 for Errors
```wyn
fn divide(a: int, b: int) -> int {
    if b == 0 {
        return -1;  // Error indicator
    }
    return a / b;
}

// Check for errors
var result = divide(10, 0);
if result == -1 {
    print("Error: division by zero\n");
}
```

#### Return Empty String for Errors
```wyn
fn read_file(path: string) -> string {
    if !File::exists(path) {
        return "";  // Error indicator
    }
    return File::read(path);
}

// Check for errors
var content = read_file("missing.txt");
if content.len() == 0 {
    print("Error: could not read file\n");
}
```

#### Use Exit Codes
```wyn
fn main() -> int {
    if some_error {
        return 1;  // Non-zero indicates error
    }
    return 0;  // Success
}
```

---

## Examples

### String Processing
```wyn
var email = "  USER@EXAMPLE.COM  ";
var cleaned = email.trim().lower();
if cleaned.contains("@") && cleaned.ends_with(".com") {
    print("Valid email: ${cleaned}");
}
```

### Array Processing
```wyn
var numbers = [5, 2, 8, 1, 9];
numbers.sort();
var min = numbers.min();
var max = numbers.max();
var avg = numbers.average();
print("Min: ${min}, Max: ${max}, Avg: ${avg}");
```

### File Operations
```wyn
var entries = File::list_dir(".");
for i in 0..entries.len() {
    var name = entries[i];
    if File::is_file(name) && name.ends_with(".wyn") {
        var size = File::file_size(name);
        print("${name}: ${size} bytes");
    }
}
```

### HashMap Usage
```wyn
var scores = {"alice": 95, "bob": 87};
if scores.has("alice") {
    var score = scores["alice"];
    print("Alice: ${score}");
}
scores["charlie"] = 92;
```

---

## See Also

- [Examples](../examples/)
- [CHANGELOG](../CHANGELOG.md)
- [GitHub Repository](https://github.com/wyn-lang/wyn)
