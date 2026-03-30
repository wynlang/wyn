# Wyn Language Examples

This directory contains **39 working examples** demonstrating various features of the Wyn programming language.

## Running Examples

```bash
cd wyn
./wyn examples/<example>.wyn
./examples/<example>.wyn.out
```

## Quick Start Examples

### 01_hello_world.wyn
The classic first program.
```wyn
fn main() -> int {
    print("Hello, World!");
    return 0;
}
```

### 02_functions.wyn
Demonstrates function definitions and calls.

### 03_generics.wyn
Shows generic function usage.

### 04_async_await.wyn
Demonstrates async/await syntax.

### 05_file_io.wyn
File reading and writing operations.

### 06_hashmap.wyn
HashMap creation and manipulation.

### 07_modules/
Module system demonstration with imports.

### 08_hashset.wyn
HashSet operations.

### 09_json.wyn
JSON parsing and generation (if implemented).

### 10_structs.wyn
Struct definitions and methods.

### 11_methods.wyn
Extension methods and impl blocks.

### 12_comprehensive_stdlib.wyn
Comprehensive standard library showcase.

### 13_best_practices.wyn
Recommended coding patterns.

### 14_string_interpolation.wyn
String interpolation examples.

### 15_option_types.wyn
Option type for safe handling of optional values.

### 32_file_operations.wyn
Advanced file system operations (copy, move, size, type checking).

### 33_cli_args.wyn
Command-line argument parsing (flags, options, positional arguments).

### 34_environment_vars.wyn
Environment variable access and modification (Env::get, Env::set, Env::all).

### 35_advanced_file_ops.wyn
Advanced file operations (modified_time, create_dir_all, remove_dir_all).

### 36_collections.wyn
Data structures: Queue (FIFO) and Stack (LIFO) collections.

### 37_process_execution.wyn
Process execution: run external commands, get output and exit codes.

### 38_time_api.wyn
Time API: timestamps, formatting, and sleep functionality.

### 39_string_comparison.wyn
String comparison: all comparison operators (==, !=, <, >, <=, >=) now work correctly.

## Real-World Examples

### 26_file_explorer.wyn
File system navigation and operations.

### 27_system_info.wyn
System information gathering.

### 28_build_tool.wyn
Build automation example.

### 29_http_server.wyn
HTTP server implementation.

### 30_functional_programming.wyn
Functional programming patterns (map, filter, reduce).

## Classic Examples

### hello.wyn
Simple hello world.

### fibonacci.wyn
Recursive Fibonacci calculation.

### calculator.wyn
Calculator with enums and pattern matching.

### structs.wyn
Struct usage demonstration.

### arrays.wyn
Array operations.

### binary_search.wyn
Binary search algorithm.

### quicksort.wyn
Quicksort implementation.

### game_of_life.wyn
Conway's Game of Life.

### state_machine.wyn
State machine pattern.

### hash_function.wyn
Hash function implementation.

### generics.wyn
Generic programming.

## Advanced Examples

See `advanced/` directory for more complex examples.

## Multifile Examples

See `multifile/` directory for examples with multiple source files.

## Language Features Demonstrated

- ✅ Variables (var, const)
- ✅ Functions with overloading
- ✅ Structs with methods
- ✅ Enums
- ✅ Arrays (dynamic)
- ✅ HashMap with literals
- ✅ HashSet with literals
- ✅ Generics
- ✅ Pattern matching
- ✅ Control flow (if, while, for)
- ✅ Async/await
- ✅ Module system
- ✅ String interpolation
- ✅ Method chaining
- ✅ Functional programming (map, filter, reduce)
- ✅ Enum to_string()
- ✅ Indexed iteration (for i, v in arr)
- ✅ Optional types (int?)
- ✅ String repeat operator ("str" * n)

## Documentation

For more information:
- Language guide: `../docs/language-guide.md`
- Standard library: `../docs/stdlib-reference.md`
- Best practices: `../docs/best-practices.md`


## Recent Additions (v1.4.0)

### 40_array_get_typed.wyn
Demonstrates typed array.get() with proper type inference for string and int arrays.

### 40_simple_get.wyn
Simple demonstration of array.get() returning correct types.


### 41_collections_typed.wyn
Queue and Stack with proper type inference.

### 42_collections_explicit.wyn
Collections with explicit type annotations.

### 43_function_return_arrays.wyn
Functions returning arrays with proper type support.


## Example 44: String starts_with and ends_with

```wyn
// TDD Test for starts_with and ends_with
fn main() -> int {
    var text = "Hello World";
    
    // Test starts_with
    var result1 = text.starts_with("Hello");
    print(result1);  // true
    
    // Test ends_with
    var result2 = text.ends_with("World");
    print(result2);  // true
    
    return 0;
}
```

**Features demonstrated:**
- String.starts_with() method
- String.ends_with() method
- Boolean return values


## Example 45: System::args() Type Inference

```wyn
// TDD Test for System::args() returning string array
fn main() -> int {
    var args = System::args();
    
    // Test that args is an array
    var count = args.len();
    print("Argument count:");
    print(count);
    
    // Test that .get() returns string (not int)
    if count > 0 {
        var first = args.get(0);
        // If this compiles and runs, first is a string
        var upper = first.upper();
        print("First arg (uppercase):");
        print(upper);
    }
    
    print("✓ System::args() returns string array");
    return 0;
}
```

**Features demonstrated:**
- System::args() returns [string]
- Array.get() preserves element type
- Type inference for array elements
- String methods on array elements


## Example 46: Array.map() - Functional Programming

```wyn
fn double_it(x: int) -> int {
    return x * 2;
}

fn main() -> int {
    var numbers = [1, 2, 3, 4, 5];
    var doubled = numbers.map(double_it);
    print(doubled.get(0));  // 2
    return 0;
}
```

## Example 47: Array.filter() - Functional Programming

```wyn
fn check_even(x: int) -> int {
    return x % 2 == 0;
}

fn main() -> int {
    var numbers = [1, 2, 3, 4, 5, 6];
    var evens = numbers.filter(check_even);
    print(evens.len());  // 3
    return 0;
}
```

## Example 48: Array.reduce() - Functional Programming

```wyn
fn add(acc: int, x: int) -> int {
    return acc + x;
}

fn main() -> int {
    var numbers = [1, 2, 3, 4, 5];
    var sum = numbers.reduce(add, 0);
    print(sum);  // 15
    return 0;
}
```

**Features demonstrated:**
- Higher-order functions
- Functional programming patterns
- Array transformations
- map, filter, reduce operations


## v1.11 Features

### 57_enum_to_string.wyn
Enum `.to_string()` method — returns the variant name as a string. Useful for logging and debugging.

### 58_indexed_iteration.wyn
Indexed `for i, v in arr` syntax — iterate with both index and value.

### 59_optional_types.wyn
Optional types with `int?` syntax — `OptionInt_Some()`, `OptionInt_None()`, and `OptionInt_unwrap()`.

### 60_string_repeat.wyn
String repeat operator — `"ha" * 3` produces `"hahaha"`.
