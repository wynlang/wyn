# Changelog

## v1.20.0 (unreleased)

Correctness release: compiler limits removed, lambdas fully typed, and a
snapshot suite guarding the generated C.

- **No more silent compiler limits**: every fixed-size compiler registry
  (lambdas, structs, enums, tuples, modules, match arms, block bodies, and
  ~40 more) now grows on demand. Previously, programs exceeding a cap
  (commonly 256) were silently miscompiled - 300 lambdas produced invalid C,
  the 65th data enum broke match exhaustiveness, and a 40-field struct or
  40-arm match could crash the compiler outright. All fixed and covered by
  regression tests.
- **Typed lambda parameters**: lambda params are no longer limited to
  int/string. Type annotations work in all three lambda forms
  (`(x: float) => x * 2.0`, `|b: bool| not b`, `fn(p: Point) => p.x`),
  float/bool are inferred from the body, and `.map()`/`.filter()` type
  unannotated params from the array's element type - so
  `points.map((p) => p.x)` just works. `.map()`/`.filter()` results are now
  correctly typed for all element types, including structs.
- **Closure correctness**: closures over float signatures
  (`fn(float) -> float`) previously returned garbage through the int call
  path - calls now go through the properly typed signature. Also fixed a
  compiler crash when calling a function-typed local.
- **Bare `return` in script mode**: a bare `return` in top-level script code
  (and in the test runner) now compiles to `return 0` instead of producing
  invalid C.
- **Recursive struct cycles rejected cleanly**: mutually-recursive struct
  fields are now a proper check-time error instead of a C compile error or
  internal compiler error.
- **Closure copies work**: assigning a closure-typed variable
  (`var g = f`) now compiles and safely shares the environment; a leak on
  closure reassignment was fixed along the way.
- **Golden-C snapshot suite**: 30 representative programs now have their
  generated C snapshotted and diffed on every `make test`, catching silent
  codegen changes.
- **LICENSE shipped in release artifacts**: the MIT license file is now
  included in every platform tarball/zip.

## v1.19.1 (2026-07-21)

Identity and polish patch - no language changes, drop-in upgrade from v1.19.0.

- **Clean CLI output**: `wyn`, `wyn version`, and `wyn --help` print a single
  informative line instead of ASCII art. The visual identity (the new
  wyvern-W emblem) lives at wynlang.com; the terminal stays clean and
  script-friendly. `wyn version` prints to stdout for easy parsing.
- **Fixed PATH invocation** (also shipped in the re-tagged v1.19.0): the
  installed binary resolves its runtime from the real executable path, so
  `wyn run x.wyn` works when invoked through PATH - not just as `./wyn`.
- Website: new wyvern-W emblem as logo/favicon/social image; WYN block
  wordmark on the homepage and 404 page.

## v1.19.0 - "The DX Release" (2026-07-20)

The developer-experience release: a real test runner, working project templates,
supply-chain auditing, the first official web package, the module-codegen
fixes that make writing Wyn packages actually pleasant - plus two hardening
passes that fixed every known silent-wrong-answer bug and put the compiler
under continuous fuzzing and sanitizer gates. No breaking changes.

**Correctness: no more silent wrong answers**
- **Structs compare with `==`**: `if a == b` on struct values works -
  field-wise (strings by content, nested structs recursively). It used to be
  an internal compiler error on day-one code. Structs with non-comparable
  fields (arrays/maps), cross-type compares, and ordering (`<`) give clear
  check-time errors instead.
- **Channels are typed**: a channel's element type is inferred from its first
  `send` and enforced from then on. `ch.send("hello")` → `recv()` returns the
  string (it used to print a pointer number); floats round-trip exactly
  (`3.14` used to come back as `3`); sending the wrong type is a check-time
  error.
- **Mismatched collection stores are check-time errors**: pushing a string
  onto an int array, or storing a string into an int-valued map slot, used to
  pass `wyn check` and corrupt memory at runtime. All store paths (method
  `push`, `m[k] = v`, `a[i] = v`) are enforced now; `int`↔`float` stays
  permissive.
