# Wyn Standard Library Reference — v1.6.0

26 modules, 260+ methods. All cross-platform (POSIX).

---

## String (29 methods)
Instance methods on `string` values.

| Method | Signature | Description |
|--------|-----------|-------------|
| `len()` | `-> int` | Length in bytes |
| `upper()` | `-> string` | Uppercase |
| `lower()` | `-> string` | Lowercase |
| `trim()` | `-> string` | Strip whitespace |
| `trim_left()` | `-> string` | Strip leading whitespace |
| `trim_right()` | `-> string` | Strip trailing whitespace |
| `contains(s)` | `-> bool` | Substring check |
| `starts_with(s)` | `-> bool` | Prefix check |
| `ends_with(s)` | `-> bool` | Suffix check |
| `index_of(s)` | `-> int` | First occurrence (-1 if not found) |
| `last_index_of(s)` | `-> int` | Last occurrence |
| `substring(start, end)` | `-> string` | Extract range |
| `slice(start, end)` | `-> string` | Extract range (alias) |
| `replace(old, new)` | `-> string` | Replace all occurrences |
| `repeat(n)` | `-> string` | Repeat n times |
| `reverse()` | `-> string` | Reverse characters |
| `capitalize()` | `-> string` | First char uppercase |
| `title()` | `-> string` | Title case |
| `pad_left(n, ch)` | `-> string` | Left-pad to width |
| `pad_right(n, ch)` | `-> string` | Right-pad to width |
| `split(delim)` | `-> [string]` | Split (preserves empty fields) |
| `split_at(delim, idx)` | `-> string` | Get nth split segment |
| `count(s)` | `-> int` | Count occurrences |
| `to_int()` | `-> int` | Parse integer |
| `is_alpha()` | `-> bool` | All alphabetic |
| `is_digit()` | `-> bool` | All numeric |
| `is_alnum()` | `-> bool` | All alphanumeric |
| `to_string()` | `-> string` | Identity (also works on int) |

String interpolation: `"hello ${name}"` — works with variables and expressions.

---

## Array (19 methods)
Instance methods on `[int]` or `[string]` arrays.

| Method | Signature | Description |
|--------|-----------|-------------|
| `len()` | `-> int` | Element count |
| `push(val)` | | Append element |
| `pop()` | `-> int` | Remove and return last |
| `slice(start, end)` | `-> array` | Sub-array |
| `reverse()` | `-> array` | Reversed copy |
| `join(sep)` | `-> string` | Join with separator |
| `index_of(val)` | `-> int` | Find element (-1 if missing) |
| `remove(idx)` | | Remove at index |
| `insert(idx, val)` | | Insert at index |
| `unique()` | `-> array` | Deduplicated copy |
| `concat(other)` | `-> array` | Concatenate arrays |
| `any(fn)` | `-> bool` | Any element matches |
| `all(fn)` | `-> bool` | All elements match |
| `map(fn)` | `-> array` | Transform elements |
| `filter(fn)` | `-> array` | Filter elements |
| `reduce(fn, init)` | `-> int` | Fold elements |
| `sort_by(cmp_fn)` | | Sort with comparator |

---

## HashMap (12 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `HashMap.new()` | `-> map` | Create empty map |
| `insert_int(key, val)` | | Set integer value |
| `insert_string(key, val)` | | Set string value |
| `get(key)` | `-> string` | Get string ("" if missing) |
| `get_int(key)` | `-> int` | Get integer (0 if missing) |
| `get_or(key, default)` | `-> int` | Get with default |
| `contains(key)` | `-> bool` | Key exists |
| `keys()` | `-> string` | Comma-separated keys |
| `len()` | `-> int` | Entry count |
| `remove(key)` | | Delete entry |
| `clear()` | | Remove all entries |
| `values()` | `-> string` | Comma-separated values |

---

## Math (20 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `Math.abs(x)` | `-> int` | Absolute value |
| `Math.max(a, b)` | `-> int` | Maximum |
| `Math.min(a, b)` | `-> int` | Minimum |
| `Math.pow(base, exp)` | `-> int` | Power |
| `Math.sqrt(x)` | `-> float` | Square root |
| `Math.clamp(x, lo, hi)` | `-> int` | Clamp to range |
| `Math.sign(x)` | `-> int` | -1, 0, or 1 |
| `Math.lerp(a, b, t)` | `-> float` | Linear interpolation |
| `Math.map_range(x, a1, a2, b1, b2)` | `-> float` | Map between ranges |
| `Math.log(x)` | `-> float` | Natural log |
| `Math.log10(x)` | `-> float` | Base-10 log |
| `Math.exp(x)` | `-> float` | e^x |
| `Math.random()` | `-> int` | Random integer |

---

