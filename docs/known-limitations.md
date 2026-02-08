# Wyn v1.4.0 - Known Limitations & Workarounds

## Working Features (100% Functional)

### ✅ String Operations
- `.len()`, `.upper()`, `.lower()`, `.trim()`, `.contains()`, `.starts_with()`, `.ends_with()`
- `.replace()`, `.slice()`, `.index_of()`, `.capitalize()`, `.reverse()`, `.title()`
- `split_get(text, delim, index)`, `split_count(text, delim)`

### ✅ Array Operations
- `.len()`, `.push()`, `.pop()`, `.contains()`, `.is_empty()`, `.index_of()`
- `.sort()`, `.reverse()`, `.get()`, `.first()`, `.last()`, `.clear()`
- Array indexing: `arr[0]`, `arr[1]`, etc.

### ✅ HashMap Operations
- Indexing: `map["key"]` (get), `map["key"] = value` (set)
- Methods: `.has(key)`, `.get(key)`, `.remove(key)`, `.len()`

### ✅ HashSet Operations
- Methods: `.add(item)`, `.contains(item)`, `.remove(item)`, `.len()`

### ✅ Type Conversions
- `str_parse_int(str)`, `int_to_str(num)`, `str_to_int(str)`

### ✅ Integer Methods
- `.abs()`, `.max(other)`, `.min(other)`

## Known Limitations

### ~~1. Array `.first()` and `.last()` Return Optional~~ ✅ FIXED
**Status:** RESOLVED - These methods now work correctly and return values directly.

```wyn
var nums = [1, 2, 3];
var first = nums.first();  // Works! Returns 1
var last = nums.last();    // Works! Returns 3
```

### ~~2. No Array `.clear()` Method~~ ✅ FIXED
**Status:** RESOLVED - Array `.clear()` method now works correctly.

```wyn
var items = [1, 2, 3];
items.clear();  // Works! Array is now empty
```

### 3. String Indexing Not Supported
**Issue:** Can't access individual characters with `str[i]`.

**Workaround:** Use `.slice()`:
```wyn
var text = "hello";
var first_char = text.slice(0, 1);  // Get first character
```

### 4. Parse Errors Return 0
**Issue:** `str_parse_int("invalid")` returns 0, same as parsing "0".

**Workaround:** Validate input before parsing or check if result is expected:
```wyn
var input = "123";
if (input.len() > 0) {
    var num = str_parse_int(input);
    // Use num
}
```

### 5. HashSet Type Warnings
**Issue:** 3 cosmetic compiler warnings in generated code.

**Impact:** None - code works correctly. Warnings are in generated C code, not user code.

### 6. Reserved Keywords
**Issue:** `map` and `set` are reserved keywords.

**Workaround:** Use different variable names:
```wyn
var scores = {"a": 1};  // Instead of: var map = ...
var tags = {:"x"};      // Instead of: var set = ...
```

## Best Practices

### String Processing
```wyn
// Extract substring
var email = "user@example.com";
var at_pos = email.index_of("@");
var username = email.slice(0, at_pos);

// Parse CSV
var csv = "a,b,c";
var first = split_get(csv, ",", 0);
var count = split_count(csv, ",");
```

### Array Operations
```wyn
// Build list
var items = [];
items.push(1);
items.push(2);

// Access elements
var first = items[0];
var last = items[items.len() - 1];

// Check contents
if (items.contains(2)) {
    print("Found 2");
}
```

### HashMap Usage
```wyn
// Create and populate
var cache = {"key1": 1};
cache["key2"] = 2;

// Safe access
if (cache.has("key1")) {
    var value = cache["key1"];
}

// Remove entries
cache.remove("key1");
```

### HashSet Usage
```wyn
// Track unique items
var seen = {:"item1"};
seen.add("item2");

// Check membership
if (seen.contains("item1")) {
    print("Already seen");
}
```

## Summary

Wyn v1.4.0 provides a solid, usable standard library with minor limitations that have simple workarounds. All core functionality works correctly and reliably.

**Recommendation:** Use the workarounds above for the known limitations. These are minor issues that don't prevent building real applications.
