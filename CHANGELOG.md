# Changelog

## [1.3.0] - 2026-01-20

### Added
- **HashMap Multi-Type Support** - HashMap now supports int, float, string, and bool values
  - New functions: `hashmap_insert_int()`, `hashmap_insert_float()`, `hashmap_insert_string()`, `hashmap_insert_bool()`
  - New functions: `hashmap_get_int()`, `hashmap_get_float()`, `hashmap_get_string()`, `hashmap_get_bool()`
  - Tagged union implementation for type-safe value storage
  - Automatic type detection in HashMap literals

- **HashSet Initialization Syntax** - Create HashSets with initial values
  - Syntax: `{:"item1", "item2", "item3"}`
  - Empty HashSet: `{:}`
  - Supports string and int elements

- **Full Standard Library** - Comprehensive built-in functions across 7 modules
  - **String Module** (15 functions): split, join, replace, substring, index_of, last_index_of, repeat, reverse, and more
  - **Array Module** (18 functions): slice, concat, fill, sum, min, max, average, last_index_of, and more
  - **Time Module** (13 functions): now, now_millis, now_micros, sleep, format, parse, year, month, day, hour, minute, second
  - **Crypto Module** (9 functions): hash32, hash64, md5, sha256, base64_encode, base64_decode, random_bytes, random_hex, xor_cipher
  - **File I/O Module**: Existing file operations
  - **System Module**: Existing system operations
  - **Net Module**: Existing network operations

### Changed
- HashMap implementation now uses tagged union for multi-type support
- HashSet literal parsing enhanced to support initialization
- Improved quicksort algorithm for array sorting (replaced bubble sort)

### Technical
- Added `stdlib_crypto.c` with hashing and encoding functions
- Enhanced `stdlib_string.c` with 8 additional string functions
- Enhanced `stdlib_array.c` with 8 additional array functions
- Updated codegen to emit type-specific HashMap insert/get calls
- Updated Makefile to include new crypto module

---

## [1.2.0] - 2026-01-20

### Changed (BREAKING)
- **Removed `let` keyword** - Use `var` (mutable) or `const` (immutable) instead
  - Migration: Replace `let x = 10` with `var x = 10` or `const x = 10`
  - Reason: Simplify syntax, reduce confusion

### Added
- **HashMap literal syntax `{}`** - Create hashmaps with `var hmap = {}`
  - Empty hashmaps: `var hmap = {}`
  - With initialization: `var hmap = {"key1": 10, "key2": 20}`
  - Keys must be strings, values are integers

- **HashSet literal syntax `{:}`** - Create hashsets with `var hset = {:}`
  - Cleaner than `hashset_new()`
  - Unique syntax that doesn't conflict with tuples

- **HashSet Operations** - Registered set operation functions
  - `set_union()`, `set_intersection()`, `set_difference()`
  - `set_is_subset()`, `set_is_superset()`
  - `set_len()`, `set_is_empty()`, `set_clear()`

### Fixed
- **Critical Bug**: Methods calling other methods in impl blocks
  - Fixed `self` parameter type resolution in method bodies
  - Test: `tests/wyn/test_impl_method_calls.wyn`

- **HashMap Implementation**: Completed all functions
  - Added `hashmap_remove()` and `hashmap_has()`
  - Implemented `map_len()`, `map_is_empty()`, `map_merge()`
  - Test: `tests/wyn/test_hashmap_complete.wyn`

- **Critical Bug**: Binary expression type inference
  - Fixed incorrect type inference for nested arithmetic expressions
  - Variables with binary expressions now correctly infer as `int` instead of `const char*`
  - Test: `tests/wyn/test_nested_binary.wyn`

### Validated
- **Array helpers** already implemented and working:
  - `contains()`, `index_of()`, `reverse()`
  - Test: `tests/wyn/test_array_helpers.wyn`

- **String helpers** already implemented and working:
  - `starts_with()`, `ends_with()`, `trim()`, `replace()`
  - Test: `tests/wyn/test_string_helpers.wyn`

- All 21 examples compile and run
- All regression tests pass
- Comprehensive v1.2.0 test passes

---

## Previous Versions

See archive/ for older changelog entries.
