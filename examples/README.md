# Wyn Language Examples

This directory contains **9 verified working examples** demonstrating various features of the Wyn programming language.

**Note:** 113 older/experimental examples have been moved to `archive/` directory.

## Running Examples

```bash
cd wyn
./wyn ../examples/<example>.wyn
../examples/<example>.wyn.out
echo $?
```

## Examples

### calculator.wyn
Demonstrates:
- Enums with qualified names (Operation::ADD)
- Pattern matching
- Functions with multiple parameters
- Control flow

**Expected output:** Exit code 41

### fibonacci.wyn
Demonstrates:
- Recursive functions
- Conditional logic
- Function calls

**Expected output:** Exit code 18 (fib(5) + fib(7) = 5 + 13)

### structs.wyn
Demonstrates:
- Struct definitions
- Struct initialization
- Functions operating on structs
- Field access

**Expected output:** Exit code 80 (area 50 + perimeter 30)

### arrays.wyn
Demonstrates:
- Array literals
- Array indexing
- Loops (while)
- Mutable variables
- Array processing functions

**Expected output:** Exit code 37 (sum 28 + max 9)

### generics.wyn
Demonstrates:
- Generic functions
- Type parameters
- Generic instantiation
- Comparison operators

**Expected output:** Exit code 67 (42 + 20 + 5)

## Language Features Demonstrated

- ✅ Variables (let, let mut)
- ✅ Functions
- ✅ Structs
- ✅ Enums
- ✅ Arrays
- ✅ Generics
- ✅ Pattern matching
- ✅ Control flow (if, while)
- ✅ Recursion
- ✅ Operators (+, -, *, /, >, ==)

## Creating Your Own Examples

1. Create a new .wyn file in this directory
2. Write your Wyn code
3. Compile with `./wyn ../examples/yourfile.wyn`
4. Run with `../examples/yourfile.wyn.out`
5. Check exit code with `echo $?`

## Documentation

For more information about the Wyn language, see:
- Standard library reference: `STDLIB_REFERENCE.md`
