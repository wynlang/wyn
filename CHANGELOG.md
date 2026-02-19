# Changelog

## v1.7.0 — 2026-02-19

### Language Features
- **Full string interpolation** — `"${arr[1]}"`, `"${name.upper()}"`, `"${fn(x)}"` all work
- **Single-quote strings** — `'no interpolation'`, enables nested quotes in `${}`
- **Variable shadowing** — `var x = 1; var x = "hello"` works across types
- **Enums with associated data** — `enum Shape { Circle(float), Point }` with destructuring match
- **Pipe operator** — `5 |> double |> add1`
- **Defer statement** — `defer cleanup()` runs at function exit in LIFO order
- **Inclusive range** — `for i in 0..=5` includes 5
- **Script mode** — no `fn main()` needed, shebang support, `wyn run -e 'code'`
- **Type aliases** — `type UserId = int`
- **`%=` operator** — compound modulo assignment

### Standard Library
- **Template engine** — `Template.render(path, ctx)` with `${if:}`, `${each:}`, `${include:}`, auto HTML-escape
- **Parameterized SQL** — `Db.exec_p(db, "? ?", [val])` prevents injection
- **HashMap.keys()/values()** return arrays (not strings)
- **System.load_env(".env")** — load environment files
- **String methods** — `.starts_with()`, `.ends_with()`, `.replace_all()`, `.pad_left()`, `.pad_right()`, `.chars()`, `.bytes()`

### CLI
- **`wyn deploy`** — deploy via SSH with wyn.toml config, `--dry-run`, `wyn ssh`, `wyn logs`
- **`wyn init --web/--cli/--lib`** — project templates that compile immediately
- **`wyn upgrade`** — self-update from GitHub releases
- **`wyn doc --html`** — generate static HTML documentation
- **`wyn bench`** — auto-compares with previous run
- **`wyn test --parallel`** — concurrent test execution
- **`wyn fmt <dir> --check`** — CI-friendly format checking
- **`wyn explain E001`** — detailed error explanations
- **`wyn build`** reads `entry` from `wyn.toml`

### Developer Experience
- **Error messages** show source code with line numbers and colored carets
- **"Did you mean?"** suggestions for typos
- **Warning** for `=` in if conditions (did you mean `==`?)
- **REPL** auto-prints expression results
- **Precompiled header** — 3.6x faster compilation (5s → 1.4s)
- **Multi-file projects** — `import utils` loads `utils.wyn` from same directory

## v1.6.0 — 2026-02-16

### Language Features

- **Unified struct syntax** — methods directly in struct body, no separate `impl` needed
- **List comprehensions** — `[x * x for x in 0..10 if x > 3]`
- **Slice syntax** — `arr[1:3]` and `str[0:5]` (both `:` and `..`)
- **Arrow lambdas** — `fn(x) => x * 2`
- **Enum match patterns** — `match c { Color.Red => "red" }`
- **Trait dynamic dispatch** — `fn render(s: Shape)` with auto call-site wrapping
- **Futures in arrays** — `spawn` results stored and awaited from arrays
- **Mutable references** — `fn inc(mut x: int)` + `inc(&n)`
- **String interpolation** — `"hello ${name}"` with expressions

### Standard Library — 27 Modules, 270+ Methods

- **New: Csv** (7 methods) — parse, row_count, col_count, get, get_field, header, header_count
- **New: Math** — sin, cos, tan, atan2, round, floor, ceil, round_to, pi, e
- **New: Json** — to_pretty_string, set_bool
- **New: Http** — set_timeout, timeout
- **New: Encoding** — base64_encode/decode, hex_encode/decode
- **New: Crypto** — sha256, md5, hmac_sha256, random_bytes
- **New: Os** — platform, arch, hostname, pid, home_dir, temp_dir
- **New: Uuid** — generate (v4)
- **New: Log** — debug, info, warn, error, set_level
- **New: Process** — exec_capture, exec_status
- **New: StringBuilder** — append, append_int, append_line, to_string, len, clear
- **String** — 29 methods including to_float
- **Array** — push/pop now handle 64-bit values (futures, pointers)
- **HashMap** — remove, clear, values, get_or
- **ResultInt** — unwrap_or added

### CLI

- **New: `wyn build --python`** — compile to shared library + auto-generated Python wrapper with type hints
- **New: `wyn build --shared`** — compile to shared library (`.so`/`.dylib`/`.dll`)
- **New: `wyn repl`** — interactive REPL with definition accumulation
- **New: `wyn bench`** — 5-run benchmark with avg/min/max
- **New: `wyn doc`** — generate docs from source comments
- **New: `wyn install` / `wyn uninstall`** — system PATH management
- **New: `wyn pkg uninstall`** — remove packages
- **New: `wyn pkg search`** — search installed packages
- **Wyvern ASCII art banner** with colored help
- **Auto-cleanup** — `wyn run` removes .c and .out (use `--debug` to keep)
- **`wyn build`** — produces clean binary name (no `.wyn.out` suffix)
- **`wyn test`** — uses `tests/run_tests.wyn` or `tests/test_*.wyn`
- **`async` deprecated** — use `spawn`/`await` instead

### Cross-Platform

- **Windows fully supported** — compat layer for dirent, Winsock, terminal, temp paths
- **iOS cross-compilation** — `wyn cross ios file.wyn`
- **Android cross-compilation** — `wyn cross android file.wyn`
- **5 targets** — linux, macos, windows, ios, android

### Editor Support

- **LSP** — completions for all 27 modules, dot-triggered method hints
- **VS Code** — syntax highlighting, LSP client, v1.6.0
- **Neovim** — syntax highlighting, LSP config, all keywords

### Performance

- Spawn: 2μs per task, 180 bytes memory
- Compilation: 62ms cached
- M:N work-stealing scheduler

### Bug Fixes

- Math.abs used float (precision loss) → long long
- File.size returned -1 for missing files → 0
- string_split skipped empty fields (strtok) → strstr
- Lambda codegen used int → long long
- Checker return type array capped at 64 → dynamic
- Module function return types not inferred → lookup table
- String interpolation buffer 256 → 8192, heap-allocated
- Parser segfault on statement overflow → exit(1)
- Spawn wrapper scanner missed for-loops
- Futures in arrays: WynValue.int_val int → long long

### Infrastructure

- GitHub Actions CI: Ubuntu, macOS ARM, macOS Intel, Windows
- Release workflow: 4 platform binaries
- Site: wynlang.com with install scripts (sh + ps1)
- GitHub org: github.com/wynlang

## v1.5.0 and earlier

See git history for previous releases.