## DateTime (16 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `DateTime.now()` | `-> int` | Unix timestamp (seconds) |
| `DateTime.millis()` | `-> int` | Milliseconds since epoch |
| `DateTime.micros()` | `-> int` | Microseconds since epoch |
| `DateTime.format(ts, fmt)` | `-> string` | Format timestamp |
| `DateTime.to_iso(ts)` | `-> string` | ISO 8601 string |
| `DateTime.year(ts)` | `-> int` | Year component |
| `DateTime.month(ts)` | `-> int` | Month (1-12) |
| `DateTime.day(ts)` | `-> int` | Day (1-31) |
| `DateTime.hour(ts)` | `-> int` | Hour (0-23) |
| `DateTime.minute(ts)` | `-> int` | Minute (0-59) |
| `DateTime.second(ts)` | `-> int` | Second (0-59) |
| `DateTime.day_of_week(ts)` | `-> int` | Day of week (0=Sun) |
| `DateTime.diff(a, b)` | `-> int` | Difference in seconds |
| `DateTime.add_seconds(ts, n)` | `-> int` | Add seconds |
| `DateTime.format_duration(ms)` | `-> string` | Human-readable duration |

---

## File (22 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `File.read(path)` | `-> string` | Read entire file |
| `File.write(path, data)` | | Write file (overwrite) |
| `File.append(path, data)` | | Append to file |
| `File.exists(path)` | `-> bool` | File exists |
| `File.delete(path)` | | Delete file |
| `File.size(path)` | `-> int` | File size (0 if missing) |
| `File.is_file(path)` | `-> bool` | Is regular file |
| `File.is_dir(path)` | `-> bool` | Is directory |
| `File.open(path, mode)` | `-> int` | Open file handle |
| `File.close(handle)` | | Close file handle |
| `File.read_line(handle)` | `-> string` | Read one line |
| `File.rename(old, new)` | | Rename/move file |
| `File.copy(src, dst)` | | Copy file |
| `File.mkdir(path)` | | Create directory |
| `File.list_dir(path)` | `-> [string]` | List directory |
| `File.glob(pattern)` | `-> string` | Glob match |
| `File.walk_dir(path)` | `-> string` | Recursive listing |
| `File.temp_file()` | `-> string` | Create temp file path |
| `File.cwd()` | `-> string` | Current directory |

---

## Json (16 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `Json.new()` | `-> json` | Create empty object |
| `Json.parse(str)` | `-> json` | Parse JSON string |
| `Json.stringify(j)` | `-> string` | Serialize to string |
| `Json.to_pretty_string(j)` | `-> string` | Pretty-print JSON |
| `Json.get(j, key)` | `-> string` | Get string value |
| `Json.get_int(j, key)` | `-> int` | Get integer value |
| `Json.get_float(j, key)` | `-> float` | Get float value |
| `Json.get_bool(j, key)` | `-> bool` | Get boolean value |
| `Json.get_array(j, key)` | `-> json` | Get array node |
| `Json.get_object(j, key)` | `-> json` | Get object node |
| `Json.set(j, key, val)` | | Set string value |
| `Json.set_int(j, key, val)` | | Set integer value |
| `Json.set_bool(j, key, val)` | | Set boolean value |
| `Json.has(j, key)` | `-> bool` | Key exists |
| `Json.keys(j)` | `-> string` | Comma-separated keys |
| `Json.array_len(j)` | `-> int` | Array length |

---

## Csv (7 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `Csv.parse(text)` | `-> csv` | Parse CSV (handles quoted fields) |
| `Csv.row_count(doc)` | `-> int` | Number of rows (including header) |
| `Csv.col_count(doc, row)` | `-> int` | Columns in row |
| `Csv.get(doc, row, col)` | `-> string` | Get cell by index |
| `Csv.get_field(doc, row, header)` | `-> string` | Get cell by header name |
| `Csv.header(doc, col)` | `-> string` | Get header name |
| `Csv.header_count(doc)` | `-> int` | Number of headers |

---

## Encoding (4 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `Encoding.base64_encode(s)` | `-> string` | Base64 encode |
| `Encoding.base64_decode(s)` | `-> string` | Base64 decode |
| `Encoding.hex_encode(s)` | `-> string` | Hex encode |
| `Encoding.hex_decode(s)` | `-> string` | Hex decode |

---

## Crypto (4 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `Crypto.sha256(s)` | `-> string` | SHA-256 hash (64 hex chars) |
| `Crypto.md5(s)` | `-> string` | MD5 hash (32 hex chars) |
| `Crypto.hmac_sha256(key, data)` | `-> string` | HMAC-SHA256 |
| `Crypto.random_bytes(n)` | `-> string` | Random bytes (hex encoded) |

---

