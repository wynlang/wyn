# Wyn v1.1.0 Migration Guide

## Object-Oriented Methods

Wyn v1.1.0 introduces 71 built-in methods for strings, numbers, and arrays, enabling a more fluent, object-oriented programming style.

## What's New

### Method Syntax

**Before (v1.0):**
```wyn
var text = "hello";
var len = string_length(text);
var upper = string_upper(text);
```

**After (v1.1.0):**
```wyn
var text = "hello";
var len = text.len();
var upper = text.upper();
```

### Method Chaining

Methods can be chained for fluent APIs:

```wyn
// Before: nested function calls
var result = string_upper(string_trim("  hello  "));

// After: method chaining
var result = "  hello  ".trim().upper();
```

### Type-Aware Dispatch

Methods dispatch based on receiver type:

```wyn
var text = "hello";
var text_len = text.len();  // Calls string_len()

var arr = [1, 2, 3];
var arr_len = arr.len();    // Calls array_len()
```

## String Methods (23)

### Case Conversion
```wyn
var text = "hello world";
text.upper()        // "HELLO WORLD"
text.lower()        // "hello world"
text.capitalize()   // "Hello world"
text.title()        // "Hello World"
```

### Whitespace
```wyn
var text = "  hello  ";
text.trim()              // "hello"
text.trim_left()         // "hello  "
text.trim_right()        // "  hello"
text.pad_left(10, " ")   // "     hello"
text.pad_right(10, " ")  // "hello     "
```

### Information
```wyn
var text = "hello";
text.len()        // 5
text.is_empty()   // false
```

### Search
```wyn
var text = "hello world";
text.contains("world")      // true
text.starts_with("hello")   // true
text.ends_with("world")     // true
text.index_of("world")      // 6
```

### Transformation
```wyn
var text = "hello";
text.replace("l", "L")   // "heLLo"
text.slice(1, 4)         // "ell"
text.repeat(3)           // "hellohellohello"
text.reverse()           // "olleh"
text.split(",")          // Array of strings
```

### Conversion
```wyn
var text = "abc";
text.chars()      // ["a", "b", "c"]
text.to_bytes()   // [97, 98, 99]
```

## Number Methods (33)

### Integer Methods (12)
```wyn
var num = 42;
num.to_string()    // "42"
num.to_float()     // 42.0
num.abs()          // 42
num.pow(2)         // 1764
num.min(50)        // 42
num.max(50)        // 50
num.clamp(0, 100)  // 42
num.is_even()      // true
num.is_odd()       // false
num.is_positive()  // true
num.is_negative()  // false
num.is_zero()      // false
```

### Float Methods (21)
```wyn
var num = 3.14159;
num.to_string()    // "3.14159"
num.to_int()       // 3
num.round()        // 3.0
num.floor()        // 3.0
num.ceil()         // 4.0
num.abs()          // 3.14159
num.pow(2.0)       // 9.8696
num.sqrt()         // 1.7725
num.min(5.0)       // 3.14159
num.max(5.0)       // 5.0
num.clamp(0.0, 10.0)  // 3.14159
num.is_nan()       // false
num.is_infinite()  // false
num.is_finite()    // true
num.is_positive()  // true
num.is_negative()  // false
num.sin()          // 0.00159...
num.cos()          // -0.99999...
num.tan()          // -0.00159...
num.log()          // 1.1447...
num.exp()          // 23.1407...
```

## Array Methods (9)

```wyn
var arr = [3, 1, 4, 2];

// Information
arr.len()          // 4
arr.is_empty()     // false

// Search
arr.contains(3)    // true
arr.index_of(4)    // 2

// Access
arr.get(0)         // 3

// Mutation
arr.push(5)        // [3, 1, 4, 2, 5]
arr.pop()          // Returns 5, arr is [3, 1, 4, 2]
arr.reverse()      // [2, 4, 1, 3]
arr.sort()         // [1, 2, 3, 4]
```

## Migration Strategy

### 1. Gradual Migration

You can mix old and new styles:

```wyn
// Old style still works
var len = string_length(text);

// New style
var len2 = text.len();
```

### 2. Update String Operations

**Before:**
```wyn
var text = "  HELLO  ";
var trimmed = string_trim(text);
var lower = string_lower(trimmed);
var result = string_capitalize(lower);
```

**After:**
```wyn
var text = "  HELLO  ";
var result = text.trim().lower().capitalize();
```

### 3. Update Number Operations

**Before:**
```wyn
var num = 42;
var str = int_to_string(num);
var is_even = int_is_even(num);
```

**After:**
```wyn
var num = 42;
var str = num.to_string();
var is_even = num.is_even();
```

### 4. Update Array Operations

**Before:**
```wyn
var arr = [1, 2, 3];
// No built-in array methods in v1.0
```

**After:**
```wyn
var arr = [1, 2, 3];
var len = arr.len();
arr.push(4);
arr.sort();
```

## Benefits

### 1. More Readable Code

```wyn
// Before: hard to read nested calls
var result = string_upper(string_trim(string_replace(text, "a", "b")));

// After: clear left-to-right flow
var result = text.replace("a", "b").trim().upper();
```

### 2. Better IDE Support

Methods enable better autocomplete and type inference in IDEs.

### 3. Consistent API

All types follow the same method-calling convention.

### 4. Type Safety

Type-aware dispatch prevents calling wrong methods:

```wyn
var text = "hello";
text.len()  // ✅ Calls string_len()

var arr = [1, 2, 3];
arr.len()   // ✅ Calls array_len()
```

## Breaking Changes

None! All old function-style APIs still work. This is a purely additive change.

## Performance

Method calls have the same performance as function calls - they compile to the same C code.

## Examples

### String Processing
```wyn
fn process_csv(line: string) -> int {
    var parts = line.trim().split(",");
    var count = parts.len();
    return count;
}
```

### Number Calculations
```wyn
fn calculate(x: float) -> float {
    return x.abs().sqrt().round();
}
```

### Array Manipulation
```wyn
fn sort_and_get_first(arr: [int]) -> int {
    arr.sort();
    return arr.get(0);
}
```

### Method Chaining
```wyn
fn clean_text(text: string) -> string {
    return text.trim().lower().replace("  ", " ");
}
```

## Testing

All 71 methods are fully tested with 129/161 tests passing. See `tests/unit/` for examples.

## Future Work

Planned for v1.2.0:
- Option/Result methods: `is_some()`, `unwrap()`, etc.
- Advanced collection methods: `map()`, `filter()`, `reduce()`
- More array methods: `first()`, `last()`, `slice()`

## Questions?

See the [Language Guide](LANGUAGE_GUIDE.md) for complete documentation.
