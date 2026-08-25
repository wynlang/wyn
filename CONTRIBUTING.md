# Contributing to Wyn

Thanks for your interest in contributing to Wyn! Here's how to get started.

## Quick Start

```bash
git clone https://github.com/wynlang/wyn.git
cd wyn
make                    # Build the compiler
./wyn run tests/run_tests.wyn  # Run tests (42 should pass)
```

## Branches

- **`dev`** — the integration branch. All ordinary fix/feature PRs target this.
- **`main`** — represents the **last release**. It moves only via a `dev` → `main`
  PR at release time, which is then tagged. Do not target `main` directly.

CI gates PRs into both. Tags (and therefore `release.yml`) are cut from `main`.

## Development Workflow

1. Create a branch from `dev`: `git checkout -b my-feature dev`
2. Write a failing test first (TDD)
3. Implement the minimum code to pass
4. Edit loop: `make check-fast` (~11s: build + golden-C snapshots). This is a
   coarse net for iterating, **not** a merge gate.
5. Before pushing, run the real gate: `make` (0 warnings) then `make test`
   (expect `249 pass, 0 fail` plus the sub-suites; ~9 min). ASan/TSan the affected
   path if you touched runtime or concurrency code.
6. Verify examples still compile: `wyn check` a few from `examples/`
7. Commit with a descriptive message
8. Open a PR against `dev`

**Keep PRs to one concern, ≤5 files / ~150 lines.** Measured on this repo: PRs of
1–5 files merged in 7–29 minutes, while one 76-file PR took 388 minutes and hid
three separate cross-platform failures. A fix plus its regression test is one
concern; a fix plus a refactor is two — land the behaviour-neutral refactor first.

> Note: `./wyn run tests/run_tests.wyn` is a Wyn-native runner used for dogfooding.
> It is **not** the gate and its counts differ from `make test`. `make test` is the
> source of truth.

## Code Style

- C11 standard
- 4-space indentation
- Descriptive variable names
- Comments for non-obvious logic
- No compiler warnings (`-w` flag is used but code should be clean)

## Project Structure

```
src/
  main.c           - CLI, commands, compilation pipeline
  lexer.c           - Tokenizer
  parser.c          - Parser → AST
  checker.c         - Type checker
  codegen.c         - Shared codegen state
  codegen_expr.c    - Expression code generation
  codegen_stmt.c    - Statement code generation
  codegen_program.c - Program-level codegen (forward decls, modules)
  types.c           - Type system, method dispatch tables
  wyn_runtime.h     - Runtime library (all stdlib implementations)
  lsp.c             - Language server protocol
```

## Testing

- All tests are in `tests/`
- Test runner: `tests/run_tests.wyn`
- Add new tests to `tests/stdlib/` and update `run_tests.wyn`
- Every PR must pass all existing tests (zero regression policy)

## What to Work On

Check [TASKS.md](../internal-docs/TASKS.md) for the current roadmap, or look for issues labeled `good first issue` on GitHub.

## Reporting Bugs

Please include:
1. Wyn version (`wyn version`)
2. OS and architecture
3. Minimal `.wyn` file that reproduces the issue
4. Expected vs actual output

## Questions & Discussion

- **GitHub Discussions**: [github.com/wynlang/wyn/discussions](https://github.com/wynlang/wyn/discussions)
- **Bug Reports**: Use the issue templates on GitHub
