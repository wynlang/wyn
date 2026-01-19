# Wyn Module System Design

## Use Cases

### 1. Basic Function Import
```wyn
// math.wyn
pub fn add(a: int, b: int) -> int {
    return a + b
}

pub fn multiply(a: int, b: int) -> int {
    return a * b
}

// main.wyn
import math

fn main() -> int {
    return math.add(10, 20) + math.multiply(2, 3)
}
```
**Expected:** Compiles, returns 36

### 2. Module with Struct
```wyn
// point.wyn
pub struct Point {
    x: int,
    y: int
}

pub fn distance(p: Point) -> int {
    return p.x + p.y
}

// main.wyn
import point

fn main() -> int {
    var p = point.Point { x: 3, y: 4 }
    return point.distance(p)
}
```
**Expected:** Compiles, returns 7

### 3. Nested Imports
```wyn
// math.wyn
pub fn add(a: int, b: int) -> int {
    return a + b
}

// utils.wyn
import math

pub fn calculate(a: int, b: int) -> int {
    return math.add(a, b) * 2
}

// main.wyn
import utils

fn main() -> int {
    return utils.calculate(5, 10)
}
```
**Expected:** Compiles, returns 30

### 4. Multiple Imports
```wyn
// main.wyn
import math
import point

fn main() -> int {
    var p = point.Point { x: 3, y: 4 }
    return math.add(p.x, p.y)
}
```
**Expected:** Compiles, returns 7

### 5. Private vs Public
```wyn
// utils.wyn
fn private_helper(x: int) -> int {  // Not pub
    return x * 2
}

pub fn public_func(x: int) -> int {
    return private_helper(x) + 1
}

// main.wyn
import utils

fn main() -> int {
    return utils.public_func(5)  // OK
    // utils.private_helper(5)    // Should error
}
```
**Expected:** Only public functions accessible

### 6. Struct Methods in Modules
```wyn
// point.wyn
pub struct Point {
    x: int,
    y: int
}

impl Point {
    pub fn sum(self) -> int {
        return self.x + self.y
    }
}

// main.wyn
import point

fn main() -> int {
    var p = point.Point { x: 3, y: 4 }
    return p.sum()  // Method call on imported struct
}
```
**Expected:** Compiles, returns 7

## Design Requirements

### Compilation Flow

1. **Parse Phase:**
   - Parse main file
   - When `import X` is encountered:
     - Find X.wyn in module paths
     - Parse X.wyn (with saved parser state)
     - Store parsed AST in module registry
     - Register public symbols (functions, structs) in symbol table
   - Continue parsing main file with knowledge of imported symbols

2. **Type Check Phase:**
   - Type check main file
   - When encountering `module.Type` or `module.function`:
     - Look up in symbol table
     - Verify it's public
     - Type check as normal

3. **Code Generation Phase:**
   - Generate code for main file
   - For each imported module:
     - Generate struct definitions (no prefix)
     - Generate functions with module prefix: `module_function`
   - Generate main file code with prefixed calls

### Symbol Table Structure

```c
typedef struct {
    char* module_name;
    Program* ast;
    HashMap* public_symbols;  // name -> symbol info
} Module;

typedef struct {
    Module* modules[64];
    int module_count;
} ModuleRegistry;
```

### Module Resolution

Search paths (in order):
1. Current directory
2. `/tmp/wyn_modules/` (for testing)
3. `~/.wyn/modules/`
4. `/usr/local/lib/wyn/modules/`

### Name Mangling

- Functions: `module_function` → `math_add`
- Structs: No prefix → `Point` (globally unique)
- Methods: `Type_method` → `Point_sum`

### Key Challenges

1. **Parser State:** Need to save/restore parser state when parsing modules
2. **Type Resolution:** Need to resolve `module.Type` during parsing
3. **Circular Imports:** Detect and prevent
4. **Symbol Conflicts:** Two modules defining same struct name

## Implementation Strategy

### Phase 1: Module Registry (30 min)
- Create module registry data structure
- Implement module loading with parser state save/restore
- Store parsed modules

### Phase 2: Symbol Table (30 min)
- Register public symbols from modules
- Look up symbols during parsing
- Handle `module.Type` and `module.function` syntax

### Phase 3: Code Generation (30 min)
- Emit struct definitions from modules
- Emit functions with prefixes
- Handle method calls on imported structs

### Phase 4: Testing (30 min)
- Test all 6 use cases
- Fix edge cases
- Verify nested imports work

**Total Estimated Time:** 2 hours

## Simplified Approach (If Needed)

If full implementation is too complex, we can:
1. Only support single-level imports (no nested)
2. Require all structs to be globally unique
3. Parse all imports before main file (two-pass)

This reduces complexity by 50% while covering 80% of use cases.
