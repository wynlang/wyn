# Changelog

## [1.2.3] - 2026-01-20

### Changed (BREAKING)
- **Removed `let` keyword** - Use `var` (mutable) or `const` (immutable) instead
  - Migration: Replace `let x = 10` with `var x = 10` or `const x = 10`
  - Reason: Simplify syntax, reduce confusion

### Added
- **HashMap literal syntax `{}`** - Create hashmaps with `var map = {}`
  - Currently only empty hashmaps supported
  - Initialization syntax `{"key": value}` planned for v1.2.4

### Note
- HashSet literal syntax `()` was attempted but disabled due to conflict with tuples
- Use `hashset_new()` for now
- Alternative syntax to be determined in v1.2.4

## [1.2.2] - 2026-01-20

### Added
- **HashSet Operations**: Registered set operation functions
  - `set_union()` - Union of two sets
  - `set_intersection()` - Intersection of two sets
  - `set_difference()` - Difference of two sets
  - `set_is_subset()` - Check if subset
  - `set_is_superset()` - Check if superset
  - `set_len()` - Get set size
  - `set_is_empty()` - Check if empty
  - `set_clear()` - Clear all elements
  - Test: `tests/wyn/test_hashset_ops.wyn`

### Validated
- **Array helpers** already implemented and working:
  - `contains()`, `index_of()`, `reverse()`
  - Test: `tests/wyn/test_array_helpers.wyn`
- **String helpers** already implemented and working:
  - `starts_with()`, `ends_with()`, `trim()`, `replace()`
  - Test: `tests/wyn/test_string_helpers.wyn`
- All 21 examples still pass
- All regression tests pass
- Comprehensive v1.2.2 test passes

## [1.2.1] - 2026-01-20

### Fixed
- **Critical Bug**: Binary expression type inference
  - Fixed incorrect type inference for nested arithmetic expressions
  - Variables with binary expression initializers (e.g., `let x = a + b + c`) now correctly infer as `int` instead of `const char*`
  - Removed duplicate and incorrect string concatenation detection logic
  - Test: `tests/wyn/test_nested_binary.wyn`

### Validated
- All 21 examples still pass
- All regression tests pass
- Comprehensive v1.2.1 feature test passes

## [1.2.0] - 2026-01-19

### Fixed
- **Critical Bug**: Methods calling other methods in impl blocks now work correctly
  - Fixed `self` parameter type resolution in method bodies
  - Test: `tests/wyn/test_impl_method_calls.wyn`
- **HashMap Implementation**: Removed all stubs, completed implementation
  - Added `hashmap_remove()` - Remove key-value pairs
  - Added `hashmap_has()` - Check if key exists
  - Implemented `map_len()` - Count entries
  - Implemented `map_is_empty()` - Check if empty
  - Implemented `map_merge()` - Merge two hashmaps
  - Test: `tests/wyn/test_hashmap_complete.wyn`

### Added
- Regression test suite (`tests/regression.sh`)
- 4 new test files documenting behavior and fixes

### Validated
- 21/21 examples still pass
- All regression tests pass
- No stubs or fake code in C backend
- Fully backward compatible with v1.1.0

## [1.1.0] - 2026-01-19

### Added
- **Module System**: Full module system with 11 search paths
  - Public/private visibility with `pub` keyword
  - Nested imports and user module override
  - Built-in math module with 7 functions (sqrt, pow, sin, cos, tan, abs, floor)
- **Cross-platform Support**: Release binaries for Linux (x64, ARM64), macOS (x64, ARM64), and Windows (x64)

### Fixed
- Method call code generation for extension methods, string methods, and number methods
- Built-in math module accessibility (all 7 functions now work)
- Test pollution with proper cleanup of `/tmp/wyn_modules/`
- Linux build compatibility with POSIX defines
- Windows compatibility issues (TokenType conflicts, time_t conversions, dirent.h)
- Compilation paths with WYN_ROOT environment variable

### Changed
- Release workflow now runs tests before cross-compilation
- Test scripts use relative paths for better portability

### Validated
- 21/21 examples pass
- 11/11 module tests pass
- All advertised features verified working
