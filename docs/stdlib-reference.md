# Wyn Standard Library Reference

![Version](https://img.shields.io/badge/version-1.6.0-blue)

Complete reference for Wyn's built-in modules and methods.

## String Methods

Called directly on string values using dot syntax.

```wyn
let text = "Hello World"
println(text.len())           // 11
println(text.upper())         // "HELLO WORLD"
println(text.lower())         // "hello world"
println(text.trim())          // Removes whitespace
println(text.contains("World")) // true
println(text.starts_with("Hello")) // true
println(text.ends_with("World"))   // true
println(text.replace("World", "Wyn")) // "Hello Wyn"
println(text.index_of("o"))   // 4
println(text.last_index_of("o")) // 7
println(text.substring(0, 5)) // "Hello"
println(text.pad_left(15, "*")) // "****Hello World"
println(text.pad_right(15, "*")) // "Hello World****"
println(text.split_at(" ", 0)) // "Hello"
println("123".to_int())       // 123
println("3.14".to_float())    // 3.14
```

## Array Methods

```wyn
let arr = [1, 2, 3]
println(arr.len())            // 3
arr.push(4)                   // [1, 2, 3, 4]
println(arr[0])               // 1
arr[0] = 10                   // [10, 2, 3, 4]
```

## Integer Methods

```wyn
let num = 42
println(num.to_string())      // "42"
```

## File Module

File system operations.

```wyn
let content = File.read("config.txt")
File.write("output.txt", "Hello Wyn")
if File.exists("data.json") {
    println("File exists")
}
File.delete("temp.txt")
```

## System Module

System interaction and process control.

```wyn
let output = System.exec("ls -la")
let exit_code = System.exec_code("make build")
let home = System.env("HOME")
let args = System.args()
System.exit(0)
```

## Terminal Module

Terminal control and input handling.

```wyn
let cols = Terminal.cols()
let rows = Terminal.rows()
Terminal.raw_mode()
let key = Terminal.read_key()  // 0=none, 113=q, 1000=Up, 1001=Down, 1002=Right, 1003=Left, 27=Esc, 13=Enter
Terminal.clear()
Terminal.move(10, 5)
Terminal.write("Hello")
Terminal.restore()
```

## HashMap Module

Key-value storage.

```wyn
let map = HashMap.new()
map.insert_int("count", 42)
map.insert_string("name", "Wyn")
let count = map.get_int("count")
let name = map.get("name")
if map.contains("count") {
    println("Key exists")
}
```

## Math Module

Mathematical operations and constants.

```wyn
println(Math.abs(-5))         // 5
println(Math.max(10, 20))     // 20
println(Math.min(10, 20))     // 10
println(Math.sqrt(16))        // 4
println(Math.pow(2, 3))       // 8
println(Math.sin(Math.PI / 2)) // 1
println(Math.cos(0))          // 1
println(Math.tan(Math.PI / 4)) // 1
println(Math.floor(3.7))      // 3
println(Math.ceil(3.2))       // 4
println(Math.round(3.6))      // 4
println(Math.random())        // Random float 0-1
println(Math.PI)              // 3.14159...
println(Math.E)               // 2.71828...
```

## DateTime Module

Date and time handling.

```wyn
let now = DateTime.now()      // Unix timestamp
let formatted = DateTime.format(now)
println(formatted)
```

## Path Module

File path manipulation.

```wyn
let file = "/home/user/document.txt"
println(Path.basename(file))  // "document.txt"
println(Path.dirname(file))   // "/home/user"
println(Path.extension(file)) // ".txt"
let joined = Path.join("/home", "user/file.txt")
```

## Json Module

JSON data handling.

```wyn
let json = Json.new()
Json.set(json, "name", "Wyn")
Json.set_int(json, "version", 1)
let json_str = Json.stringify(json)
```

## Regex Module

Regular expression operations.

```wyn
let match_pos = Regex.match("hello123", r"\d+") // Position of match
let replaced = Regex.replace("hello world", r"world", "Wyn")
```

## Test Module

Unit testing framework.

```wyn
Test.init("Math Tests")
Test.describe("Basic operations")
Test.assert(2 + 2 == 4, "Addition works")
Test.assert_eq_int(Math.max(5, 3), 5, "Max function")
Test.assert_eq_str("hello".upper(), "HELLO", "String upper")
Test.assert_gt(10, 5, "Greater than")
Test.assert_lt(3, 7, "Less than")
Test.assert_gte(5, 5, "Greater or equal")
Test.assert_lte(4, 6, "Less or equal")
Test.assert_contains("hello world", "world", "Contains substring")
Test.skip("Not implemented yet")
let failures = Test.summary()
```

## Task Module

Concurrency and communication.

```wyn
let counter = Task.value(0)
Task.set(counter, 10)
Task.add(counter, 5)
let val = Task.get(counter)

let ch = Task.channel(10)
Task.send(ch, 42)
let received = Task.recv(ch)
Task.close(ch)
```

## Net Module

Network operations.

```wyn
let server = Net.listen(8080)
let client = Net.connect("localhost", 8080)
Net.send(client, "Hello Server")
let response = Net.recv(server)
```

## Result and Option Types

Error handling and optional values.

```wyn
let success = Ok(42)
let failure = Err("Something went wrong")
let some_val = Some(10)
let no_val = None()

if success.is_ok() {
    println(success.unwrap())
}

if failure.is_err() {
    println(failure.unwrap_err())
}

if some_val.is_some() {
    println(some_val.unwrap())
}

let default_val = no_val.unwrap_or(0)
```

## I/O Functions

Basic input/output operations.

```wyn
print("Hello ")
println("World!")
```

## Concurrency

Asynchronous execution with spawn and await.

```wyn
fn background_task(n) {
    return n * 2
}

let future = spawn background_task(21)
let result = await future  // 42
```

---

*This reference covers all modules and methods available in Wyn v1.6.0. All examples are tested and verified working.*