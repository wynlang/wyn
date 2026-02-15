# Wyn Examples

Real-world code patterns for common tasks.

## Hello World
```wyn
fn main() -> int {
    println("Hello, World!")
    return 0
}
```

## HTTP API Client
```wyn
fn main() -> int {
    var body = Http.get("https://api.example.com/data")
    println(body)
    
    var response = Http.post("https://api.example.com/items", "name=widget")
    println(response)
    return 0
}
```

## REST API Server
```wyn
fn main() -> int {
    var server = Http.serve(8080)
    println("Listening on :8080")
    
    var sep = "|"
    while 1 == 1 {
        var req = Http.accept(server)
        var method = req.split_at(sep, 0)
        var path = req.split_at(sep, 1)
        var fd = req.split_at(sep, 3).to_int()
        
        if path == "/api/health" {
            Http.respond(fd, 200, "application/json", "{\"status\":\"ok\"}")
        } else {
            Http.respond(fd, 404, "text/plain", "not found")
        }
    }
    return 0
}
```

## SQLite Database
```wyn
fn main() -> int {
    var db = Db.open("app.db")
    Db.exec(db, "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, name TEXT)")
    Db.exec(db, "INSERT INTO users (name) VALUES ('Alice')")
    
    var count = Db.query_one(db, "SELECT COUNT(*) FROM users")
    println("Users: ${count}")
    
    var rows = Db.query(db, "SELECT id, name FROM users")
    println(rows)
    
    Db.close(db)
    return 0
}
```

## Concurrent Processing
```wyn
fn process(data: int) -> int {
    var result = 0
    for i in 0..data { result = result + i }
    return result
}

fn main() -> int {
    var f1 = spawn process(1000000)
    var f2 = spawn process(2000000)
    var f3 = spawn process(3000000)
    var f4 = spawn process(4000000)
    
    var total = await f1 + await f2 + await f3 + await f4
    println("Total: ${total}")
    return 0
}
```

## Shared State Between Tasks
```wyn
fn worker(counter: int, amount: int) -> int {
    Task.add(counter, amount)
    return 0
}

fn main() -> int {
    var counter = Task.value(0)
    
    var f1 = spawn worker(counter, 100)
    var f2 = spawn worker(counter, 200)
    await f1
    await f2
    
    println("Counter: ${Task.get(counter)}")
    return 0
}
```

## File Processing
```wyn
fn main() -> int {
    // Stream large file line by line
    var fh = File.open("data.csv", "r")
    var header = File.read_line(fh).trim()
    
    var count = 0
    while File.eof(fh) == 0 {
        var line = File.read_line(fh).trim()
        if line.len() > 0 { count = count + 1 }
    }
    File.close(fh)
    
    println("Processed ${count} rows")
    return 0
}
```

## Data Pipeline
```wyn
fn double_val(x: int) -> int { return x * 2 }
fn is_big(x: int) -> int { return x > 50 }
fn add(a: int, b: int) -> int { return a + b }

fn main() -> int {
    var data = [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]
    var result = data.map(double_val).filter(is_big).reduce(add, 0)
    println("Pipeline result: ${result}")
    return 0
}
```

## Error Handling
```wyn
fn parse_config(path: string) -> ResultInt {
    if File.exists(path) == 0 { return Err("file not found") }
    var content = File.read(path)
    return Ok(content.len())
}

fn load_app(config_path: string) -> ResultInt {
    var size = parse_config(config_path)?
    return Ok(size)
}

fn main() -> int {
    var result = load_app("app.conf")
    if result.is_err() {
        println("Error: ${result.unwrap_err()}")
    }
    return 0
}
```

## GUI Application
```wyn
fn main() -> int {
    Gui.create("My App", 800, 600)
    
    while Gui.running() > 0 {
        var ev = Gui.poll()
        // Handle events...
        
        Gui.clear(30, 30, 40)
        Gui.color(255, 255, 255)
        Gui.text(20, 20, "HELLO WYN", 3)
        Gui.button(20, 80, 150, 40, "CLICK ME")
        Gui.progress(20, 140, 300, 25, 75)
        Gui.present()
        Gui.delay(16)
    }
    
    Gui.destroy()
    return 0
}
```

## Testing
```wyn
fn add(a: int, b: int) -> int { return a + b }

fn main() -> int {
    Test.init("Math Tests")
    Test.assert_eq_int(add(2, 3), 5, "2+3=5")
    Test.assert_eq_int(add(-1, 1), 0, "-1+1=0")
    Test.assert(add(1, 1) > 0, "positive")
    Test.summary()
    return 0
}
```
