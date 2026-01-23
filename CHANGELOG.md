# Changelog

## [1.4.0] - 2026-01-23

### Added - Developer Tools

**Project Configuration:**
- `wyn.toml` - Project configuration file
- `[project]` section: name, version, entry, author, description
- `[dependencies]` section: package dependencies (parsing ready)
- `wyn init` creates wyn.toml automatically

**Build Tools:**
- `wyn watch <file>` - Auto-rebuild on file changes
- Simple polling-based file watching
- Cross-platform support (Unix/Windows)

**Package Manager (Basic):**
- `wyn install` - Install packages from wyn.toml
- `wyn pkg list` - List installed packages
- Package cache in ~/.wyn/packages/
- Foundation for full package ecosystem

**LSP Server (Stub):**
- `wyn lsp` - Start Language Server Protocol server
- Foundation for IDE integration
- Planned capabilities: diagnostics, hover, completion, go-to-definition

**List Comprehensions (Functional Style):**
- Use `.map()`, `.filter()`, `.reduce()` for list transformations
- Method chaining for complex operations
- More explicit than Python-style syntax
- Example: `numbers.filter(is_even).map(twice)`

### Added - Sample Applications

**Disk Usage Analyzer:**
- ncdu clone in sample-apps/utilities/disk-usage/
- Recursive directory scanning
- File and directory size calculation
- Demonstrates File:: module usage

**Functional Programming Example:**
- examples/50_list_comprehensions.wyn
- Demonstrates .map, .filter, .reduce
- Shows method chaining

### Added - Function Types & Functional Programming

**Function Types:**
- Function type syntax: `fn(T) -> R`
- Functions as first-class values
- Function pointers as parameters
- Type-safe higher-order functions

**Functional Array Methods:**
- `.map(fn)` - Transform each element with function
- `.filter(fn)` - Keep elements matching predicate
- `.reduce(fn, initial)` - Fold array into single value

**Examples:**
```wyn
fn twice(x: int) -> int { return x * 2; }
var doubled = [1, 2, 3].map(twice);  // [2, 4, 6]

fn is_positive(x: int) -> int { return x > 0; }
var positive = [-1, 2, -3].filter(is_positive);  // [2]

fn add(acc: int, x: int) -> int { return acc + x; }
var sum = [1, 2, 3].reduce(add, 0);  // 6
```

### Added - Standard Library Expansion

**String Character Methods:**
- `.is_alpha()` - Check if all characters are alphabetic
- `.is_digit()` - Check if all characters are numeric
- `.is_alnum()` - Check if all characters are alphanumeric
- `.is_whitespace()` - Check if all whitespace
- `.char_at(index)` - Get character at index
- `.equals(other)` - String equality comparison
- `.count(substring)` - Count occurrences of substring
- `.is_numeric()` - Check if valid number (int or float)

**Array Utility Methods:**
- `.count(value)` - Count occurrences of value in array

**Integer Methods:**
- `.to_binary()` - Convert to binary string
- `.to_hex()` - Convert to hexadecimal string
- `.is_even()` / `.is_odd()` - Check parity (already existed, now documented)

**File Path Manipulation:**
- `File::dirname(path)` - Extract directory from path
- `File::extension(path)` - Extract file extension
- `File::path_join(a, b)` - Join paths safely (already existed, now documented)
- `File::basename(path)` - Extract filename (already existed, now documented)
- `File::get_cwd()` - Get current directory (already existed, now documented)

**File System Operations (already existed, now validated):**
- `File::list_dir(path)` - List directory contents
- `File::is_file(path)` - Check if path is file
- `File::is_dir(path)` - Check if path is directory
- `File::create_dir(path)` - Create directory
- `File::file_size(path)` - Get file size

**System Operations (already existed, now validated):**
- `System::args()` - Get command line arguments as array

**System Environment (NEW):**
- `System::set_env(key, value)` - Set environment variable
- `System::exec_code(command)` - Execute command and get exit code

**Time Operations (NEW):**
- `Time::format(timestamp)` - Format timestamp as human-readable string

**Error Handling (Documented):**
- Return code patterns for error handling
- -1 for numeric errors
- Empty string for string errors
- Non-zero exit codes for program errors

**Networking (NEW):**
- `Net::listen(port)` - Create TCP server
- `Net::connect(host, port)` - Connect to TCP server
- `Net::send(socket, data)` - Send data
- `Net::recv(socket)` - Receive data
- `Net::close(socket)` - Close socket

### Added - Module System (Major Feature)
- **Nested Modules** - Java/TypeScript-style syntax with `.` separator
  - Import: `import network.http`
  - Use short name: `http::get()`
  - Or full path: `network_http::get()`
  - Filesystem: `network/http.wyn`

- **Relative Imports** - Navigate module hierarchy easily
  - `root::` - Import from project root (absolute)
  - `self::` - Import from current directory (relative)
  - Example: `import root::config` (root), `import self::utils` (sibling)

- **Visibility Control** - Encapsulation with `pub` keyword
  - Functions are private by default
  - Use `pub fn` for public API
  - Cross-module access checking
  - Example: `pub fn api()` (public), `fn helper()` (private)

- **Module Name Collision Detection** - Prevents ambiguous imports
  - Detects when two modules have same short name
  - Clear error messages with suggestions
  - Example: `import network.http` + `import storage.http` → error with fix suggestions

### Changed (BREAKING)
- **Unified print() function** - Replaced type-specific print functions with polymorphic `print()`
  - Migration: Replace `print_int(x)`, `print_str(s)`, `print_float(f)` with `print(x)`, `print(s)`, `print(f)`
  - Reason: Consistency with "everything is an object" philosophy
  - Uses C11 `_Generic` for type dispatch

