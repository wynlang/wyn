# Changelog

## v1.9.0 (2026-03-18)
- Generators/yield: `yield` keyword, `.collect()`, `.map()`, `.filter()`, `.take()`, `for x in gen()`
- Debugger: `wyn debug` with DWARF + lldb integration, shows .wyn source
- LSP: find-all-references, rename, prepareRename (scope-aware, cross-file)
- 5 new packages: sdl, opengl, wgpu, target-ios, target-android (36 total)
- Cross-compile CI: linux-arm64 + windows-x64 via zig
- Fix: nested string interpolation crash
- Fix: type alias checker for string/float/bool types
- Fix: Windows cross-compile signal.h
- Fix: TCC DT_DIR compatibility

## v1.8.0 (2026-02-27)
- Concurrency: spawn/await with M:N scheduler, coroutine pool, await_all/await_any
- Shared atomic values: Shared.new(), .get, .set, .add, .sub
- Cross-compilation: Linux x64/arm64, Windows x64, iOS, Android
- Package registry: wyn pkg install/push, lockfile, transitive deps
- 31 official packages with real source code
- LSP server: diagnostics, hover, go-to-def, completions
- Online playground: play.wynlang.com
- 90 tests, 43 sample apps

## v1.7.0 (2026-01-15)
- Traits/interfaces, generic functions with trait bounds
- Pattern matching: enum destructuring, Ok/Err/Some/None
- ? operator for Result/Option unwrapping
- Type aliases, range types (1..10, 1..=10)
- Destructuring assignment
- ARC memory management with move semantics

## v1.6.0 (2025-12-01)
- Closures with capture, returning closures
- HashMap and HashSet collections
- JSON parsing and generation
- File I/O and networking (TCP sockets)
- wyn test, wyn fmt, wyn doc commands

## v1.5.0 (2025-10-15)
- Structs with methods and impl blocks
- Enums (simple and data variants)
- For-in loops over arrays
- Array methods (push, pop, map, filter, sort)

## v1.4.0 (2025-09-01)
- Float support, string methods
- Math module, error handling (Result type)
- While loops, constants

## v1.3.0 (2025-07-15)
- Functions with return types
- If/else expressions
- Basic module system

## v1.2.0 (2025-06-01)
- Variables (var, const)
- Basic types (int, string, bool)
- Print/println
- Basic arithmetic

## v1.1.0 (2025-05-01)
- Initial release
- Lexer, parser, C codegen
- Hello world
