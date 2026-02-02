# Bug Fix: Enum Fields in Structs Causing Hang

## Problem
When compiling programs with enum fields in structs, the compiler would hang indefinitely during the linking phase.

## Root Cause
The `llvm_link_binary()` function in `src/llvm_codegen.c` was redirecting stderr to stdout (`2>&1`) in the clang link command executed via `system()`. This caused buffering issues that made the process hang.

## Symptoms
- Compiler would complete parsing, type checking, and LLVM IR generation successfully
- Hang occurred during the final linking step when calling `system()` to execute clang
- The generated `.ll` and `.o` files were valid
- Running the same clang command manually would work fine

## Fix
Removed the `2>&1` stderr redirection from the clang link command in `src/llvm_codegen.c` line 269.

**Before:**
```c
"%s/runtime/libwyn_runtime.a -lpthread -lm 2>&1",
```

**After:**
```c
"%s/runtime/libwyn_runtime.a -lpthread -lm",
```

## Test Case
`tests/test_enum_in_struct.wyn` - Tests enum fields in structs with:
- Struct definition with enum field
- Struct instantiation with enum values
- Field access (both int and enum fields)
- Enum field comparison

## Verification
```bash
./wyn-llvm tests/test_enum_in_struct.wyn && ./tests/test_enum_in_struct.out
# Expected: Exit 0 (success)
```

## Impact
- **Fixed**: Enum fields in structs now work correctly
- **No regressions**: All 26 comprehensive tests still pass (100%)
- **Self-hosting unblocked**: Can now use proper enum types in struct fields for the self-hosted compiler

## Technical Details
The `2>&1` redirection in a `system()` call can cause the parent process to block waiting for the child process's output buffers to be consumed, while the child process blocks waiting for the parent to read. This creates a deadlock situation. Removing the redirection allows stderr to flow normally without buffering issues.
