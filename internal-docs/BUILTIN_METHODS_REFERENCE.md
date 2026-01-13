# Complete Built-in Methods Reference

## All Core Types Have Built-in Methods! ✅

---

## INT Methods

### `.abs()` - Absolute Value
```wyn
let n = -42;
let positive = n.abs();  // 42
```

### `.is_even()` - Check if Even
```wyn
let n = 10;
let check = n.is_even();  // 1 (true)
```

### `.is_odd()` - Check if Odd
```wyn
let n = 7;
let check = n.is_odd();  // 1 (true)
```

---

## FLOAT Methods

### `.abs()` - Absolute Value
```wyn
let f = -3.14;
let positive = f.abs();  // 3.14
```

### `.floor()` - Round Down
```wyn
let f = 3.7;
let down = f.floor();  // 3.0
```

### `.ceil()` - Round Up
```wyn
let f = 3.2;
let up = f.ceil();  // 4.0
```

### `.round()` - Round to Nearest
```wyn
let f = 3.5;
let nearest = f.round();  // 4.0
```

---

## BOOL Methods

### `.to_int()` - Convert to Integer
```wyn
let b = true;
let n = b.to_int();  // 1

let f = false;
let z = f.to_int();  // 0
```

---

## ARRAY Methods

### `.sum()` - Sum All Elements
```wyn
let arr = [1, 2, 3, 4, 5];
let total = arr.sum();  // 15
```

### `.max()` - Find Maximum
```wyn
let arr = [5, 2, 8, 1, 9];
let biggest = arr.max();  // 9
```

### `.min()` - Find Minimum
```wyn
let arr = [5, 2, 8, 1, 9];
let smallest = arr.min();  // 1
```

### `.first()` - Get First Element
```wyn
let arr = [10, 20, 30];
let first = arr.first();  // 10
```

### `.last()` - Get Last Element
```wyn
let arr = [10, 20, 30];
let last = arr.last();  // 30
```

### `.push(val)` - Add Element
```wyn
let arr = [1, 2, 3];
arr.push(4);  // arr is now [1, 2, 3, 4]
```

### `.pop()` - Remove Last Element
```wyn
let arr = [1, 2, 3];
let last = arr.pop();  // 3, arr is now [1, 2]
```

---

## STRING Methods

### `.length()` - Get Length
```wyn
let s = "Hello";
let len = s.length();  // 5
```

### `.upper()` - Convert to Uppercase
```wyn
let s = "hello";
let upper = s.upper();  // "HELLO"
```

### `.lower()` - Convert to Lowercase
```wyn
let s = "HELLO";
let lower = s.lower();  // "hello"
```

### `.contains(substr)` - Check if Contains
```wyn
let s = "Hello World";
let has = s.contains("World");  // 1 (true)
```

### `.substring(start, end)` - Extract Substring
```wyn
let s = "Hello";
let sub = s.substring(1, 4);  // "ell"
```

### `.concat(other)` - Concatenate Strings
```wyn
let s1 = "Hello";
let s2 = " World";
let combined = s1.concat(s2);  // "Hello World"
```

---

## Complete Example

```wyn
fn main() -> int {
    // INT
    let n = -10;
    let abs_n = n.abs();           // 10
    let even = abs_n.is_even();    // 1
    
    // FLOAT
    let f = 3.7;
    let floor_f = f.floor();       // 3.0
    let ceil_f = f.ceil();         // 4.0
    
    // BOOL
    let b = true;
    let b_int = b.to_int();        // 1
    
    // ARRAY
    let arr = [1, 2, 3, 4, 5];
    let sum = arr.sum();           // 15
    let max = arr.max();           // 5
    
    // STRING
    let s = "Wyn";
    let len = s.length();          // 3
    
    return abs_n + even + floor_f + ceil_f + b_int + sum + max + len;
    // 10 + 1 + 3 + 4 + 1 + 15 + 5 + 3 = 42
}
```

---

## Works With Custom Extensions!

```wyn
struct Point { x: int, y: int }

impl Point {
    fn manhattan(self) -> int {
        // Using built-in abs() on ints!
        return self.x.abs() + self.y.abs();
    }
}

fn main() -> int {
    let p = Point { x: -3, y: 4 };
    let arr = [1, 2, 3];
    
    return p.manhattan() + arr.sum();  // 7 + 6 = 13
}
```

---

## Summary

### Types with Built-in Methods: 5
- ✅ **int** - abs, is_even, is_odd
- ✅ **float** - abs, floor, ceil, round
- ✅ **bool** - to_int
- ✅ **array** - sum, max, min, first, last, push, pop
- ✅ **string** - length, upper, lower, contains, substring, concat

### Types Without Methods Yet:
- ⚠️ **Option** - Needs is_some, is_none, unwrap, unwrap_or
- ⚠️ **Result** - Needs is_ok, is_err, unwrap, unwrap_or
- ⚠️ **Map** - Has some methods (get, set, has, size, clear, keys)
- ⚠️ **Enum** - Custom per enum
- ⚠️ **Struct** - Custom via impl blocks

---

## No Imports Required!

All these methods work **out of the box** without any imports or manual definitions:

```wyn
// Just works!
fn main() -> int {
    let n = -5;
    return n.abs();  // 5
}
```

---

## Test Coverage

```bash
./tests/test_builtin_methods.wyn.out      # Exit: 100 ✅
./tests/test_all_builtin_methods.wyn.out  # Exit: 100 ✅
```

All built-in methods tested and working! 🎉
