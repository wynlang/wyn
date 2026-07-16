# Changelog

## v1.17.0 — "The Ecosystem Release" (2026-07-16)

The big one. Wyn gets a real package ecosystem: a **git-URL package manager** (no
central registry — a dependency is just a git repo), a C FFI that generates
bindings from a header and pulls curated C libraries with one command (proven end
to end against SQLite), and concurrency on a coroutine scheduler by default
(cooperative I/O everywhere) with cooperative task cancellation. `print()` becomes
properly Python-style. Plus Pythonic sugar and a long tail of correctness fixes.

**Heads up — breaking changes** (see *Removed / changed* below): the `|>` pipe
operator, the `&&`/`||`/`!` operators, and `try`/`catch`/`throw` are gone;
`import m` now requires qualified calls (`m.foo()`); the `pkg.wynlang.com` registry
and its `wyn pkg register/login/publish` commands are removed in favour of git
URLs. Migration is mechanical.

### Package manager (git URLs, no registry)

- **A dependency is a git repo.** `wyn add args` expands a bare name to
  `github.com/wynlang/args` (the "official" convention — org membership is the
  source of truth); `wyn add github.com/bob/lib@v1.2` takes any repo, any host, at
  an optional tag/branch/commit; `wyn add <url> --as name` overrides the import
  name. `wyn remove`, `wyn list`, and `wyn install` (restore from `wyn.lock`) round
  it out. Dependencies live in `wyn.toml [dependencies]`; `import <name>` resolves
  through it.
- **Global content-addressed cache** (`~/.wyn/pkg/<host>/<owner>/<repo>@<ref>`,
  overridable via `WYN_PKG_CACHE`), cloned once and shared across projects, pinned
  by commit in `wyn.lock` for reproducible builds.
- **Publishing is `git push` + `git tag`** — there is no registry server to talk to.
- **Private repos just work**: the compiler shells out to `git clone`, so your
  ambient git auth (HTTPS credential helper, stored token, or SSH key/agent, incl.
  `git@host:owner/repo` URLs) authenticates with no extra Wyn config.
- The old `pkg.wynlang.com` client and `wyn pkg register/login/whoami/search/
  publish/push` are removed.

### C ecosystem (FFI)

- **`wyn add <lib>` — curated C packages, end to end.** For a curated C library,
  one command resolves it, generates its bindings, links it, and records the flags
  in `wyn.toml`, so you can `import` it and start calling. Nine recipes ship:
  `m` (libm), `z` (zlib), `curl`, `sqlite3`, `crypto`/`ssl` (OpenSSL), `curses`
  (ncurses), `readline`, and `xml2` (libxml2).
- **`wyn bind <header.h>` — binding generation.** Reads a C header and emits the
  Wyn `extern fn` declarations for everything representable by the FFI type map.
- **`wyn add` TUI.** Run `wyn add` with no name to browse/filter/preview the
  curated registry interactively (falls back to a plain list when non-interactive).
- **`Ptr` — pointer cells for C out-parameters.** `Ptr.cell()`/`read()`/`write()`/
  `free()` give you a heap slot to pass where C wants a `T**` (e.g.
  `sqlite3_open(path, sqlite3** out)`) and read the handle back — the piece that
  makes opaque-handle APIs usable.
- **By-value FFI structs + typed `cstr`** (raw `char*`), and `ptr` is a first-class
  type consistent across the checker and codegen.
- **Dogfooded against SQLite:** open a DB, create a table, insert rows, and query
  them back through prepared statements — all from Wyn (see the C-FFI guide).
- FFI robustness: standard-header symbols (`strlen`, `toupper`, the math library,
  …) no longer clash with the prototypes Wyn emits; `string` coerces to a `ptr`
  parameter; imported C-package bindings link correctly.

### Concurrency

- **Coroutine scheduler is now the default.** Awaited `spawn` / `await_all` /
  `parallel { }` run as coroutines on an M:N scheduler, so cooperative I/O and
  `Time::sleep` engage everywhere (not just for fire-and-forget). Awaiting from
  `main` pumps the scheduler and never deadlocks. The old thread pool remains as a
  fallback behind `WYN_ASYNC_POOL=1`.
