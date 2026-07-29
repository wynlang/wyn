# Changelog

## v1.20.0 (2026-07-28) - "The Concurrency & Correctness Release"

> **Re-released 2026-07-29.** The v1.20.0 artifacts were rebuilt and replaced in
> place after an adversarial review of this release found defects serious enough
> that shipping them was not defensible - including a license-compliance
> violation in every binary, a scheduler bug that burned a full CPU core in
> *every* concurrent program, and a silent data race that produced wrong answers
> at exit 0. Nothing had been downloaded, so the artifacts were replaced rather
> than superseded by a v1.20.1. The full list is in "Fixed in the re-release"
> below. If you somehow obtained the original artifacts, replace them.

The headline: **concurrency that's fast out of the box**. Fire-and-forget
`spawn` now dispatches 1,000,000 tasks in ~0.67s (Go goroutines: ~0.32s on the
same machine), a channel-backpressure livelock that hung real programs is gone,
and awaited `spawn`/`await_all`/`parallel { }` now run cooperatively on the
coroutine scheduler by default (8 awaited 100ms sleeps overlap into ~112ms, not
~800ms) - no thread-pool tuning, no async ceremony. Concurrent programs now also
idle at **~0 CPU** rather than burning a core (see the re-release notes).

Alongside that: **experimental GPU dispatch** for `[float].map` (Metal on
macOS, OpenCL on Linux/Windows, opt-in), **nested & recursive data**
(recursive enums, arrays of structs, generics over non-scalar types), a broad
memory-safety sweep (a string use-after-free, a `repeat` heap-corruption, a
run-queue data race, `Shared`/channel guards - all found and fixed with
ASan/TSan and locked behind regression tests, suite 210 -> 237), plus real
crypto, AWS from pure Wyn, and removed compiler limits.

What CI actually gates, so you can judge the rest: every push and PR builds on
Linux, macOS-arm64, macOS-x64 and Windows, and on the three Unix platforms runs
`make test` - the 248-file expect/regression suite, the golden-C codegen
snapshots, and 38 dedicated behavioral/negative sub-suites - plus the same suite
re-run on the coroutine async substrate, plus ASan and TSan runtime jobs. Fixes
listed below carry a regression test in that suite. The `tests/stdlib` suite (68
files) is gated through an allowlist (`tests/stdlib/known_failures.txt`) that is
now **empty**: the debt was paid down to zero during this cycle, so all 68 files
are hard gates and any regression fails the build. One honest caveat remains:
Windows runs a declared portable subset rather than `make test`, because the
exit-code- and signal-dependent suites are POSIX-only; the list of what is and
is not covered there is in `.github/workflows/ci.yml`.

The v1.20.0 tag originally shipped this paragraph as "Everything below is
verified: 0-warning build on 4 platforms, full suite + golden-C snapshots
green". That was false: CI ran only `run_bdd.sh` at the time, so `make test`
was never gated, and `tests/errors/run_channel_deadlock_test.sh` was failing
deterministically at the tag. `make test` is now a required check (see
`.github/workflows/ci.yml`) precisely so that claim cannot be made again
without being true.

### Fixed in the re-release

An adversarial review of v1.20.0 - deliberately reading it the way a hostile
reader on a link-aggregator front page would - found the following. Every item
was reproduced against a clean build of the original `v1.20.0` tag before being
fixed, and re-verified after.

**Legal**

- The shipped binary statically links TinyCC (`libtcc`), which is **LGPL-2.1**,
  while `LICENSE` was MIT-only and `vendor/` contained **no license text at
  all**. That made every published artifact non-compliant. Added the full
  LGPL-2.1 text (`vendor/tcc/COPYING`), minicoro's license (it was also
  undocumented), a root `THIRD-PARTY-NOTICES.md` inventorying every vendored
  component, and a third-party section in `LICENSE`. The release workflow now
  **fails** if the notices are missing from an artifact, so this cannot silently
  regress. Note LGPL-2.1 §6 also requires choosing dynamic linking or conveying
  object code with relink terms; that decision is open and documented in
  `THIRD-PARTY-NOTICES.md` rather than glossed over.

**Correctness - wrong answers at exit code 0**

- **Data race on shared mutable globals.** A global written from a `spawn`ed
  function tore: 4 tasks x 2000 increments produced 7956 instead of 8000,
  silently, exit 0. Now a **compile-time error** naming the global and the
  function, pointing at `Shared`. (Found a real instance of this in our own
  published REST-API blog post, where two concurrent requests could be assigned
  the same ID.)
