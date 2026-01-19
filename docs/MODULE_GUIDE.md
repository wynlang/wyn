# Wyn Module System Guide

## Overview

Wyn v1.1.0 includes a complete module system for organizing code across multiple files and using external packages.

## Basic Usage

### Importing Modules

```wyn
import math      // Import a module
import point     // Import another module

fn main() -> int {
    return math.add(10, 20)
}
```

### Creating Modules

Create a file `utils.wyn`:

```wyn
// Only public items are accessible from other modules
pub fn helper(x: int) -> int {
    return x * 2
}

// Private function (not accessible)
fn internal_helper(x: int) -> int {
    return x + 1
}
```

Use it in `main.wyn`:

```wyn
import utils

fn main() -> int {
    return utils.helper(21)  // Returns 42
}
```

### Modules with Structs

Create `point.wyn`:

```wyn
pub struct Point {
    x: int,
    y: int
}

pub fn distance(p: Point) -> int {
    return p.x + p.y
}
```

Use it:

```wyn
import point

fn main() -> int {
    var p = point.Point { x: 3, y: 4 }
    return point.distance(p)  // Returns 7
}
```

## Module Resolution

When you write `import mymodule`, Wyn searches for `mymodule.wyn` in this order:

1. **Current directory** - `./mymodule.wyn`
2. **Project modules** - `./modules/mymodule.wyn`
3. **Project packages** - `./wyn_modules/mymodule.wyn`
4. **User packages** - `~/.wyn/packages/mymodule/mymodule.wyn`
5. **User modules** - `~/.wyn/modules/mymodule.wyn`
6. **System modules** - `/usr/local/lib/wyn/modules/mymodule.wyn`
7. **Standard library** - `./stdlib/mymodule.wyn`

The first match wins, so local files take priority over installed packages.

## Standard Library

Wyn includes one built-in module:

### math

```wyn
import math

fn main() -> int {
    var a = math.add(10, 20)        // 30
    var b = math.multiply(5, 6)     // 30
    var c = math.abs(-15)           // 15
    var d = math.max(10, 20)        // 20
    var e = math.min(10, 20)        // 10
    var f = math.pow(2, 8)          // 256
    var g = math.sqrt(16)           // 4
    return g
}
```

**Note:** Additional standard library modules (io, string, array) are planned for future releases.

## Project Structure

### Simple Project

```
my-project/
├── main.wyn
└── utils.wyn
```

### Organized Project

```
my-project/
├── main.wyn
├── modules/
│   ├── database.wyn
│   ├── auth.wyn
│   └── utils.wyn
└── wyn_modules/
    └── external_lib.wyn
```

## Nested Imports

Modules can import other modules:

`math.wyn`:
```wyn
pub fn add(a: int, b: int) -> int {
    return a + b
}
```

`utils.wyn`:
```wyn
import math

pub fn calculate(a: int, b: int) -> int {
    return math.add(a, b) * 2
}
```

`main.wyn`:
```wyn
import utils

fn main() -> int {
    return utils.calculate(5, 10)  // Returns 30
}
```

## Installing Packages

### Manual Installation

1. Create directory: `~/.wyn/packages/mypackage/`
2. Add module file: `~/.wyn/packages/mypackage/mypackage.wyn`
3. Import: `import mypackage`

### User Modules

For personal reusable modules:

1. Create: `~/.wyn/modules/myutils.wyn`
2. Import from any project: `import myutils`

## Public vs Private

Only items marked with `pub` are accessible from other modules:

```wyn
// Accessible from other modules
pub fn public_function() -> int { return 1 }
pub struct PublicStruct { x: int }

// Only accessible within this module
fn private_function() -> int { return 2 }
struct PrivateStruct { y: int }
```

## Best Practices

1. **Use `./modules/` for project code** - Keep related modules together
2. **Use `~/.wyn/modules/` for personal utilities** - Reusable across projects
3. **Mark only necessary items as `pub`** - Keep implementation details private
4. **One module per file** - Clear organization
5. **Descriptive names** - `database.wyn`, not `db.wyn`

## Examples

### Database Module

`modules/database.wyn`:
```wyn
pub struct Connection {
    host: string,
    port: int
}

pub fn connect(host: string, port: int) -> Connection {
    return Connection { host: host, port: port }
}

pub fn query(conn: Connection, sql: string) -> int {
    // Execute query
    return 0
}
```

### Using Multiple Modules

`main.wyn`:
```wyn
import database
import auth
import utils

fn main() -> int {
    var conn = database.connect("localhost", 5432)
    var user = auth.login("admin", "password")
    var result = utils.process(conn, user)
    return result
}
```

## Troubleshooting

### Module Not Found

If you get "Module 'X' not found":

1. Check the filename matches: `import foo` needs `foo.wyn`
2. Check the search paths (see Module Resolution above)
3. Verify the file exists: `ls -la modules/foo.wyn`

### Symbol Not Found

If you get "Undefined function 'X'":

1. Check the function is marked `pub`
2. Use the module prefix: `math.add()` not `add()`
3. Verify the module was imported

### Circular Imports

Avoid circular dependencies:
```
❌ a.wyn imports b.wyn
   b.wyn imports a.wyn
```

Instead, extract shared code to a third module:
```
✓ a.wyn imports common.wyn
  b.wyn imports common.wyn
```

## Future Features (v1.2+)

- Module aliases: `import math as m`
- Selective imports: `import math { add, multiply }`
- Package manager: `wyn pkg install github.com/user/repo`
- Package manifest: `wyn.toml`
- Version management
- Central package registry

## Summary

- Use `import module` to import modules
- Mark exports with `pub`
- Organize code in `./modules/`
- Share code via `~/.wyn/modules/`
- Built-in modules: `math`, `io`, `string`, `array`