### Added
- **Pure Object-Oriented API** - All operations now use method syntax
  - String methods: `.concat()`, `.parse_int()`, `.parse_float()`
  - Number methods: `.to_string()` on int and float
  - No standalone functions needed - everything is a method or module call
  - Example: `"42".parse_int()` instead of `str_parse_int("42")`
  - Example: `42.to_string()` instead of `int_to_str(42)`
  - Example: `s1.concat(s2)` instead of `str_concat(s1, s2)`

- **Standard Library Modules** - Organized functionality with `::` syntax
  - `File::read()`, `File::write()`, `File::exists()`, `File::delete()`
  - `System::exec()`, `System::exit()`
  - `Math::pow()`, `Math::sqrt()`
  - All modules available without import
  - Clean namespace separation

- **HashMap Indexing Syntax** - Access HashMap values with bracket notation
  - Get: `var value = map["key"]`
  - Set: `map["key"] = value`
  - Cleaner than function-style API

- **HashMap Methods** - Object-oriented method syntax for HashMaps
  - `.has(key)` - Check if key exists
  - `.remove(key)` - Remove key-value pair
  - `.len()` - Get number of entries
  - `map["key"]` - Index syntax for get/set
  - Example: `if (scores.has("alice")) { var score = scores["alice"]; }`

- **HashSet Methods** - Object-oriented method syntax for HashSets
  - `.add(item)` - Add element to set
  - `.contains(item)` - Check if element exists
  - `.remove(item)` - Remove element
  - `.len()` - Get number of elements
  - Example: `tags.add("urgent")`

- **Enhanced String Functions** - New string utilities for real-world apps
  - `char_at(text, index)` - Get character at position
  - `is_numeric(text)` - Check if string is numeric
  - `str_count(text, substr)` - Count substring occurrences
  - `str_contains_substr(text, substr)` - Check if substring exists
  - `split_get(text, delim, index)` - Get specific split element
  - `split_count(text, delim)` - Count split elements

- **Enhanced Array Methods** - Statistical and manipulation functions
  - `.first()` - Get first element
  - `.last()` - Get last element
  - `.clear()` - Remove all elements
  - `.min()` - Get minimum value
  - `.max()` - Get maximum value
  - `.sum()` - Sum all elements
  - `.average()` - Calculate average
  - `.remove(value)` - Remove all occurrences
  - `.insert(index, value)` - Insert at position

- **Sample Applications** - Nine production-ready apps organized by category (sample-apps/)
  - **Utilities:** file-finder, disk-analyzer, process-monitor, build-monitor
  - **Data Processing:** log-analyzer, csv-processor, text-processor
  - **Dev Tools:** code-stats
  - **Tutorials:** calculator-modules (module system demo)

- **Comprehensive stdlib demo** - New example (12_comprehensive_stdlib.wyn) showcasing all major features
  - 23 examples total (was 22)
  - Demonstrates strings, arrays, HashMaps, HashSets, type conversions, integer methods, string splitting

- **String splitting functions** - New `split_get()` and `split_count()` for working with split strings
  - `split_get(text, delim, index)` - Get specific element from split result
  - `split_count(text, delim)` - Count elements in split result
  - Solves the string array indexing limitation

- **Character access function** - New `char_at(text, index)` for accessing individual characters
  - Returns single character as string
  - Safe bounds checking (returns "" if out of bounds)

- **Number validation function** - New `is_numeric(text)` for validating numeric strings
  - Returns 1 if string is a valid number, 0 otherwise
  - Handles negative numbers
  - Use before `str_parse_int()` to avoid ambiguity

- **Array `.first()` and `.last()` methods** - Now return int directly (not Optional)
  - Returns 0 if array is empty
  - Can be used directly with `print()` and assignments

- **Array `.clear()` method** - Clear all elements from array
  - Sets array length to 0
  - Memory efficient

### Improved
- **Documentation Consistency** - All docs updated to use OO method syntax
  - 52 instances of old print API updated
  - Examples demonstrate best practices
  - Consistent "everything is an object" approach

- **Standard Library** - Comprehensive stdlib already in place
  - String module: 15 functions (split, join, replace, trim, etc.)
  - Array module: 18 functions (map, filter, reduce, sort, etc.)
  - Time module: 13 functions (now, sleep, format, parse, etc.)
  - Crypto module: 9 functions (hash, base64, random, etc.)

- **Better error messages** - Helpful hints for unknown methods with suggestions

- **Updated examples** - All active examples use new v1.4.0 syntax
  - 06_hashmap.wyn - Showcases indexing and methods
  - 08_hashset.wyn - Fixed precedence issues, uses method syntax

### Changed
- **Math module cleanup** - Removed trivial functions (math.add, math.multiply)
  - Users should use operators: +, -, *, / for basic arithmetic
  - Kept useful functions: pow, sqrt, abs, floor, ceil, round, sin, cos, tan, log, exp, min, max, pi, e

### Technical
- Added `hashmap_len()` function for HashMap size queries
- Fixed HashMap indexing codegen to use correct functions
- Updated method dispatch in types.c for HashMap/HashSet
- All regression tests passing (23/23 examples compile and run)

---

## [1.3.2] - 2026-01-20

### Fixed
- **Windows MinGW Compatibility** - Added `-std=c11` flag to all GCC compilation commands
  - Fixes GitHub issue #1: hello world compilation on Windows 7 MinGW-winlibs
  - Ensures `_Generic` macro support across all platforms
  - Affects: normal compilation, `wyn run`, `wyn build`, and cross-compilation

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