- **`.format()` was a silent no-op** - `"Hello %s".format("World")` printed
  `Hello %s`. Now implemented with `{}` placeholders; printf-style specs are a
  hard compile error rather than silently ignored.
- **`input_line()` returned a pointer as an integer** (e.g. `4339582744`). It was
  registered in a blanket int-typed builtin loop. Also fixed: it returned a
  `static` buffer, so two calls aliased, and lines over 1023 bytes were silently
  truncated. `input_float()` had the same typing defect.
- **Floats did not round-trip.** Printing at `%.15g` meant the printed text
  parsed back to a *different* double: `0.1 + 0.2` printed `0.3` while
  `0.1 + 0.2 == 0.3` was `false`, and `1e16 + 2.0` lost its low bits through any
  string round-trip (JSON, CSV, logs). Now shortest-round-trip: the fewest digits
  that parse back to the identical value. The `--release` slim runtime was worse
  (6 digits) and is now identical.
- **`array_push(arr, <fresh string>)` was a use-after-free** - the string was
  released after being handed to the array, so elements read back empty.
- **Annotated `[int]` arrays selected a different C representation than inferred
  ones**, so `var a: [int]` then `a.sort()` failed codegen after `wyn check`
  passed - and `a[0] = 9` *compiled* while writing a 16-byte value into an 8-byte
  slot, printing `1` and corrupting memory.
- **`HashMap<K,V>` annotations with a space** (`HashMap<string, int>`) failed to
  parse with a misleading "unclosed block" error.
- **`?` followed by `return` on one line** misparsed as a ternary.

**Resource exhaustion**

- **Every program that spawned a task burned ~1 CPU core forever**, including
  while completely idle. Three separate busy-spin sites: idle workers polled on a
  100us timeout instead of parking; the fire-and-forget drain loop spun with no
  sleep at all (2.8 CPU-seconds per 2s wall, across multiple cores); and
  `future_get` yielded in a tight loop. All three now measure **~0** idle CPU, with
  the cooperative-await latency that this release is named for fully preserved
  (verified: 8x100ms -> 115ms, 1M spawns 0.64-0.69s, TSan clean on both
  executors). A CI gate now asserts idle CPU stays under 50ms.
- **The HTTP server never closed connections.** `Http_respond` deferred the close
  to the handler's *next* `read_request`, which never comes in the one-shot
  handler shape every example used - so the response advertised
  `Connection: close` and then never sent FIN, wedging any client that waits for
  EOF (including `ab` at concurrency 1) and leaking an fd per request.
  `Http_close_client` was advertised by the checker with **no implementation**.
- **`wyn run` orphaned its child.** Killing `wyn` left the program running.
  Signals are now forwarded, and the child dies with the parent even on
  `SIGKILL` (`PR_SET_PDEATHSIG` on Linux, a pipe-EOF supervisor on macOS/BSD, a
  job object on Windows).
- **`stdout` was never flushed** when not a tty, so a server's startup line never
  appeared in redirected output.

**Supply chain**

- **`wyn.lock` did not pin anything.** Install reconstructed the spec from
  url+ref and **never compared the recorded commit sha**, so a moved branch or
  retagged release silently yielded different code than the lockfile recorded.
  Now enforced, preferring a checkout of the recorded commit, with a clear error
  and legacy-lockfile handling.
- **`install.sh` / `install.ps1` had no integrity checking of any kind** - they
  fetched from the mutable `releases/latest` pointer and piped straight into
  `tar`. Releases now publish `SHA256SUMS`; both installers verify and **fail
  closed**. The release canary verifies the digest and proves tamper-detection by
  corrupting a byte. (Signing the manifest is a separate, still-open step.)

**Testing - the root cause behind most of the above**

- **CI never ran `make test`.** It ran only `run_bdd.sh`, so ~80 test files were
  ungated on every PR: all of `tests/errors/`, the golden-C snapshots, GPU,
  bindgen, cpkg, sqlite - and `tests/stdlib` (68 files) was run by *nothing at
  all*. `tests/errors/run_channel_deadlock_test.sh` was failing **deterministically
  at the v1.20.0 tag** while the release notes claimed the full suite was green.
  `make test` is now a required check on all three Unix platforms, and it drives
  50 suites.
- `tests/stdlib` went from **17 known failures to 0** and is now a hard gate. Two
  of those were real compiler bugs (above); most of the rest were tests asserting
  behavior this release deliberately changed, plus five that depended on a live
  `httpbin.org` and are now hermetic.
