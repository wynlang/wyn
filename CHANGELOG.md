# Changelog

## v1.11.0 — "The Developer Experience Release" (2026-03-30)

Make developers productive in their first hour. Every change improves error messages, tooling, or language ergonomics.

### Language Features

- **`enum.to_string()`** returns variant name: `Color.Red.to_string()` → `"Red"` (was `"0"`)
- **`for i, v in arr`** indexed iteration: `for i, v in ["a","b","c"] { println(i.to_string() + ":" + v) }`
- **`int?` optional syntax**: `var x: int? = OptionInt_Some(42)` — sugar for `OptionInt`
- **`"ha" * 3` string repeat**: `"-" * 40` produces a 40-character separator line

### Compiler Improvements

- **Missing return is now an error**, not a warning: `fn get() -> int { }` fails to compile
- **Type mismatch errors from Wyn**, not C: `var x: int = "hello"` shows clear Wyn error with line number and suggestion
- **Typed arrays**: `var arr: [int] = []` uses `WynIntArray` (raw `long long*`) — sum 1M ints 2x faster

### Tooling

- **`wyn test`** rewritten: cross-platform test runner using direct process spawning (no shell exec), works on Windows
- **`wyn fmt`** functional: 4-space indent, no semicolons, braces on same line. `wyn fmt --check` for CI

### Internal

- Converted strcat/strcpy to memcpy in codegen hot paths
- 4 new expect tests, 4 new examples (57-60)
- 5 new official package READMEs (opengl, sdl, wgpu, target-android, target-ios)

### Metrics

| Metric | v1.10 | v1.11 |
|--------|-------|-------|
| Unit tests | 110 | 110 |
| BDD tests | 32 | 36 |
| Binary size | 49KB | 49KB |
| fib(35) | 33ms | 34ms |
| Official packages | 31 | 36 |

## v1.10.0 — "The Quality Release" (2026-03-28)

No new language features. Every change is about making the existing language faster, safer, and more stable.

### Performance

- **fib(35)**: 120ms → 33ms (3.6x faster via -O2 release builds)
- **1M string append**: 12,143ms → 6ms (2,024x faster — RC header caches length + capacity)
- **1M `.len()`**: ~100ms → 1ms (O(1) from RC length cache, was O(n) strlen)
- **Array sort**: 189ms → 1ms for 10K ints (O(n²) bubble sort → O(n log n) inline introsort)
- **`wyn build`**: ~4s → 411ms (precompiled `libwyn_rt.a` + system cc)
- **Binary size**: 425KB → 49KB (dead code stripping)
- **Spawn overhead**: ~20μs → 6μs per task
- **4x parallel fib(35)**: perfect 4x scaling (33ms, same as sequential)
- **HashMap**: 17x insert, 23x get (FNV-1a + 4096 buckets)

### Memory Safety — Zero Leaks

Every known memory leak pattern is fixed. Verified with AddressSanitizer + UndefinedBehaviorSanitizer.

- **50+ string functions**: raw `malloc` → RC-tracked `wyn_str_alloc`
- **String concat temps**: left and right temporaries released after concat
- **Chained concat**: `"a" + x + "b"` — all intermediates released
- **String interpolation**: `"item ${i}"` — interpolation temps released
- **String reassignment**: `s = "new" + x` — old value released, ownership transfer
- **Function argument temps**: `consume("a" + x)` — arg released after call
- **println/print temps**: `println(x.to_string())` — temp released after print
- **Method chain intermediates**: `"hello".upper().trim()` — upper() result released
- **Method object temps**: `("a"+x).split(",")` — concat result released
- **Unused return values**: `some_fn()` returning string — released
- **Array scope cleanup**: `array_free()` at block exit, releases RC strings inside
- **HashMap scope cleanup**: `hashmap_free()` at block exit

### Thread Safety

- RC heap range tracking: atomic CAS loops (was non-atomic race condition)
- `wyn_rc_release`: proper `acq_rel` ordering + acquire fence before free
- `wyn_rc_retain`: relaxed ordering (only needs atomicity)
- String concat: replaced `realloc` with `alloc+copy` (was double-free when memory moved)
- Pool shutdown: `atexit` handler joins all threads cleanly

### Buffer Overflow Prevention

All 64KB fixed buffers replaced with dynamic RC-tracked buffers:
- `regex_find_all`, `regex_replace`, `regex_split`
- `File_glob`, `File_walk_dir`, `array_join`
- All `sprintf` → `snprintf` with bounds
- All hot-path `strcat` loops → `memcpy`

### Bug Fixes

- **Stdlib param counts**: all ~100 builtin module functions (Json, Http, Db, Crypto, Regex, DateTime) had hardcoded 1-param limit in the checker. `Json.get(handle, "key")` was rejected as "wrong number of arguments". Now all work correctly.
- **`int.upper()` segfault**: calling a string method on an int variable crashed at runtime. Now gives clear error: "Unknown method 'upper' for type 'int'".
- **`Http.listen()` missing**: was registered in checker but had no implementation. Added as alias to `Http_serve`. Also added `Http.method()`, `Http.path()`, `Http.fd()` helpers for parsing request data.
- **String array `.sort()`**: used int comparator instead of `strcmp`. Now correctly dispatches to string sort.
- **`.sort()` was a no-op**: codegen emitted `array_sort_copy()` which returned a sorted copy that was discarded. Now emits in-place `array_sort(&arr)`.
- **Signed division**: `-7 / 2` gave `-4` (arithmetic shift rounds toward -∞). Removed shift strength reductions for signed division. Now correctly gives `-3`.
- **Division by zero**: panics at runtime with clear message (was silent 0).
- **Triple method chain**: `"hello".trim().upper().reverse()` crashed because codegen assumed `.reverse()` always returns array. Now checks object type.
- **Variable shadowing**: inner block variables properly scoped via `#undef`/`#define` restore.
- **Recursive spawn**: `pool_try_run_one` in `future_get` prevents deadlock for moderate depth.
- **`int_array_get` OOB**: panics with clear message (was silent 0).

### Developer Experience

- **Source-line error messages**: errors point to the exact line in your `.wyn` file
- **LSP struct field completions**: autocomplete for struct fields in editors
- **`wyn doctor`**: shows active compile path and speed recommendations
- **4-platform CI**: macOS, Linux, Windows (ARM + x64), all green
- **Online playground**: deployed at play.wynlang.com

### Testing

- 110 unit tests (was ~80)
- 31 expect + regression tests (was 0)
- ASan + UBSan clean across all patterns
- 30/30 concurrent stress runs correct

---

## v1.9.0 (Previous)

Generators, Debugger, 36 Packages — all roadmap phases complete.