## Regex (5 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `Regex.match(str, pattern)` | `-> bool` | Pattern matches |
| `Regex.replace(str, pattern, repl)` | `-> string` | Replace all matches |
| `Regex.find(str, pattern)` | `-> int` | First match position (-1 if none) |
| `Regex.find_all(str, pattern)` | `-> string` | All matches |
| `Regex.split(str, pattern)` | `-> string` | Split by pattern |

---

## Path (4 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `Path.basename(path)` | `-> string` | Filename component |
| `Path.dirname(path)` | `-> string` | Directory component |
| `Path.extension(path)` | `-> string` | File extension |
| `Path.join(a, b)` | `-> string` | Join paths |

---

## Http (8 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `Http.get(url)` | `-> string` | HTTP/HTTPS GET |
| `Http.post(url, body, content_type)` | `-> string` | HTTP POST |
| `Http.get_json(url)` | `-> json` | GET + JSON parse |
| `Http.post_json(url, data)` | `-> json` | POST + JSON parse |
| `Http.serve(port)` | `-> int` | Start HTTP server |
| `Http.accept(server)` | `-> int` | Accept connection |
| `Http.respond(client, status, body)` | | Send response |
| `Http.set_timeout(seconds)` | | Set request timeout |
| `Http.timeout()` | `-> int` | Get current timeout |

---

## Net (6 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `Net.connect(host, port)` | `-> int` | TCP connect |
| `Net.send(sock, data)` | `-> int` | Send data |
| `Net.recv(sock, size)` | `-> string` | Receive data |
| `Net.close(sock)` | | Close socket |
| `Net.listen(port)` | `-> int` | Listen on port |
| `Net.resolve(host)` | `-> string` | DNS resolve |

---

## Db (9 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `Db.open(path)` | `-> int` | Open SQLite database |
| `Db.exec(db, sql)` | | Execute SQL |
| `Db.query(db, sql)` | `-> string` | Query (tab-separated) |
| `Db.query_one(db, sql)` | `-> string` | Single value query |
| `Db.last_insert_id(db)` | `-> int` | Last inserted row ID |
| `Db.table_exists(db, name)` | `-> bool` | Table exists |
| `Db.escape(s)` | `-> string` | SQL escape |
| `Db.close(db)` | | Close database |

---

## System (5 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `System.exec(cmd)` | `-> string` | Execute and capture output |
| `System.exec_code(cmd)` | `-> int` | Execute and return exit code |
| `System.env(name)` | `-> string` | Get environment variable |
| `System.exit(code)` | | Exit process |
| `System.args()` | `-> [string]` | Command-line arguments |

---

## Os (6 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `Os.platform()` | `-> string` | "macos", "linux", "windows" |
| `Os.arch()` | `-> string` | "arm64", "x86_64" |
| `Os.hostname()` | `-> string` | Machine hostname |
| `Os.pid()` | `-> int` | Process ID |
| `Os.home_dir()` | `-> string` | Home directory |
| `Os.temp_dir()` | `-> string` | Temp directory |

---

## Process (2 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `Process.exec_capture(cmd)` | `-> string` | Execute and capture output |
| `Process.exec_status(cmd)` | `-> int` | Execute and return status |

---

## Terminal (4 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `Terminal.cols()` | `-> int` | Terminal width |
| `Terminal.rows()` | `-> int` | Terminal height |
| `Terminal.raw_mode(on)` | | Enable/disable raw mode |
| `Terminal.read_key()` | `-> int` | Read single keypress |

---

## Task (7 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `Task.value(init)` | `-> int` | Create shared atomic value |
| `Task.get(v)` | `-> int` | Read shared value |
| `Task.set(v, val)` | | Write shared value |
| `Task.add(v, n)` | | Atomic add |
| `Task.channel(cap)` | `-> int` | Create bounded channel |
| `Task.send(ch, val)` | | Send to channel |
| `Task.recv(ch)` | `-> int` | Receive from channel |

---

## StringBuilder (7 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `StringBuilder.new()` | `-> int` | Create builder |
| `StringBuilder.append(sb, s)` | | Append string |
| `StringBuilder.append_int(sb, n)` | | Append integer |
| `StringBuilder.append_line(sb, s)` | | Append with newline |
| `StringBuilder.to_string(sb)` | `-> string` | Build final string |
| `StringBuilder.len(sb)` | `-> int` | Current length |
| `StringBuilder.clear(sb)` | | Reset to empty |
| `StringBuilder.free(sb)` | | Free memory |

---

## Uuid (1 method)

| Method | Signature | Description |
|--------|-----------|-------------|
| `Uuid.generate()` | `-> string` | Generate v4 UUID |

---