- `tests/run_tests_parallel.sh`, which CI *did* run, reads a `tests/test_list.txt`
  that is not in the tree - it executed **zero tests** and reported success. Its
  green tick was decoration.
- `channel(0)` is now rejected at **check** time rather than panicking at runtime.

**Honesty of published numbers**

- Roughly 25 false or unsupportable performance claims were corrected across ~40
  files. The pattern: nearly every *multiplier* was fabricated or traced to a
  vanished baseline, while most *absolute* Wyn measurements were about right.
  Worst offenders: "1M string appends in 6ms, 33x faster than Python" (actually
  ~11.7s, and ~1.25x **slower** - naive concat is O(n^2) in both languages);
  "Wyn compiles 7-15x faster than Rust" (cargo builds hello world in ~0.4s, so
  the table was deleted); a GPU "~2-5x" speedup that **reverses** under
  end-to-end measurement; ~10 files citing hardware the project has never run
  on. The web throughput figure was real but measured against a *looping* handler
  while the site published a *one-shot* snippet that cannot do keep-alive - the
  snippet was fixed and the number re-derived with a committed harness
  (`benchmarks/http_load.sh`).
- Release artifacts are now built with `-O2` and stripped. Every prior release
  shipped an unoptimized, unstripped compiler; the binary is **35% smaller**
  (1,349,552 -> 871,584 bytes). Warm compile time is unchanged, so this is a
  download-size win only.
- The GitHub Release body is now scoped to the released version's section instead
  of republishing all 1,082 lines of this file.

**Found by the newly-gated `make test`, on platforms the dev box is not**

Wiring `make test` into CI immediately paid for itself: these were caught before
the re-release shipped, and none of them reproduce on macOS-arm64.

- **`Db.` programs failed to link on Linux.** `-lsqlite3` was placed *before* the
  objects on the link line, and GNU ld only resolves a `-l` against objects
  listed earlier - so every `Db.*` call became an undefined reference (18 of them
  in one test). Apple's linker ignores `-l` order, which is why this was
  invisible on macOS while breaking every Linux user compiling against a system
  libsqlite3. Moved to the end of the link line, where the FFI flags beside it
  already were, and covered by a new gate
  (`tests/errors/run_sqlite_link_order_test.sh`) that exercises the builtin `Db.`
  path - the existing sqlite test covers `import sqlite3` (FFI) and passed
  throughout.
- **Awaited-drain spin was bounded by iterations, not time.** `wyn_spawn_wait`
  spun a fixed 2048 rounds before parking, which costs whatever the host makes it
  cost: ~13ms on an M3 Pro but ~91ms on a slower x86 runner, over the 50ms
  idle-CPU budget. It now also carries a 20ms wall-clock budget, so the CPU burned
  before parking is bounded in *time* on every platform. Measured on a throttled
  0.5-CPU container, same program: 117ms -> 10ms, with no change to the 1M-spawn,
  awaited-sleep, or timer-chain benchmarks.
- **Build failures were reported without the error.** Two separate defects: the
  stdlib runner truncated output such that only "Build failed" and the "Compiler
  output:" *heading* survived, and `wyn build` wrote the C compiler's stderr to a
  fixed `/tmp` path, so parallel builds overwrote each other and a failure could
  be reported with a *sibling's* error text - naming a file you were not
  building. The error file is now per-process, and Windows captures compiler
  stderr too (it was the last platform still discarding it).

Correctness foundation (compiler limits removed, lambdas fully typed, real
crypto, AWS from pure Wyn, and a snapshot suite guarding the generated C):

- **Out-of-bounds is fatal by default** (breaking): `a[10]` on a 3-element
  array (and `s[10]` on a 3-char string) now panics and exits 1 instead of
  printing a warning and continuing with `0`/`""`. A memory-safe language
  must not substitute a value and keep running after an OOB read (Go
  panics, Rust panics, Python raises IndexError). The same fatal-by-default
  posture now applies to `Math.checked_add/sub/mul` overflow. Set
  `WYN_LENIENT=1` to restore the old print-and-continue behavior;
  `WYN_STRICT` is accepted as a no-op alias of the new default.
- **`.to_int()` / `.to_float()` reject garbage and overflow** (breaking):
  `"abc".to_int()` and `"99999999999999999999".to_int()` returned a silent
  `0` - indistinguishable from parsing `"0"`. Both now panic with the
  offending value in the message (Python raises ValueError, Go strconv
  returns an error). Trailing junk (`"12x"`) and empty strings also panic;
  trailing whitespace is tolerated. `int("...")` / `float("...")` go
  through the same checked parse. `WYN_LENIENT=1` restores silent zeros.
