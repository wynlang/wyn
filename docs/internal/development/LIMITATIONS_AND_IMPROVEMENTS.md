# Wyn Compiler - Current Limitations & Improvement Opportunities

**Version:** 1.6.0+  
**Date:** 2026-01-30  
**Status:** Production Ready (198/198 tests passing)

## Current Limitations

### 1. Language Features

#### Missing Core Features
- **No string interpolation** - Must use str_concat()
  ```wyn
  // Current: str_concat(str_concat("Hello, ", name), "!")
  // Desired: "Hello, {name}!"
  ```

- **No format strings** - println() only takes one argument
  ```wyn
  // Current: println("Value:"); println(x);
  // Desired: println("Value: {}", x)
  ```

- **No range function** - Manual loop counters required
  ```wyn
  // Current: var i = 0; while (i < 10) { ... i = i + 1; }
  // Desired: for i in range(0, 10) { ... }
  ```

- **No array methods** - Missing map/filter/reduce
  ```wyn
  // Desired: nums.map(fn(x) -> x * 2)
  // Desired: nums.filter(fn(x) -> x > 5)
  ```

- **No string split** - Can't split strings into arrays
  ```wyn
  // Desired: "a,b,c".split(",") → ["a", "b", "c"]
  ```

#### Type System Limitations
- **No generics** - Can't write generic functions/structs
- **No traits/interfaces** - No polymorphism
- **No enums** - No sum types
- **typeof() limited** - Only returns "int" or "string"

#### Syntax Limitations
- **No string indexing** - Can't do `text[0]`
- **No array slicing** - Can't do `arr[1:3]`
- **No operator overloading** - Can't customize operators
- **No method chaining on literals** - Can't do `"hello".upper().len()`

### 2. Performance Limitations

#### Test Execution
- **Bash-based parallelism** - Uses xargs, not native concurrency
- **12 workers max** - Limited by CPU cores
- **Sequential compilation** - Each test compiles separately
- **No test caching** - Recompiles unchanged tests

**Current Performance:**
- 198 tests in ~140-160s (parallel)
- ~570s sequential
- 3-4x speedup

**Potential with Wyn's Spawn/Tasks:**
- Could use 50-100+ concurrent tasks
- Native async I/O instead of shell pipes
- Shared compilation cache
- Estimated: 10-20s for 198 tests (10-30x speedup)

#### Compiler Performance
- **Single-threaded** - No parallel compilation
- **No incremental compilation** - Recompiles everything
- **No caching** - Regenerates LLVM IR every time

### 3. Tooling Limitations

#### Error Messages
- **Basic error reporting** - Line numbers only
- **No suggestions** - Doesn't suggest fixes
- **No color coding** - Plain text errors

#### Debugging
- **No debugger integration** - Can't step through code
- **No stack traces** - Crashes show no context
- **Limited printf debugging** - println() is basic

#### IDE Support
- **LSP incomplete** - Basic features only
- **No autocomplete** - Can't suggest methods
- **No go-to-definition** - Can't navigate code

### 4. Standard Library Limitations

#### Missing Modules
- **No regex** - Pattern matching unavailable
- **No datetime** - Time handling basic
- **No HTTP client** - Network requests manual
- **No JSON parsing** - JSON support incomplete

#### Limited Methods
- **String methods** - Missing split, join, format
- **Array methods** - Missing map, filter, reduce
- **Number methods** - Missing sqrt, pow, round
- **File I/O** - Basic read/write only

## Improvement Opportunities

### 1. Native Parallel Test Runner (HIGH IMPACT)

**Current:** Bash script with xargs  
**Proposed:** Wyn program using Spawn/Tasks

```wyn
// tests/parallel_runner.wyn
fn main() -> int {
    var tests = discover_tests("tests/unit");
    var tasks = [];
    
    // Spawn 100 concurrent tasks
    for test in tests {
        var task = spawn(run_test(test));
        tasks.push(task);
    }
    
    // Wait for all tasks
    var passed = 0;
    var failed = 0;
    for task in tasks {
        var result = task.await();
        if (result == 0) {
            passed = passed + 1;
        } else {
            failed = failed + 1;
        }
    }
    
    println("Passed: " + passed.to_string());
    println("Failed: " + failed.to_string());
    return 0;
}

fn run_test(path: string) -> int {
    // Compile and run test
    var compile = Process.run("./wyn", [path]);
    if (compile.exit_code != 0) {
        return 1;
    }
    
    var out_path = path.replace(".wyn", ".out");
    var run = Process.run(out_path, []);
    return run.exit_code;
}

fn discover_tests(dir: string) -> [string] {
    var tests = [];
    var files = Fs.read_dir(dir);
    for file in files {
        if (file.ends_with(".wyn")) {
            tests.push(dir + "/" + file);
        }
    }
    return tests;
}
```