- **Cooperative task cancellation.** `Task.cancel(handle)` requests cancellation;
  a task checks `Task.is_cancelled()` and returns early. Cooperative and
  leak-on-cancel (no forced unwind).
- Fire-and-forget `spawn` is drained (and its output flushed) at program exit, so
  orphan tasks run reliably; unbound `spawn` inside `parallel { }` is now joined.

### Language & syntax

- **`print()` is now Python-style.** `print(a, b, c)` prints its arguments
  space-separated with a trailing newline; `print(x, end: "")` suppresses the
  newline; `print(a, b, sep: "-")` sets the separator. `println` remains as an
  alias. (Previously `print` was inconsistent — no newline for strings/ints, a
  newline for arrays, and a silently-dropped second argument.)
- **Namespaced imports (W9):** `import m` exposes members as `m.foo()` — the
  canonical, collision-free form. A bare `foo()` after `import m` is now an error
  (selective `import { foo } from m` keeps the flat call).
- **`?.` optional chaining** — `x?.field` / `x?.method()` short-circuit on `None`.
- **`zip()` + pair destructuring** — `for x, y in zip(a, b)`.
- **Optional struct fields** — a `Struct?` (or `int?`/`string?`/…) field now
  compiles and matches correctly.
- **Closure copy** — `var g = f` where `f` is a closure.
- **Braceless single-statement bodies** for `for`/`if`/`else`/`while`.
- **`_` as a throwaway for-loop variable.**
- Variables may be named after C keywords (`int`, `long`, `char`, …).

### Removed / changed (breaking)

- **`|>` pipe operator removed** — compose with nested calls or intermediate
  `var`s.
- **`&&` / `||` / `!` removed** — use `and` / `or` / `not` (already canonical).
- **`try` / `catch` / `throw` removed** — use `Result` + `Ok`/`Err` + `match` + `?`.
- **`import m` requires `m.foo()`** — a flat `foo()` no longer resolves.
- **The `pkg.wynlang.com` registry is gone** — `wyn pkg register/login/whoami/
  search/publish/push` are removed; use git URLs (`wyn add <url>`) and publish by
  pushing a repo + tag.

### Fixes & internals

- Memory-safety: `print()` on an int expression, a module-load use-after-free, and
  a JSON out-of-bounds read — all fixed and ASan-verified.
- Pattern matching / data enums: statement-form match, multi-field variants,
  arrays of enum values, tuple-variable destructuring.
- Struct methods: `self.other()` dispatch, method params, struct-body method
  typing, single-letter struct names; the checker now errors on calls to methods a
  struct doesn't have.
- `wyn fmt` no longer corrupts inclusive ranges (`..=`) or slices (`[a:b]`).
- LSP: reliable diagnostics via `wyn check`, real messages, protocol tests, and
  autocomplete on C-package bindings.
- Variables named after C keywords (`int`, `long`, `char`, …) compile correctly.
- Map literals: `{k: someVar}` with a typed non-literal value stored the wrong type
  (defaulted to int) and read back garbage — the three map-store paths now share
  one type selector, so store and load agree.
- FFI: variables/params of type `ptr` and `cstr` lower correctly; `string` coerces
  to a `ptr` parameter; imported C-package `extern fn`s link; an `extern fn` for a
  standard-header symbol (`strlen`, math functions, …) no longer conflicts.
- Compiler cleanup: dead code removed (`scope.c`, the old registry/semver client),
  runtime source lists de-duplicated; warning-clean.

## v1.16.0 — "The Batteries Release" (2026-07-13)

Batteries included. This release makes Wyn feel more like Python where it counts
and — the big one — turns on a real C FFI, so the entire C ecosystem is now
reachable from Wyn. All backward compatible; no source changes required.

### C interop (FFI)

- **`extern fn` actually works now, with real types and library linking.** You can
  declare a C function and call it directly. Scalar types (`int`, `float`, `bool`),
  `string`, and `void` all map correctly (previously only `int`/`string` worked and
  everything else silently miscompiled).

  ```wyn
  extern fn sqrt(x: float) -> float;

  fn main() {
      println("${sqrt(16.0)}")     // 4.0 — links the C math library
  }
  ```