- **Fixed use-after-free reading a map string value across an overwrite**
  (memory safety, ASan-verified): `v = m["a"]; m["a"] = "new"; print(v)`
  read freed memory - the insert freed the old value even though a live
  read had escaped. Overwritten and removed string values now stay alive
  until the map itself is freed, matching the array element-overwrite
  discipline.
- **Concurrent array mutation panics instead of corrupting the heap**:
  two awaited `spawn`s pushing to the same plain array raced the realloc
  and died with malloc heap corruption (SIGABRT). Mutating pushes now
  carry a one-relaxed-atomic-exchange write flag (the Go map race detector
  approach) and panic with "concurrent array mutation detected - use a
  channel or Shared to coordinate writers". Note: plain variables are NOT
  synchronized across awaited spawns (racing `total = total + 1` loses
  updates) - use the atomic `Shared` type (`Shared.new/get/set/add`) or a
  channel for cross-task counters.
- **Floats always print with a decimal point**: `print(1.0)` printed `1`,
  indistinguishable from an int (Python, the advertised mental model,
  prints `1.0`). `print`, `println`, `${x}` interpolation, `.to_string()`,
  `str()`, and array rendering all agree: integral floats get `.0`,
  fractional and scientific forms are unchanged.
- **Scientific-notation float literals parse**: `1.0e10`, `1e-6`, `2E+8`
  are now valid literals. Wyn already printed large floats as `1e+10` but
  could not read its own output back.
- **StringBuilder pool is growable**: the fixed cap of 32 live builders
  made the 33rd `StringBuilder.new()` silently return a dead handle that
  dropped every append. The pool now grows on demand (same
  realloc-doubling pattern as the compiler registries); freed slots are
  reused.
- **Stack overflow is diagnosed by name**: 1M-deep recursion died with a
  silent SIGILL/SIGSEGV (exit 132, no output). The crash handler's
  alternate signal stack was below MINSIGSTKSZ, so it could never run for
  a stack fault; it now installs correctly and faults near the stack guard
  page report "panic: stack overflow (recursion too deep?)" with a hint,
  like Rust.

- **`pub` visibility is now enforced** (breaking): calling a module
  function that is not marked `pub` (or `export`) from outside its module
  is a check-time error. Previously the keyword was accepted but ignored -
  every module function was callable from anywhere. The error tells you
  the exact fix:
  `function 'whisper' in module 'dep' is private` /
  `Help: Add 'pub' to 'fn whisper' in dep.wyn to export it.`
  Enforced for dot calls (`m.f()`), qualified calls (`m::f()`), aliased
  imports (`import m as x`), and selective imports
  (`import { f } from m`). Same-module calls, main-file functions,
  stdlib namespaces (Math, File, ...), and C packages are unaffected.
  Struct and enum `pub` markers are still parsed but not yet enforced
  (types are not namespaced per module yet).
- **Week-one stdlib batch**: the methods a developer reaches for in their
  first week, designed by precedent (Python/Go/Rust/Kotlin), all
  monomorphized (no boxing).
  - `xs.sort_by((x) => key)` - sort structs (or anything) by a key lambda
    (Python `sorted(key=)`, Kotlin `sortedBy`). Int, float, and string keys.
    The legacy two-arg comparator form still works.
  - `xs.max_by(f)` / `xs.min_by(f)` - element with the largest/smallest key
    (Kotlin `maxBy`, Rust `max_by_key`).
  - `xs.group_by(f)` - map of key to `[elements]` (Kotlin `groupBy`);
    string and int keys; buckets read back with `m[k]`, `m.get(k)`, and
    `for k, vs in m`.
  - `xs.sorted()` and `sorted(xs)` - non-mutating sorted copy (Python
    `sorted`); `.sort()` still mutates in place.
  - `xs.flatten()` - `[[T]]` to `[T]`, one level (Kotlin `flatten`).
  - `File.read_lines(path)` - returns `[string]` (Python `readlines`);
    keeps interior blank lines, strips `\n`/`\r\n`, no trailing empty
    entry.
  - `str(x)` - uniform to-string builtin (Python `str`), joining
    `len()`/`int()`; `float(x)` added alongside. `int("42")`, `int(3.9)`,
    `float("1.5")`, `float(2)` all work.
  - `m.get(key, default)` - key-or-default lookup (Python `dict.get`);
    previously compiled but returned garbage.
  - `.sort()` on `[float]` now compares as doubles - negative floats were
    ordered wrong (IEEE bit-pattern comparison).