- **Awaiting a future twice returns the same value**: results are memoized.
  The second `await f` used to return 0 silently - or, worse, steal a
  *different* spawn's result; float results crashed. Also fixed a race that
  could hand the same future to two spawns, and spawned functions' float
  arguments/returns no longer truncate (`spawn half(7.0)` used to return 3).
- **Nested Options work**: `Some(Some(x))` with nested `match` compiles and
  runs (four distinct compiler bugs fixed), including string payloads,
  `Some(None)`, triple nesting, and lowercase `none` match arms.
- **`select` dispatches correctly at every arity**: one-arm and 4+-arm
  selects used to always run the *first* arm (hanging if its channel was
  empty). A `select` that can never receive - no ready channel, no live
  tasks - now exits with a deadlock error instead of hanging forever.
- **`println(struct)` prints the fields** (`Point { x: 3, y: 4 }`) and
  `println(option)` prints `Some(5)` / `none` - both were internal compiler
  errors. Nested array assignment (`m[0][1] = v`) and lambda captures inside
  `"${...}"` interpolation are fixed too.

**Match statements** (statement position now equals expression position)
- Or-patterns `1 | 3 | 5 =>` work (they used to compile to `if (0)` - the arm
  silently *never* matched), guards `x if x > 2 =>` no longer crash the
  compiler, and range patterns `1..5` / `1..=5` parse.

**Stability: the parser can't be crashed**
- A fuzzing harness now runs in `make test` and CI. Every crash and hang it
  found is fixed: four parser infinite loops, a segfault on `===`, UTF-8 BOM
  silently emptying entire programs, `split("")`/`replace("")` hangs,
  `.chars()`/`.reverse()` corrupting multi-byte UTF-8, `.reduce(init, fn)`
  segfaulting, and more. Invariant: **0 crashes, 0 hangs, 0 internal errors**
  on malformed input.

**Error messages that tell the truth**
- **Unterminated strings point at the opening quote** with "Unterminated
  string literal", instead of a misleading `Expected '}'` at the end of the
  file - and you get *one* error, not a cascade of three identical ones.
- **Python/JS habits get targeted fixes**: `def`, `lambda`, `True`, `null`,
  `let`, `function`, `console`, `try` and friends each produce the exact Wyn
  equivalent (`functions are declared with 'fn': fn add(a, b) {...}`) instead
  of a generic parse error.
- `wyn --version` reports the real version everywhere (it was cwd-dependent),
  `wyn check`/`run` on a directory fails instead of false-greening, cached
  `wyn run` propagates the program's real exit code, and `wyn test` with zero
  matching tests says so instead of reporting success.

