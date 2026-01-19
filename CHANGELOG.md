# Changelog

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