- **Real SHA-256 and HMAC-SHA256**: `Crypto.sha256` previously used a
  non-standard stand-in hash and `Crypto.hmac_sha256` shelled out to
  openssl with the secret key on the command line. Both are now real,
  in-process implementations (FIPS 180-4 / RFC 2104), validated against
  the RFC 4231 test vectors. New `Crypto.hmac_sha256_hex(hex_key, data)`
  enables chained signing (AWS SigV4). Also fixed heap overflows on
  strings returned by Crypto, `File.temp_file`, and
  `Process.exec_capture` when used in concat chains.
- **AWS from pure Wyn**: `demos/aws/` has a working SigV4 signer plus
  STS/S3/EC2 demos - call AWS APIs with zero dependencies.
- **Type-correctness batch**: `print(x.to_float())` no longer segfaults;
  bools stored in vars or interpolated print `true`/`false` (was `1`/`0`);
  variables named after C keywords (`long`, `short`) compile; arrays
  interpolate in strings (`"${nums}"`); a loop variable name can be
  re-bound after the loop; cross-type maps (`ints.map((n) => n.to_float())`)
  return correct values; single-letter struct names work as lambda params.
- **`print` is the canonical output function** in all docs, templates, and
  examples (`println` still works as an alias).
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

### Nested & aggregate values

- **Recursive enums**: enum payloads may reference the enum itself
  (`enum Expr { Num(int), Add(Expr, Expr) }`) - expression trees and linked
  lists work; payloads are heap-boxed automatically. Mutually-recursive
  enums are a clean check error (not yet supported), not leaked C.
- **Dynamic nested arrays**: `[[int]]`, arrays of structs (`[Point]`),
  push/index/iterate all work. (Maps with struct *values* — `{string: Point}`
  — type-check but do not yet codegen; see Known limitations.)
- **Generics over non-scalar T**: generic structs and functions instantiate
  with array and struct type arguments (`Box<[int]>`, `Box<Point>`), including
  multiple instantiations and multi-type-param generics.
- **Enum equality**: `==`/`!=` on enum values, and enums as struct fields
  (including in struct equality).
- **`Result<Struct, E>` for any error type**: `Result<User, string>`,
  `Result<User, ErrCode>`, struct errors, int errors - with `is_ok`,
  `unwrap`, `unwrap_err`, and `?` propagation.
- Unsupported nested cases (closure-typed struct fields, generic enums,
  `Box<Box<T>>`) fail with clean "not yet supported" errors instead of
  emitting broken C.

### Experimental: GPU dispatch for [float].map (opt-in)

- With `[gpu] enabled = true` + `float32 = true` in wyn.toml, large
  `[float].map` calls auto-dispatch to the GPU: **Metal on macOS, OpenCL on
  Linux/Windows** (loaded at run time - no SDK needed to build, binaries
  still run on machines with no GPU via the always-correct CPU fallback).
- Gated by build TARGET (cross-compiling from a Mac to Linux wires the
  OpenCL backend in), thread-safe, `WYN_GPU=0` kill-switch.
- Honest status: re-measured end-to-end on an Apple M3 Pro, the GPU path did
  **not** beat the CPU path (10M-element `[float].map`: ~197ms with `[gpu]`
  enabled vs ~134ms CPU-only; 50M: ~679ms vs ~584ms). First dispatch pays a
  one-time kernel JIT and a runtime cost model keeps small workloads on the
  CPU, but no reproducible speedup has been demonstrated on this hardware. This
  is a spike, not a performance feature - which is why it is opt-in, off by
  default, and float32-only.

### Concurrency: much faster, and correct (2026-07-27 batch)