## Log (5 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `Log.debug(msg)` | | Debug message (gray) |
| `Log.info(msg)` | | Info message (green) |
| `Log.warn(msg)` | | Warning message (yellow) |
| `Log.error(msg)` | | Error message (red) |
| `Log.set_level(level)` | | Set minimum level (0-3) |

---

## Url (2 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `Url.encode(s)` | `-> string` | URL-encode |
| `Url.decode(s)` | `-> string` | URL-decode |

---

## Test (12 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `Test.init(name)` | | Start test suite |
| `Test.describe(name)` | | Start test group |
| `Test.assert(cond, msg)` | | Assert truthy |
| `Test.assert_eq_int(a, b, msg)` | | Assert integers equal |
| `Test.assert_eq_str(a, b, msg)` | | Assert strings equal |
| `Test.assert_eq_float(a, b, msg)` | | Assert floats equal |
| `Test.assert_not_contains(s, sub, msg)` | | Assert not contains |
| `Test.skip(msg)` | | Skip test |
| `Test.summary()` | | Print results |

---

## Gui (30+ methods) — requires SDL2

| Method | Description |
|--------|-------------|
| `Gui.create(title, w, h)` | Create window |
| `Gui.clear(r, g, b)` | Clear screen |
| `Gui.present()` | Flip buffer |
| `Gui.rect(x, y, w, h, r, g, b)` | Draw rectangle |
| `Gui.line(x1, y1, x2, y2, r, g, b)` | Draw line |
| `Gui.circle(cx, cy, radius, r, g, b)` | Draw circle |
| `Gui.text(x, y, str, r, g, b)` | Draw text |
| `Gui.button(x, y, w, h, label)` | Draw button (returns click) |
| `Gui.panel(x, y, w, h, r, g, b)` | Draw panel |
| `Gui.text_input(x, y, w, h)` | Text input widget |
| `Gui.progress(x, y, w, h, pct)` | Progress bar |
| `Gui.poll_event()` | Poll for events |
| `Gui.key_pressed(key)` | Check key state |
| `Gui.mouse_x()` / `Gui.mouse_y()` | Mouse position |
| `Gui.mouse_clicked()` | Mouse button state |
| `Gui.load_sprite(path)` | Load BMP sprite |
| `Gui.draw_sprite(id, x, y)` | Draw sprite |
| `Gui.delay(ms)` | Frame delay |
| `Gui.close()` | Close window |

---

## Audio (5 methods) — requires SDL2_mixer

| Method | Description |
|--------|-------------|
| `Audio.init()` | Initialize audio |
| `Audio.load(path)` | Load audio file |
| `Audio.play(id)` | Play audio |
| `Audio.stop()` | Stop playback |
| `Audio.close()` | Close audio |

---

## List Comprehensions

```wyn
var squares = [x * x for x in 0..10]
var evens = [x for x in 0..20 if x % 2 == 0]
var doubled = [x * 2 for x in some_array]
var big = [x for x in nums if x > 100]
```

## Slice Syntax

```wyn
var sub = arr[1:3]              // array slice
var sub = arr[2..5]             // same with ..
var hello = "hello world"[0:5]  // string slice
```

---

## Unified Struct Syntax

Fields and methods together — no separate `impl` block needed:

```wyn
struct Vec2 {
    x: int
    y: int

    fn mag_sq(self) -> int {
        return self.x * self.x + self.y * self.y
    }

    fn dot(self, other: Vec2) -> int {
        return self.x * other.x + self.y * other.y
    }
}

var v = Vec2{x: 3, y: 4}
println(v.mag_sq().to_string())  // 25
```

Use `impl` only for trait implementations:

```wyn
impl Drawable for Circle {
    fn draw(self) -> string { return "circle" }
}
```

---

## Arrow Lambdas

Short syntax for inline functions:

```wyn
var doubled = nums.map(fn(x) => x * 2)
var evens = nums.filter(fn(x) => x % 2 == 0)
var big = [1,2,3,4,5].filter(fn(x) => x > 3).map(fn(x) => x * 10)
```

## Additional Math Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `Math.sin(x)` | `-> float` | Sine |
| `Math.cos(x)` | `-> float` | Cosine |
| `Math.tan(x)` | `-> float` | Tangent |
| `Math.atan2(y, x)` | `-> float` | Two-argument arctangent |
| `Math.round(x)` | `-> int` | Round to nearest |
| `Math.floor(x)` | `-> int` | Round down |
| `Math.ceil(x)` | `-> int` | Round up |
| `Math.round_to(x, places)` | `-> float` | Round to N decimal places |
| `Math.pi()` | `-> float` | π (3.14159...) |
| `Math.e()` | `-> float` | e (2.71828...) |

## String.to_float

```wyn
var f = "3.14".to_float()  // 3.14
```
