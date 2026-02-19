# Wyn v1.7.0 — Known Limitations

## Design Decisions

- **Variable shadowing** — `var x = 1; var x = 2` is a compile error. Use different names.
- **HashMap.keys()/values()** — return comma-separated strings. Use `.split(",")` for an array.
- **`wyn check`** — best-effort type checker. May not catch all type errors.

## Platform Notes

- **Windows regex** — POSIX regex stubs (returns false). Use `.contains()`, `.starts_with()`, etc.
- **Audio** — requires SDL2_mixer. Stubs when absent.
- **Mobile** — `system()`/`popen()` return stubs on iOS/Android.
- **TCC backend** — bundled for macOS ARM64. Other platforms use system `gcc`/`clang`.

## File Extensions

Both `.wyn` and `.🐉` are supported everywhere — CLI, imports, editor extensions.

## Type Conversions (method syntax)

```wyn
var n = "42".to_int()       // string → int
var s = 42.to_string()      // int → string
var f = "3.14".to_float()   // string → float
```

## String Methods

```wyn
var s = "hello world"
s.len()                  // 11
s.upper()                // "HELLO WORLD"
s.lower()                // "hello world"
s.contains("world")      // true
s.starts_with("hello")   // true
s.ends_with("world")     // true
s.replace("world", "wyn") // "hello wyn"
s.split(" ")             // ["hello", "world"]
s.trim()                 // removes whitespace
s.reverse()              // "dlrow olleh"
s.index_of("world")      // 6
s.substring(0, 5)        // "hello"
s.repeat(2)              // "hello worldhello world"
```
