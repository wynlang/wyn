# Changelog

## 1.7.0 - 2026-01-07

Multi-file projects with maps, structs, and tuples - critical features for real-world systems programming.

### Added
- **Module System** - Complete import/export functionality
  - `export fn add() {}` and `export var PI = 3.14` syntax
  - `import math` statement with symbol resolution
  - **NEW**: `math.add(5, 3)` qualified access syntax
  - Multi-file project support with proper namespacing
- **Map Type** - Full key-value storage with methods and index syntax
  - `map[string, int]{"alice": 25, "bob": 30}` syntax
  - `.get(key)`, `.set(key, value)`, `.has(key)` methods
  - **NEW**: `map["key"]` index syntax for natural access
  - **NEW**: `map["key"] = value` assignment syntax
  - Proper type system integration with WynMap typedef
- **Struct Literals** - Planned for future release
  - Foundation implemented but disabled due to parser conflicts
  - Will be enabled in v1.7.1 with improved conflict resolution
- **Tuple Types** - Multiple value grouping
  - `(1, 2, 3)` syntax with comma detection
  - Anonymous struct code generation
  - Foundation for destructuring and field access
- **Enhanced Type System** - Rich data structure support
  - TYPE_MAP with MapType structure
  - Field access type checking for module.function syntax
  - Improved method dispatch for context-aware resolution

### Fixed
- **CRITICAL**: Map index assignment parsing bug where `map["key"] = value` generated incorrect code
- Parser precedence issue with TOKEN_EQ consumption order
- Map assignment now correctly generates `map_set(&map, "key", value)` calls

### Improved
- Method dispatch system prioritizes map methods over HTTP methods
- Symbol registration handles both qualified and unqualified names
- Conditional code generation prevents function conflicts
- All existing v1.6.0 features preserved and stable

### Technical Details
- **125+ stdlib functions** (preserved from v1.6.0)
- **75+ methods** across all types (preserved from v1.6.0)
- **3 new map methods**: get(), set(), has()
- **2 new keywords**: export, map
- **3 new AST nodes**: STMT_EXPORT, EXPR_MAP, EXPR_TUPLE
- **All 69 tests passing** with no regressions