**Benefits:**
- 10-30x faster (10-20s vs 140-160s)
- 100+ concurrent tasks vs 12 workers
- Native async I/O
- Better error reporting
- Test result caching
- Progress reporting

**Requirements:**
- Spawn/Tasks must be stable
- Process API must work
- Fs API must work
- String methods must work (✅ all fixed!)

### 2. String Interpolation (HIGH IMPACT)

```wyn
var name = "World";
var greeting = "Hello, {name}!";  // Instead of str_concat
```

**Implementation:**
- Parse `{expr}` in string literals
- Generate str_concat calls at compile time
- Support expressions: `"Result: {x + y}"`

### 3. Format Strings (MEDIUM IMPACT)

```wyn
println("Name: {}, Age: {}", name, age);
```

**Implementation:**
- Variadic function support
- Format specifiers: `{:d}`, `{:s}`, `{:x}`
- Type-safe formatting

### 4. Array Methods (HIGH IMPACT)

```wyn
var doubled = nums.map(fn(x) -> x * 2);
var evens = nums.filter(fn(x) -> x % 2 == 0);
var sum = nums.reduce(fn(acc, x) -> acc + x, 0);
```

**Implementation:**
- Lambda/closure support
- Higher-order functions
- Efficient iteration

### 5. Incremental Compilation (HIGH IMPACT)

**Current:** Recompile everything  
**Proposed:** Cache LLVM IR, only recompile changed files

**Benefits:**
- 10-100x faster rebuilds
- Better development experience
- Enables watch mode

### 6. Better Error Messages (MEDIUM IMPACT)

```
Error at line 42, column 15:
  var result = text.contains("hello");
                    ^^^^^^^^
  Method 'contains' expects 1 argument, got 0

Suggestion: Did you mean 'text.contains("hello")'?
```

**Implementation:**
- Track source locations
- Suggest fixes
- Color-coded output
- Show context

## Priority Ranking

### P0 (Critical - Do First)
1. **Native parallel test runner** - 10-30x speedup, dogfooding
2. **String interpolation** - Essential for usability
3. **Format strings** - Better debugging

### P1 (High Priority)
4. **Array methods** (map/filter/reduce) - Modern language feature
5. **String split/join** - Common operations
6. **Better error messages** - Developer experience

### P2 (Medium Priority)
7. **Incremental compilation** - Faster development
8. **Range function** - Cleaner loops
9. **Generics** - Type safety

### P3 (Low Priority)
10. **Regex** - Advanced string processing
11. **HTTP client** - Network operations
12. **Debugger integration** - Advanced debugging

## Implementation Plan

### Phase 1: Native Test Runner (Week 1)
1. Verify Spawn/Tasks stability
2. Implement test discovery
3. Implement parallel execution
4. Add result caching
5. Benchmark and optimize

**Expected Result:** 10-20s test runs (10-30x speedup)

### Phase 2: String Features (Week 2)
1. String interpolation
2. Format strings
3. String split/join

**Expected Result:** Much better string handling

### Phase 3: Array Methods (Week 3)
1. Lambda support
2. map/filter/reduce
3. Array slicing

**Expected Result:** Functional programming support

### Phase 4: Compiler Improvements (Week 4)
1. Incremental compilation
2. Better error messages
3. Performance optimizations

**Expected Result:** 10-100x faster rebuilds

## Conclusion

**Current State:** Production-ready for basic use  
**Biggest Limitation:** Test execution speed (bash-based)  
**Biggest Opportunity:** Native parallel test runner (10-30x speedup)  
**Next Steps:** Implement native test runner using Wyn's Spawn/Tasks

The compiler is stable and functional, but the test infrastructure is the bottleneck. By dogfooding Wyn's concurrency features, we can achieve dramatic speedups and validate the language's capabilities.
