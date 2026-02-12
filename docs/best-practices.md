# Wyn Best Practices

## Code Style

```wyn
// Use var for mutable, const for immutable
var count = 0
const MAX_SIZE = 1024

// Functions: verb_noun naming
fn calculate_total(items: [int]) -> int { ... }

// Structs: PascalCase
struct HttpRequest { method: string, path: string }

// Enums: PascalCase with PascalCase variants
enum Status { Active, Inactive, Pending }
```

## Error Handling

Use `ResultInt`/`ResultString` with `?` for propagation:

```wyn
fn load_config(path: string) -> ResultInt {
    if File.exists(path) == 0 { return Err("not found: " + path) }
    var content = File.read(path)
    return Ok(content.len())
}

fn init_app() -> ResultInt {
    var config_size = load_config("app.conf")?  // propagates error
    return Ok(config_size)
}
```

## Concurrency

Prefer `spawn`/`await` for parallel work. Use `Task.value` for shared state:

```wyn
// Good: parallel independent work
var f1 = spawn process(chunk1)
var f2 = spawn process(chunk2)
var r1 = await f1
var r2 = await f2

// Good: shared counter
var counter = Task.value(0)
spawn worker(counter)
Task.add(counter, 1)  // thread-safe

// Use channels for producer/consumer
var ch = Task.channel(100)
spawn producer(ch)
var val = Task.recv(ch)
```

## Strings

Use interpolation for readability, concatenation for performance:

```wyn
// Readable
println("Hello ${name}, you have ${count} items")

// For building in loops, use concatenation
var result = ""
for i in 0..n { result = result + i.to_string() + "," }
```

## Collections

Use functional methods for data pipelines:

```wyn
fn is_valid(x: int) -> int { return x > 0 }
fn transform(x: int) -> int { return x * 2 }
fn combine(a: int, b: int) -> int { return a + b }

var result = data.filter(is_valid).map(transform).reduce(combine, 0)
```

## File I/O

Use streaming for large files:

```wyn
// Small files: read all at once
var content = File.read("small.txt")

// Large files: stream line by line
var fh = File.open("large.csv", "r")
while File.eof(fh) == 0 {
    var line = File.read_line(fh).trim()
    // process line...
}
File.close(fh)
```

## Testing

Write tests alongside your code:

```wyn
fn main() -> int {
    Test.init("My Module")
    Test.assert_eq_int(add(2, 3), 5, "basic add")
    Test.assert(result.is_ok(), "no error")
    Test.assert_eq_str(name, "expected", "name matches")
    Test.summary()  // prints results, returns 0 on success
    return 0
}
```