- **Fire-and-forget `spawn` is much faster**: 1,000,000 spawns now take ~0.67s
  (Go goroutines: ~0.25s on the same machine). The coroutine stack pool was
  re-`mmap`-ing an 8MB region on every reuse (a syscall + ~2000 page faults
  per spawn); it now reuses the pooled stack directly. Also added Go's `wakep`
  rule (skip the wake syscall when a worker is already spinning) (#189).
- **`Task.recv` collection no longer livelocks**: a program with more concurrent
  senders than channel capacity (e.g. 10k senders on a 4k channel) span at 900%
  CPU forever - blocked senders busy-yielded and were re-enqueued. Blocked
  senders/receivers now park on a per-channel waiter list (zero CPU while
  blocked); the 10k-task benchmark went from hanging to ~0.09s (#190).
- **`Task.try_recv(ch)`** is now callable from Wyn and returns `int?`
  (`Some(value)` / `none`) - a non-blocking receive that disambiguates a real
  `0` from an empty channel (#190).
- **`await_all` and `parallel { }` preserve the result type**: results were
  hardcoded to `[int]`, so string/float/struct results miscompiled (and a
  `parallel` float join printed garbage like `2.1e-314`). All element types now
  flow through correctly (#191).
- **Awaited concurrency is cooperative by default**: awaited `spawn`/`await_all`/
  `parallel` run on the coroutine scheduler (not the thread pool), so cooperative
  `Time::sleep` and I/O overlap - 8 awaited 100ms sleeps finish in ~112ms, not
  ~800ms. (`WYN_ASYNC_POOL=1` selects the old blocking pool if needed.) (#196)
- **`Shared.*` handles are bounds-checked**: `Shared.set(-1, x)` and slab
  exhaustion were silent out-of-bounds reads/writes; now a clean panic, and the
  pool is much larger (#193, ASan-verified).
- **`Task.channel(0)` fails cleanly** instead of hanging forever (unbuffered
  channels aren't supported yet - it now says so) (#193).
- **Data race fixed** on the internal run-queue link field (`Task::next` is now
  atomic) - TSan-clean on the channel wake path (#195).

### Enums & generics

- **Bare (unqualified) enum constructors work**: `Circle(5)` for
  `enum Shape { Circle(int) }` (previously only `Shape.Circle(5)` compiled), and
  a dataless variant of a data enum (`enum S { A(int), B }` → `var x = B`).
  Rule: a real function of the same name wins; the qualified form is always
  available to disambiguate (#192).
- **Generics returning generics**: `fn wrap<T>(x: T) -> Box<T>`,
  multi-parameter `fn mk<A, B>(...) -> Pair<A, B>`, and nested generic literals
  (`Box { val: Box { val: 42 } }`) now compile (#194). Generic *enums*
  (`enum Opt<T>`) give a clean "not yet supported" error instead of leaking C.

### Soundness & memory safety

- **Cross-type comparison segfault**: `int == "5"` type-checked and then
  crashed at runtime - now a clean check error (#183).
- **Use-after-free pushing a block-scoped array into an outer array**:
  silent wrong answers from freed memory; pushes now deep-copy
  (ASan-verified) (#185).
- **Checker soundness gate K1-K11**: a batch of holes where the checker
  accepted programs codegen couldn't compile (#177).
- **User function named `len`** (or another builtin) no longer silently
  shadowed by the builtin lowering - your function is called (#185).
- **Generic `[T]` no longer assumes int elements** (#185).
- **Deep nesting** (statements, array literals) and empty `0x`/`0b`
  literals: clean errors instead of parser stack overflow / ICE (#179, #185).
- **Missing HashMap key panics with the key name** instead of silently
  returning 0 (#183). Struct field typos are check-time errors (#183).
- **Channel-deadlock backstop**: recv on a channel every sender has
  abandoned panics instead of hanging forever (#178).

### Type inference & DX

- **Word-count / cache patterns work**: reading from an empty `{}` map no
  longer poisons its value type to string - `counts[w] = counts[w] + 1`
  infers int, and `return cache[key]` in an int function types the map from
  the return type (final-sweep fix).
- **`m.contains(k).to_int()`** and other bool-int bridges compile (#final).
- **Semicolons as statement separators**: `a = 1; b = 2` on one line (#179).
- **`x: int = 5`** annotated bare assignment (#179).
- **`HashMap::new()` then `m.set("a", 42)`** infers int values (was: string
  default that decoded 42 as garbage) (#179).
- **Option<T> struct fields** (was an ICE) (#179).
- **`Json.get_string` links; `Json.keys` returns `[string]`** (#180, #183).
- **`${Math.sqrt(2.0)}`** and other float builtins in interpolation no
  longer truncate to int (#183).
- **StringBuilder `to_string`** works in both `::new()` and `.new()` forms
  (#183).
- **`int_to_string` imported across modules** no longer mis-namespaced (#180).
- **Argument-count errors carry file:line; panics no longer cite the
  generated `.wyn.c`** - errors point at your code (#179).
- **`return f(...)` where `f` returns void** compiles (the iOS-shim
  pattern) (#167).

### Tooling, CI, portability

- **`wyn doctor`** no longer reports "All good" when it isn't; a stray
  `./VERSION` file in your project no longer hijacks `wyn version` (#183).
- **Test runners are storm-proof and watchdog-guarded**: bounded
  parallelism (was: a fork-storm that starved CI runners), per-test
  wall-clock timeout + CPU rlimit (a looping test can no longer exhaust the
  host) (#180, final sweep).
- **glibc/gcc + linux-arm64 portability**: builds clean under gcc/glibc;
  runtime libs published for linux-arm64 (#178, #183).
- **Release artifacts**: bundle vendor/tcc so `wyn run` works out of the
  box; canonical Windows layout + install canary (#179).

### Benchmarks (Apple M3 Pro, 15-run warm median; Go 1.26, Python 3.14)

| Benchmark | Wyn 1.20 | Go | Python |
|---|---|---|---|
| fib(35) | 41 ms | 48 ms | 958 ms |
| 1M `StringBuilder` appends | 14 ms | - | 85 ms |
| 100K `.upper().trim()` chains | 18 ms | - | 56 ms |
| Sort 1M ints | 124 ms | 101 ms | 591 ms |
| Startup (hello world) | 7.3 ms | 8.7 ms | 42 ms |
| Hello binary | 50 KB (51,400 B) | 2,492 KB | - |
| `wyn build` hello (dev) | 288 ms | 96 ms (`go build`) | - |
| Edit-run loop (`wyn run`, already built) | 30 ms | - | - |
| 1M fire-and-forget spawns | 0.67 s | 0.25 s | - |
| 8 awaited 100 ms sleeps | 112 ms | - | - |
| `web` pkg, keep-alive, c=200 | ~22,000 req/s | - | - |

Wall-clock medians of the whole process, so the ~7 ms startup floor is included
in every Wyn row.

Honest notes:
- Naive string concatenation is the one place Wyn loses outright: `s = s + "x"`
  1M times takes ~11.7 s versus Python's ~10.2 s (both are O(n²); Wyn's
  per-copy constant is just worse). Use `StringBuilder` - 1M appends in 14 ms.
- Fire-and-forget spawn at the 1M scale is now ~2.7x behind goroutines
  (0.67 s vs 0.25 s) rather than ~35x, but Go is still far denser on memory
  (~7 MB vs ~71 MB RSS for 1M outstanding tasks).
- The GPU `[float].map` spike did **not** beat the CPU path end-to-end in
  re-measurement on this machine (10M elements: ~197 ms GPU-enabled vs ~134 ms
  CPU-only). Treat it as a spike, not a speedup. The runtime cost model keeps
  small workloads on the CPU.

### Known limitations (documented, not hidden)

- Recursive-enum payloads currently leak (freeing shared boxed subtrees
  needs move/RC analysis - on the roadmap).
- User-declared generic enums (`enum Opt<T>`) are a clean check error, not
  yet supported (generic structs work; a single C enum can't hold two payload
  types across instantiations - needs monomorphization machinery).
- A few constructs type-check but don't yet codegen (they fail at build with a
  C-compiler error rather than running, so they never ship a broken binary):
  maps with struct/array *values* (`{string: Point}`), `${struct}`/`${Option}`
  string interpolation, `Result<Struct, CustomEnum>`, forward-referenced struct
  field types, and escaping/nested closures. On the 1.21 fix list.
- GPU: float32 precision; verified on Apple Silicon + NVIDIA (T4, A10G);
  AMD untested (driver provisioning, not Wyn); Windows GPU path untested
  (CPU fallback verified via CI).

### Memory safety & robustness (final hardening batch)

- **String use-after-free on aliased returns fixed**: `var b = f(a)` where `f`
  returns its argument (a borrow) double-released one buffer - now the callee
  retains borrowed returns (ARC "+1 out"). ASan-clean, RSS-flat both directions.
  Backed by a mapped RC ownership model (`docs/RC_OWNERSHIP_MODEL.md`).
- **Heap corruption in `str.repeat(negative)` fixed**: `"x".repeat(-1)` / `s * -n`
  integer-overflowed the allocation into a wild out-of-bounds write. Non-positive
  counts now return `""`; an overflowing product panics cleanly.
- **`Shared.*` handles bounds-checked** and **`Task.channel(0)` rejected** (was a
  silent OOB and an infinite hang, respectively).
- **Data race on the internal run-queue fixed** (`Task::next` is now atomic) -
  TSan-clean on the channel wake path.

### Enums & tooling (final batch)

- **Bare enum constructors** (`Circle(5)`, not just `Shape.Circle(5)`) and the
  `Enum::Variant(payload)` form both lower correctly (the `::` form dropped the
  enum prefix under gcc - a real Linux-only miscompile, now fixed).
- **`wyn run` recompiles** when the compiler binary is newer than the cached
  build (no more running a stale binary after an upgrade).
- **`Task.select`** gives a clean "did you mean `select_2`/`select_3`?" error
  instead of a leaked C compiler error.

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
  segfaulting, and more. The gated invariant is **0 crashes, 0 hangs, 0 internal
  errors** over the harness's generated corpus (`make test` runs seed 1 / 60
  programs; larger runs are manual). That is a bound on what the fuzzer has
  explored, not a proof over all inputs.

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
  the `default`, `cli`, `api` and `web` templates are CI-verified to scaffold a
  full tree, check clean, and pass their own tests (`tests/errors/run_scaffold_test.sh`).
  The `lib` template is not yet in that loop. `http-service` is an alias for `api`.
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
  headers, pkg-config, and per-OS install hints. Two recipes are gated
  end-to-end in CI (add → bind → link → run): `m` and `sqlite3` (the latter
  against a real library with opaque out-parameters). The other 29 were each
  verified by hand at authoring time but are not in the automated suite, since
  gating them would require 29 system libraries on every runner.
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
fuzz invariants + ASan/TSan sanitizer sets. (Correction, added later: at the
1.19.0 tag CI ran only `run_bdd.sh`, so "all green on all platforms" - the
original wording here - was an assertion about local runs, not something the
automation enforced. Those sub-suites became a required check in 1.20.x; the
sanitizer jobs are Linux-only.)

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
| Binary size | 50KB | 50KB |
| fib(35) | 41ms | 41ms |
| Official packages | 31 | 36 |

## v1.10.0 - "The Quality Release" (2026-03-28)

No new language features. Every change is about making the existing language faster, safer, and more stable.

### Performance

- **fib(35)**: much faster via -O2 release builds (41ms as re-measured on v1.20)
- **String length is O(1)**: the RC header now caches length and capacity, so
  `.len()` no longer walks the string. 1M `.len()` calls measure 9ms on v1.20.
  Note: this did *not* make naive concatenation cheap - `s = s + "x"` still
  copies, so a 1M-iteration concat loop is O(n²) and takes ~11.7s. (An earlier
  version of this entry claimed 1M string appends dropped to 6ms; that was
  wrong by roughly three orders of magnitude. Use `StringBuilder` - 1M appends
  in ~14ms.)
- **Array sort**: O(n²) bubble sort replaced with O(n log n) introsort. On
  v1.20, 10K ints measure 9ms and 1M ints 124ms (wall clock, including the
  ~7ms startup floor).
- **`wyn build`**: ~4s → sub-second (288ms dev / 1.17s `--release` on v1.20)
- **Binary size**: dead-code stripping added; an unstripped dev build was
  around 425KB, and `--release` is 50KB today
- **Spawn overhead**: down from ~20μs to ~2μs per spawn+await (v1.20 measurement)
- **4x parallel fib(35)**: real 4x scaling - four in `parallel { }` take the
  same wall time as one (42ms on v1.20)
- **HashMap**: much faster insert and get (FNV-1a + 4096 buckets)

### Memory Safety

The leak patterns listed below are fixed, each verified under AddressSanitizer +
UndefinedBehaviorSanitizer. (Correction, added later: this section was headed
"Zero Leaks" and claimed "every known memory leak pattern is fixed". Releases
1.12, 1.13, 1.19 and 1.20 each found and fixed more - a string use-after-free,
a `repeat` heap corruption, RC container aliasing - so the only defensible claim
is the specific one above.)

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
- **4-platform CI**: macOS, Linux, Windows (ARM + x64) build and smoke-test
- **Online playground**: deployed at play.wynlang.com

### Testing

- 110 unit tests (was ~80)
- 31 expect + regression tests (was 0)
- ASan + UBSan clean on the patterns exercised by those tests
- 30/30 concurrent stress runs correct

(Correction, added later: "all green" and "clean across all patterns" as
originally written here overstated a build-and-smoke-test CI plus 31 tests.
Nothing beyond `run_bdd.sh` was gated by automation until 1.20.x.)

---

## v1.9.0 (Previous)

Generators, Debugger, 36 Packages - all roadmap phases complete.