- **Link any C library from `wyn.toml`.** A new `[ffi]` section tells the compiler
  what to link:

  ```toml
  [ffi]
  libs = "curl, z"
  lib_dirs = "/usr/local/lib"
  include_dirs = "/usr/local/include"
  ```

  The named libraries are linked into your program automatically. (For safety, the
  compiler rejects any `[ffi]` value containing shell metacharacters.)

  This is phase one of Wyn's ecosystem story: decades of battle-tested C libraries
  are now within reach, with automatic binding generation and a package-manager TUI
  to follow.

### Pythonic sugar

- **`range(a, b)` and `range(a, b, step)`** as a `for`-loop iterator, including
  descending ranges with a negative step:

  ```wyn
  for i in range(0, 10, 2)  { … }   // 0 2 4 6 8
  for i in range(10, 0, -1) { … }   // 10 9 8 … 1
  ```

- **`if let`** conditional binding — match one case inline without a full `match`:

  ```wyn
  if let Some(v) = find(key) {
      println("found ${v}")
  } else {
      println("missing")
  }
  ```

  Works on any `Option` or `Result`, with or without an `else`.

## v1.15.0 — "The Payloads Release" (2026-07-13)

Two correctness fixes that remove long-standing sharp edges: any scalar can now
ride inside an `Option`/`Result`, and your function names no longer collide with
the C standard library.

### Optionals & Results

- **`float?` / `bool?` and `Result<float, _>` / `Result<bool, _>` now compile.**
  Previously only `int` and `string` payloads worked — a `-> float?` function
  failed to compile with an internal type mismatch. Now every form works:
  returning `Some`/`None`/`Ok`/`Err`, statement- and expression-position
  `match`, and the `.is_some()` / `.unwrap()` / `.unwrap_or(default)` methods.

  ```wyn
  fn half(x: float) -> float? {
      if x < 0.0 { return None }
      return Some(x / 2.0)
  }

  fn divide(a: float, b: float) -> Result<float, string> {
      if b == 0.0 { return Err("division by zero") }
      return Ok(a / b)
  }
  ```

### Functions

- **Function names can now shadow C standard-library symbols.** A function
  called `connect`, `read`, `write`, `open`, `close`, `socket`, `send`, `link`,
  `index`, and friends used to fail to compile (`static declaration of 'connect'
  follows non-static declaration`). Those names are now free to use — the
  generated C is transparently namespaced, so your Wyn code is unaffected.

  ```wyn
  fn connect(host: string) -> string {
      return "connected to ${host}"
  }
  ```

## v1.14.0 — "The Polish Release" (2026-07-11)

Follows the Pythonic release with a batch of ergonomics and correctness fixes
that smooth over the last rough edges in optionals, results, match, and printing.

### Optionals & Results

- **`int?` / `string?` / `Result<T, E>` work as return types.** You no longer
  need the mangled `OptionInt` / `ResultInt` names anywhere:
  `fn find(t: int) -> int? { return Some(t) }` and
  `fn parse(s: string) -> Result<int, string> { ... }` now type-check and flow
  through calls, variables, and `match` correctly.
- **`match` on any Option/Result works in expression position.**
  `var r = match opt { Some(v) => ..., None => ... }` handles string, float, and
  bool payloads (previously an internal codegen error, even for int).

### match & control flow

- **Exhaustive `if/else` counts as a return.** A function ending in an
  `if/else` (or `if/else if/else`) where every branch returns is no longer
  wrongly rejected with "may not return a value". A genuinely missing return is
  still an error.

### Printing & indexing

- **`println(array)` works** — arrays and `map.keys()` print their real values
  with a newline (was a type error; `print(arr)` already worked).
- **String negative indexing** — `s[-1]` is the last character (matches arrays).

### Functions

- **Default args infer their type.** `fn greet(who = "world")` works — the
  parameter type comes from the default value's literal, so you don't have to
  write `who: string = "world"`. Typed defaults are unchanged.

## v1.13.0 — "The Pythonic Release" (2026-07-11)

Wyn gets a lot more Python-like, plus a deep batch of codegen correctness and
memory-safety fixes. All memory fixes verified under AddressSanitizer.

### Language ergonomics ("feels like Python")

- **`in` / `not in` membership.** `x in list`, `key in map`, `sub in string`,
  and their `not in` negations, at comparison precedence.
