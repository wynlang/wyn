# Known Issues: Test Segfaults with Redirected Output

## Problem

76 out of 166 tests segfault when stdout/stderr are redirected to `/dev/null`, but run successfully when output goes to terminal.

## Reproduction

```bash
# Works fine
./tests/unit/test_collection_methods.out

# Segfaults
./tests/unit/test_collection_methods.out > /dev/null 2>&1
```

## Root Cause Analysis

### Hypothesis 1: Runtime Initialization
The LLVM-generated code or runtime may be initializing stdout/stderr file descriptors during startup. When these are redirected to `/dev/null`, the initialization fails or corrupts memory.

### Hypothesis 2: Cleanup Code
The runtime cleanup code may be trying to flush or close stdout/stderr, causing issues when they're already closed/redirected.

### Hypothesis 3: Global State
Tests may be using global state that depends on stdout/stderr being valid file descriptors.

## Evidence

1. **No output dependency**: Tests that don't print anything still segfault
2. **Consistent pattern**: All 76 tests fail the same way
3. **Timing**: Segfault happens immediately, not during test execution
4. **Exit code**: 139 (128 + 11 = SIGSEGV)

## Affected Tests

- test_collection_methods.wyn
- test_array_various.wyn
- test_hashmap_indexing.wyn
- test_hashset.wyn
- test_string_trim_split.wyn
- test_string_final.wyn
- test_type_aware_dispatch.wyn
- And 69 others (76 total)

## Workaround

The parallel test runner automatically detects and excludes these tests:

```bash
# This works - only runs 90 working tests
make test
```

## Investigation Steps

1. **Check runtime initialization**:
   ```bash
   # Add debug output to src/wyn_wrapper.c or runtime files
   # Look for stdout/stderr usage in initialization
   ```

2. **Test with strace**:
   ```bash
   strace -o trace.log ./tests/unit/test_collection_methods.out > /dev/null 2>&1
   # Check trace.log for failed syscalls
   ```

3. **Check LLVM IR**:
   ```bash
   ./wyn tests/unit/test_collection_methods.wyn --emit-llvm
   # Look for stdout/stderr references
   ```

4. **Minimal reproduction**:
   ```wyn
   fn main() -> int {
       return 0;
   }
   ```
   If this segfaults with redirect, it's definitely runtime initialization.

## Priority

**P1 - High**: This blocks 46% of tests from running in parallel mode, significantly limiting regression testing capabilities.

## Next Steps

1. Add debug logging to runtime initialization
2. Use strace to identify exact failure point
3. Check if issue exists in C codegen (non-LLVM) mode
4. Review stdout/stderr usage in all runtime files
5. Consider lazy initialization of I/O streams

## Temporary Solution

Current workaround is acceptable for development:
- 90 tests run in 2 seconds
- All tests compile successfully
- Tests can be run individually without redirect

## Long-term Fix

Need to ensure runtime is robust when:
- stdout/stderr are redirected
- File descriptors are closed
- Running in non-interactive environments
- Multiple tests run in parallel with fork()
