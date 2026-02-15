# Creating Python Libraries with Wyn

Wyn can compile your code into shared libraries that Python can call directly. This gives you C-level performance with Python-level convenience.

## Quick Start

1. Write a Wyn library (no `main` needed, but won't hurt):

```wyn
// mathlib.wyn
fn add(a: int, b: int) -> int {
    return a + b
}

fn greet(name: string) -> string {
    return "Hello, " + name + "!"
}

fn factorial(n: int) -> int {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}
```

2. Build with `--python`:

```bash
wyn run mathlib.wyn --python
```

This produces:
- `libmathlib.dylib` (or `.so` on Linux, `.dll` on Windows)
- `mathlib.py` — auto-generated Python wrapper

3. Use from Python:

```python
from mathlib import add, greet, factorial

print(add(2, 3))           # 5
print(greet("World"))       # Hello, World!
print(factorial(20))        # 2432902008176640000
```

## Type Mapping

| Wyn Type | Python Type | Notes |
|----------|-------------|-------|
| `int`    | `int`       | 64-bit integer |
| `float`  | `float`     | 64-bit double |
| `string` | `str`       | Auto encode/decode UTF-8 |
| `bool`   | `bool`      | True/False |

## Flags

| Flag | Output |
|------|--------|
| `--shared` | Shared library only (`.so`/`.dylib`/`.dll`) |
| `--python` | Shared library + Python wrapper |

## Tips

- Functions named `main` are excluded from the wrapper
- Struct methods (with `self`) are excluded
- C keyword names (like `float`, `double`) are automatically prefixed with `_`
- String arguments are automatically encoded to bytes when passed from Python
- String return values are automatically decoded to Python `str`
- The generated `.py` file includes Wyn type signatures as comments

## Example: String Processing Library

```wyn
// textlib.wyn
fn reverse(s: string) -> string {
    return s.reverse()
}

fn word_count(s: string) -> int {
    return s.split(" ").len()
}

fn shout(s: string) -> string {
    return s.upper() + "!"
}
```

```bash
wyn run textlib.wyn --python
```

```python
from textlib import reverse, word_count, shout

print(reverse("hello"))      # olleh
print(word_count("a b c"))   # 3
print(shout("hello"))        # HELLO!
```