- **Negative indexing.** `a[-1]` is the last element (was a runtime panic).
- **Open-ended slices.** `a[:2]`, `a[2:]`, `a[:]` (empty start defaults to 0,
  empty end to the length). `a[i:j]` unchanged.
- **Dict iteration.** `for k, v in map` binds key + value; `for k in map`
  iterates keys.
- **Tuple unpacking & multi-assignment.** `var a, b = 10, 20`,
  `var lo, hi = minmax()`, and swap/rotate `a, b = b, a` / `x, y, z = z, x, y`
  (all right-hand values are evaluated before any assignment).
- **`Some`/`None`/`Ok`/`Err` work for any payload** in any context — you never
  need to write a mangled `OptionString_Some(...)` form again.

### Correctness (codegen type selection)

- **HashMap values are correctly typed.** `m[k]` and `m.get(k)` on int/float/bool
  maps returned garbage (always used the string getter) — even the shipped
  hashmap example printed a blank value. Now typed from the map's value type;
  also fixed a segfault on `if m.get(k) != n`.
- **`match` fixes.** Statement-form `match` on a payload enum no longer
  miscompiles; multi-field variants (`Rect(w, h)`) bind each field with its real
  type; a `Result<int, string>` `Err(e)` arm treats `e` as a string; a
  non-exhaustive `match` no longer reads uninitialized memory (result is
  zero-initialized, and the checker flags uncovered variants).
- **Float & bool arrays preserve values.** `[1.5, 2.5]` previously truncated to
  `[1, 2]`; float/bool array literals now store, index, iterate, and print
  correctly.
- **Array/collection printing.** `print(["a", "b"])` and `print(map.keys())` show
  the real values instead of `[0, 0]`.

### Silent-miscompile & hang fixes

- **String ternary** (`cond ? "a" : "b"`) no longer prints a garbage pointer.
- **Chained comparison** (`0 < x < 10`) is rejected at compile time with a
  message pointing to `0 < x and x < 10` (was silently evaluated as `(0<x)<10`).
- **`elif` no longer hangs the compiler** — it errors with "use `else if`". A
  general no-progress guard now protects every statement-parsing loop from hangs.

### Memory safety

- **Use-after-free fixes** for a local string moved into an array, a struct
  field, an `Option`/`Result`, or an enum variant, then released at scope exit
  while the container still aliased it. All ASan-verified with regression tests.

## v1.12.0 — "The Hardening Release" (2026-07-09)

Memory-safety fixes, deeper concurrency, and a warning-clean build. Three of the
fixes below are genuine bugs that shipped in v1.11.0 — upgrading is recommended.
All memory fixes were verified under AddressSanitizer.

### Memory safety

- **String concat use-after-free fixed.** `a + b` where the result was reused as a
  later operand (`j = a+b; m = j+c`) could corrupt the live string and double-free
  it. `wyn_string_concat_safe` now always returns a distinct allocation.
- **print/println double-free fixed.** Printing a fresh interpolated/temp string
  (`println("x=${x}")`) could release it twice.
- **Database/JSON query heap overflow fixed.** `Db.query` / `Db.query_params` /
  `Json.keys` wrote unbounded results into fixed 64KB/4KB buffers — any larger
  result overflowed the heap. They now grow as needed (verified on a ~168KB result).
- **String interpolation no longer truncates at 512 bytes.** Interpolated strings of
  any length now round-trip intact (previously silently cut to 511 bytes).
- **Reference-count heap detection hardened** with a dual sentinel, and two compiler
  buffers made bounds-safe against overly long identifiers.

### Concurrency

- **Cooperative `Time::sleep`.** Inside the coroutine scheduler (fire-and-forget
  `spawn`), sleeping now yields to other tasks instead of blocking the worker
  thread. In one benchmark, 50 concurrent 200ms sleeps finished in ~0.21s instead of
  ~1.03s. (Awaited/`parallel` spawns run on the thread pool and still block — see
  the roadmap.)

### Build & tooling

- **Warning-clean build** under `-Wall -Wextra`.
- **Runtime library rebuild fixed** — `make` now rebuilds `libwyn_rt.a` when a
  runtime source or header changes, so programs no longer link a stale runtime.

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