**Self-upgrade that works (and can't hurt you)**
- **`wyn upgrade`** updates to the latest release; **`wyn upgrade 1.18.0`**
  installs an exact version (up or down). The new binary is downloaded to a
  temp dir, extracted, and proven to run before anything is touched - a
  failed download can no longer damage the install (previously a 404
  response body could overwrite the binary). Bare `wyn upgrade` never
  downgrades; an explicit version pin is honored exactly.

**Hardening gates (CI)**
- Every commit now runs the full suite under **AddressSanitizer** (the
  runtime, where the real memory bugs live) and **ThreadSanitizer** (all
  concurrency tests, under *both* executor configurations) - plus an
  install-layout canary that blocks any release whose tarball wouldn't
  actually run after `install.sh`. The TSan gate caught and fixed a real
  data race in `select` before this release.

**New tooling**
- **`wyn test` is a real test runner for your projects**: discovers
  `tests/test_*.wyn` and `*_test.wyn` (one subdirectory level too), runs them,
  and - crucially - **failing assertions now fail the run** (they used to exit 0
  and count as passed). `wyn test <name>` filters by name; an empty project
  prints a copy-pasteable starter test.
- **`wyn new <name> --template cli|api|web|lib`** scaffolds a runnable project
  (wyn.toml, src/, a passing test, README, CI workflow). The templates now
  actually compile - they had shipped with the removed `&&`/`||` syntax - and
  every template is CI-verified to check clean and pass its own tests.
  `http-service` is an alias for `api`.
- **`wyn pkg audit`** verifies your lockfile against reality: a **moved tag**
  (the classic retag/force-push supply-chain attack) is an error, branch pins
  warn with the exact command to pin today's SHA, local cache tampering is
  detected, and dependencies that link native code via `[ffi]` are flagged for
  review. `--offline` skips remote checks. Exit code = worst finding.

**Language: string lambdas** (the biggest expressiveness gap, closed)
- **String-parameter lambdas work end-to-end**: `words.map((s) => s.upper())`,
  `names.map((s) => "Hi " + s + "!")`, `words.filter((s) => s.len() > 3)` -
  string methods, string concat, and identity lambdas on `[string]` arrays all
  compile and run. Lambda bodies now go through the real expression code
  generator instead of a separate int-only mini-emitter, so every expression
  form works inside a lambda. `.map()`/`.filter()` on a `[string]` array
  return `[string]` (element types are preserved through the chain).

**Ecosystem**
- **`wyn pkg add web`** - the first official batteries-included web package
  (github.com/wynlang/web): request parsing, JSON/HTML responses, traversal-safe
  static files, an HTML page builder, spawn-per-request concurrency.
- **Curated C recipes: 12 → 31.** `wyn add png|jpeg|pcre2|yaml|ffi|uuid|ev|uv|
  sdl2|pthread|expat|bz2|iconv|gmp|onig|ssh2|git2|archive|magic` - each with
  headers, pkg-config, and per-OS install hints. Every recipe is verified
  end-to-end (add → bind → link → run).
- **`wyn bind` handles real-world headers.** Bindgen now resolves typedefs
  (`png_uint_32`, `uv_file`, opaque handle pointers - recorded from the whole
  preprocessed unit), scans prototypes behind macOS availability/`__asm`
  attributes, handles many declarations per line (pcre2's macro-generated
  API), and strips `_Nullable`/`restrict`. Measured on real libraries:
  png 0→232 bound functions, pcre2 0→212, pthread 0→94, archive 434,
  OpenSSL crypto 729. Recipes can carry required `-D` defines
  (`wyn bind -D` works too).
- **Multi-package projects link.** Repeated `[ffi]` sections in wyn.toml
  (one per `wyn add`) now accumulate - previously only the LAST added
  package's libs survived, so any project using two C packages failed at
  link time.

**Module/package correctness** (found building that package)
- Fixed four imported-module codegen bugs: sibling calls to functions with
  common names (`path`, `text`, …) lost their module prefix; `for x in xs` loop
  variables miscompiled; **returning a composed string
  (`return "a" + t + "b"` or `"${t}"`) released `t` before reading it** - a
  use-after-free that silently produced empty output (this one was general, not
  module-only); unannotated void module functions emitted conflicting C types.

**Concurrency**
- Coroutine stacks are now 8MB (lazily committed - no RSS cost) instead of
  64KB: deep recursion and heavy sort/parse workloads inside awaited spawns no
  longer crash with SIGBUS.
- `println(await_all(futs))` prints the result array (was an internal error).

**Forgiveness & errors**
- `return x` and `return none` auto-wrap in `fn -> T?` functions - no more
  mandatory `Some(x)` / `None()` ceremony.
- `opt == None` / `!= none` give a targeted "use `.is_none()` / `.is_some()`"
  error instead of a C-level crash.
- Unknown methods on strings/arrays are rejected at check time with a
  "Did you mean: `.upper()`?" hint (they used to pass `wyn check` and die in
  the C compiler). The method table gained the missing entries this exposed
  (array `min`/`max`/`sum`/`average`/`clear` and friends).
- Multi-arg and zero-arg `println` work (Python-style, like `print`).
- Statement-position `mut` no longer hangs the compiler; recursive struct
  fields are rejected at check time with a clear message.

**Web performance** (with the `web` package)
- **HTTP/1.1 keep-alive**: persistent connections instead of a TCP handshake
  per request - measured **22,000+ req/s with zero failed requests at 200
  concurrent connections** on the hello example (3× the previous throughput).
- **Concurrent accept**: the old accept path read the request on the accept
  loop, so one slow client stalled every other connection (and could wedge
  the server under load). New `Http.accept_fd` + `Http.read_request` split:
  accept-only main loop, request reads park cooperatively in each handler's
  coroutine. One dead client can also no longer kill the server (SIGPIPE
  protection - none existed).

**Performance**
- Code generation no longer flushes per fragment - large builds are ~25%
  faster on the frontend side.
- The checker's symbol table is hashed - `wyn check` on very large files is
  now near-linear instead of O(n²) (name lookups were 95% of checker time).
- Dev builds are ~40% faster on macOS: the 5,400-line runtime header is
  precompiled once (`make runtime`) instead of re-parsed on every
  `wyn build`/`wyn run` (hello: 830ms → 500ms).
- Compiled binaries no longer carry a 2.1MB zeroed scheduler-deque block -
  the work-stealing deques are allocated at scheduler start, sized to the
  real core count (hello's data segment: 2.1MB → 16KB).

**Tests**: 146 expect/regression tests + 20 dedicated sub-suites (parser
stability, struct equality, collection types, select deadlock, unterminated
strings, scaffolding, test runner, pkg audit, module codegen, and more) + the
fuzz invariants + ASan/TSan sanitizer sets, all green on all platforms.

## v1.18.0 - "The Correctness Release" (2026-07-17)

A focused hardening pass: a batch of real correctness fixes found by dogfooding the
whole corpus (all 48 sample-apps now build), a memory-safety fix in codegen, a big C-FFI
reliability win, and the first steps of the lambda + dot-syntax cleanups. No breaking
changes - a drop-in upgrade from v1.17.

**Memory safety**
- Fixed a heap-use-after-free in codegen's string-var scope tracking: on large programs
  with nested scopes + string-into-array moves, a freed name pointer was re-exposed and
  its bytes emitted into a `wyn_rc_release(...)` call, producing invalid (non-UTF-8) C.
  ASan-verified; the 2600-line JS-interpreter sample now builds reliably.

**Compiler correctness**
- `.sort()` / `.reverse()` work as expressions and chain (`xs.sort().reverse()`).
- `for row in [[...],[...]]` binds each row as the sub-array (was a silent `0 0`);
  `var d = structArray[i]` binds the struct element type (was `long long` → C error).
- `match` on a `Result`/`Option` payload **inside a loop** compiles.
- `m.get(k).unwrap_or(default)` on int maps; `len(s)` on a string; a namespaced stdlib
  call returning a string (`Color.green(...)`) is typed correctly (incl. its codegen name).
- Bare `return` in an inferred-void `main`; the parser no longer hangs on the removed
  `&&`/`||` (clean "use `and`/`or`" error); array-index typing no longer keys off names.

**C FFI / ecosystem**
- `wyn bind` handles export-macro'd headers (strips `__attribute__((...))`) - unlocking
  real libraries (**lz4 0→44, zstd 0→59** bindings); bare `unsigned` returns map to int.
  New curated recipes: **lz4, zstd, jsonc**.
- `wyn add` records pkg-config's `-L` dirs into `[ffi].lib_dirs`, so libraries off the
  default path (Homebrew Cellar) actually link.

**Language surface**
- Lambda parameter types are inferred; a string/float-parameter lambda gives a clear
  "not supported yet" message + workaround (was a cryptic error or silent miscompile).
  Integer lambdas + `.map`/`.filter`/`.reduce` unchanged.
- The `File` namespace works uniformly via both `File.x` and `File::x` - a step toward
  `.` as the one canonical call syntax.

**Tooling / tests**
- New `wyn fix` migrator for removed syntax (`&&`→`and`, `||`→`or`, `elseif`→`else if`).
- De-flaked the fire-and-forget drain test; new regression tests across all of the above.

## v1.17.0 - "The Ecosystem Release" (2026-07-16)

The big one. Wyn gets a real package ecosystem: a **git-URL package manager** (no
central registry - a dependency is just a git repo), a C FFI that generates
bindings from a header and pulls curated C libraries with one command (proven end
to end against SQLite), and concurrency on a coroutine scheduler by default
(cooperative I/O everywhere) with cooperative task cancellation. `print()` becomes
properly Python-style. Plus Pythonic sugar and a long tail of correctness fixes.

**Heads up - breaking changes** (see *Removed / changed* below): the `|>` pipe
operator, the `&&`/`||`/`!` operators, and `try`/`catch`/`throw` are gone;
`import m` now requires qualified calls (`m.foo()`); the `pkg.wynlang.com` registry
and its `wyn pkg register/login/publish` commands are removed in favour of git
URLs. Migration is mechanical.

### Package manager (git URLs, no registry)

- **A dependency is a git repo.** `wyn add args` expands a bare name to
  `github.com/wynlang/args` (the "official" convention - org membership is the
  source of truth); `wyn add github.com/bob/lib@v1.2` takes any repo, any host, at
  an optional tag/branch/commit; `wyn add <url> --as name` overrides the import
  name. `wyn remove`, `wyn list`, and `wyn install` (restore from `wyn.lock`) round
  it out. Dependencies live in `wyn.toml [dependencies]`; `import <name>` resolves
  through it.
- **Global content-addressed cache** (`~/.wyn/pkg/<host>/<owner>/<repo>@<ref>`,
  overridable via `WYN_PKG_CACHE`), cloned once and shared across projects, pinned
  by commit in `wyn.lock` for reproducible builds.
- **Publishing is `git push` + `git tag`** - there is no registry server to talk to.
- **Private repos just work**: the compiler shells out to `git clone`, so your
  ambient git auth (HTTPS credential helper, stored token, or SSH key/agent, incl.
  `git@host:owner/repo` URLs) authenticates with no extra Wyn config.
- The old `pkg.wynlang.com` client and `wyn pkg register/login/whoami/search/
  publish/push` are removed.

### C ecosystem (FFI)

- **`wyn add <lib>` - curated C packages, end to end.** For a curated C library,
  one command resolves it, generates its bindings, links it, and records the flags
  in `wyn.toml`, so you can `import` it and start calling. Nine recipes ship:
  `m` (libm), `z` (zlib), `curl`, `sqlite3`, `crypto`/`ssl` (OpenSSL), `curses`
  (ncurses), `readline`, and `xml2` (libxml2).
- **`wyn bind <header.h>` - binding generation.** Reads a C header and emits the
  Wyn `extern fn` declarations for everything representable by the FFI type map.
- **`wyn add` TUI.** Run `wyn add` with no name to browse/filter/preview the
  curated registry interactively (falls back to a plain list when non-interactive).
- **`Ptr` - pointer cells for C out-parameters.** `Ptr.cell()`/`read()`/`write()`/
  `free()` give you a heap slot to pass where C wants a `T**` (e.g.
  `sqlite3_open(path, sqlite3** out)`) and read the handle back - the piece that
  makes opaque-handle APIs usable.
- **By-value FFI structs + typed `cstr`** (raw `char*`), and `ptr` is a first-class
  type consistent across the checker and codegen.
- **Dogfooded against SQLite:** open a DB, create a table, insert rows, and query
  them back through prepared statements - all from Wyn (see the C-FFI guide).
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
  alias. (Previously `print` was inconsistent - no newline for strings/ints, a
  newline for arrays, and a silently-dropped second argument.)
- **Namespaced imports (W9):** `import m` exposes members as `m.foo()` - the
  canonical, collision-free form. A bare `foo()` after `import m` is now an error
  (selective `import { foo } from m` keeps the flat call).
- **`?.` optional chaining** - `x?.field` / `x?.method()` short-circuit on `None`.
- **`zip()` + pair destructuring** - `for x, y in zip(a, b)`.
- **Optional struct fields** - a `Struct?` (or `int?`/`string?`/…) field now
  compiles and matches correctly.
- **Closure copy** - `var g = f` where `f` is a closure.
- **Braceless single-statement bodies** for `for`/`if`/`else`/`while`.
- **`_` as a throwaway for-loop variable.**
- Variables may be named after C keywords (`int`, `long`, `char`, …).

### Removed / changed (breaking)

- **`|>` pipe operator removed** - compose with nested calls or intermediate
  `var`s.
- **`&&` / `||` / `!` removed** - use `and` / `or` / `not` (already canonical).
- **`try` / `catch` / `throw` removed** - use `Result` + `Ok`/`Err` + `match` + `?`.
- **`import m` requires `m.foo()`** - a flat `foo()` no longer resolves.
- **The `pkg.wynlang.com` registry is gone** - `wyn pkg register/login/whoami/
  search/publish/push` are removed; use git URLs (`wyn add <url>`) and publish by
  pushing a repo + tag.

### Fixes & internals

- Memory-safety: `print()` on an int expression, a module-load use-after-free, and
  a JSON out-of-bounds read - all fixed and ASan-verified.
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
  (defaulted to int) and read back garbage - the three map-store paths now share
  one type selector, so store and load agree.
- FFI: variables/params of type `ptr` and `cstr` lower correctly; `string` coerces
  to a `ptr` parameter; imported C-package `extern fn`s link; an `extern fn` for a
  standard-header symbol (`strlen`, math functions, …) no longer conflicts.
- Compiler cleanup: dead code removed (`scope.c`, the old registry/semver client),
  runtime source lists de-duplicated; warning-clean.

## v1.16.0 - "The Batteries Release" (2026-07-13)

Batteries included. This release makes Wyn feel more like Python where it counts
and - the big one - turns on a real C FFI, so the entire C ecosystem is now
reachable from Wyn. All backward compatible; no source changes required.

### C interop (FFI)

- **`extern fn` actually works now, with real types and library linking.** You can
  declare a C function and call it directly. Scalar types (`int`, `float`, `bool`),
  `string`, and `void` all map correctly (previously only `int`/`string` worked and
  everything else silently miscompiled).

  ```wyn
  extern fn sqrt(x: float) -> float;

  fn main() {
      println("${sqrt(16.0)}")     // 4.0 - links the C math library
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

- **`if let`** conditional binding - match one case inline without a full `match`:

  ```wyn
  if let Some(v) = find(key) {
      println("found ${v}")
  } else {
      println("missing")
  }
  ```

  Works on any `Option` or `Result`, with or without an `else`.

## v1.15.0 - "The Payloads Release" (2026-07-13)

Two correctness fixes that remove long-standing sharp edges: any scalar can now
ride inside an `Option`/`Result`, and your function names no longer collide with
the C standard library.

### Optionals & Results

- **`float?` / `bool?` and `Result<float, _>` / `Result<bool, _>` now compile.**
  Previously only `int` and `string` payloads worked - a `-> float?` function
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
  follows non-static declaration`). Those names are now free to use - the
  generated C is transparently namespaced, so your Wyn code is unaffected.

  ```wyn
  fn connect(host: string) -> string {
      return "connected to ${host}"
  }
  ```

## v1.14.0 - "The Polish Release" (2026-07-11)

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

- **`println(array)` works** - arrays and `map.keys()` print their real values
  with a newline (was a type error; `print(arr)` already worked).
- **String negative indexing** - `s[-1]` is the last character (matches arrays).

### Functions

- **Default args infer their type.** `fn greet(who = "world")` works - the
  parameter type comes from the default value's literal, so you don't have to
  write `who: string = "world"`. Typed defaults are unchanged.

## v1.13.0 - "The Pythonic Release" (2026-07-11)

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
- **`Some`/`None`/`Ok`/`Err` work for any payload** in any context - you never
  need to write a mangled `OptionString_Some(...)` form again.

### Correctness (codegen type selection)

- **HashMap values are correctly typed.** `m[k]` and `m.get(k)` on int/float/bool
  maps returned garbage (always used the string getter) - even the shipped
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
- **`elif` no longer hangs the compiler** - it errors with "use `else if`". A
  general no-progress guard now protects every statement-parsing loop from hangs.

### Memory safety

- **Use-after-free fixes** for a local string moved into an array, a struct
  field, an `Option`/`Result`, or an enum variant, then released at scope exit
  while the container still aliased it. All ASan-verified with regression tests.

## v1.12.0 - "The Hardening Release" (2026-07-09)

Memory-safety fixes, deeper concurrency, and a warning-clean build. Three of the
fixes below are genuine bugs that shipped in v1.11.0 - upgrading is recommended.
All memory fixes were verified under AddressSanitizer.

### Memory safety

- **String concat use-after-free fixed.** `a + b` where the result was reused as a
  later operand (`j = a+b; m = j+c`) could corrupt the live string and double-free
  it. `wyn_string_concat_safe` now always returns a distinct allocation.
- **print/println double-free fixed.** Printing a fresh interpolated/temp string
  (`println("x=${x}")`) could release it twice.
- **Database/JSON query heap overflow fixed.** `Db.query` / `Db.query_params` /
  `Json.keys` wrote unbounded results into fixed 64KB/4KB buffers - any larger
  result overflowed the heap. They now grow as needed (verified on a ~168KB result).
- **String interpolation no longer truncates at 512 bytes.** Interpolated strings of
  any length now round-trip intact (previously silently cut to 511 bytes).
- **Reference-count heap detection hardened** with a dual sentinel, and two compiler
  buffers made bounds-safe against overly long identifiers.

### Concurrency

- **Cooperative `Time::sleep`.** Inside the coroutine scheduler (fire-and-forget
  `spawn`), sleeping now yields to other tasks instead of blocking the worker
  thread. In one benchmark, 50 concurrent 200ms sleeps finished in ~0.21s instead of
  ~1.03s. (Awaited/`parallel` spawns run on the thread pool and still block - see
  the roadmap.)

### Build & tooling

- **Warning-clean build** under `-Wall -Wextra`.
- **Runtime library rebuild fixed** - `make` now rebuilds `libwyn_rt.a` when a
  runtime source or header changes, so programs no longer link a stale runtime.

## v1.11.0 - "The Developer Experience Release" (2026-03-30)

Make developers productive in their first hour. Every change improves error messages, tooling, or language ergonomics.

### Language Features

- **`enum.to_string()`** returns variant name: `Color.Red.to_string()` → `"Red"` (was `"0"`)
- **`for i, v in arr`** indexed iteration: `for i, v in ["a","b","c"] { println(i.to_string() + ":" + v) }`
- **`int?` optional syntax**: `var x: int? = OptionInt_Some(42)` - sugar for `OptionInt`
- **`"ha" * 3` string repeat**: `"-" * 40` produces a 40-character separator line

### Compiler Improvements

- **Missing return is now an error**, not a warning: `fn get() -> int { }` fails to compile
- **Type mismatch errors from Wyn**, not C: `var x: int = "hello"` shows clear Wyn error with line number and suggestion
- **Typed arrays**: `var arr: [int] = []` uses `WynIntArray` (raw `long long*`) - sum 1M ints 2x faster

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

## v1.10.0 - "The Quality Release" (2026-03-28)

No new language features. Every change is about making the existing language faster, safer, and more stable.

### Performance

- **fib(35)**: 120ms → 33ms (3.6x faster via -O2 release builds)
- **1M string append**: 12,143ms → 6ms (2,024x faster - RC header caches length + capacity)
- **1M `.len()`**: ~100ms → 1ms (O(1) from RC length cache, was O(n) strlen)
- **Array sort**: 189ms → 1ms for 10K ints (O(n²) bubble sort → O(n log n) inline introsort)
- **`wyn build`**: ~4s → 411ms (precompiled `libwyn_rt.a` + system cc)
- **Binary size**: 425KB → 49KB (dead code stripping)
- **Spawn overhead**: ~20μs → 6μs per task
- **4x parallel fib(35)**: perfect 4x scaling (33ms, same as sequential)
- **HashMap**: 17x insert, 23x get (FNV-1a + 4096 buckets)

### Memory Safety - Zero Leaks

Every known memory leak pattern is fixed. Verified with AddressSanitizer + UndefinedBehaviorSanitizer.

- **50+ string functions**: raw `malloc` → RC-tracked `wyn_str_alloc`
- **String concat temps**: left and right temporaries released after concat
- **Chained concat**: `"a" + x + "b"` - all intermediates released
- **String interpolation**: `"item ${i}"` - interpolation temps released
- **String reassignment**: `s = "new" + x` - old value released, ownership transfer
- **Function argument temps**: `consume("a" + x)` - arg released after call
- **println/print temps**: `println(x.to_string())` - temp released after print
- **Method chain intermediates**: `"hello".upper().trim()` - upper() result released
- **Method object temps**: `("a"+x).split(",")` - concat result released
- **Unused return values**: `some_fn()` returning string - released
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

Generators, Debugger, 36 Packages - all roadmap phases complete.
