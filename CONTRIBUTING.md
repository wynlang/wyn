# Contributing to Wyn

Thanks for your interest in contributing to Wyn! Here's how to get started.

## Quick Start

```bash
git clone https://github.com/wynlang/wyn.git
cd wyn
make                    # Build the compiler
./wyn run tests/run_tests.wyn  # Run tests (42 should pass)
```

## Development Workflow

1. Create a branch from `dev`: `git checkout -b my-feature dev`
2. Write a failing test first (TDD)
3. Implement the minimum code to pass
4. Run the full test suite: `./wyn run tests/run_tests.wyn`
5. Verify sample apps still compile: check a few from `../sample-apps/`
6. Commit with a descriptive message
7. Open a PR against `dev`

## Code Style

- C11 standard
- 4-space indentation
- Descriptive variable names
- Comments for non-obvious logic
- No compiler warnings (`-w` flag is used but code should be clean)

## Project Structure

```
src/
  main.c           — CLI, commands, compilation pipeline
  lexer.c           — Tokenizer
  parser.c          — Parser → AST
  checker.c         — Type checker
  codegen.c         — Shared codegen state
  codegen_expr.c    — Expression code generation
  codegen_stmt.c    — Statement code generation
  codegen_program.c — Program-level codegen (forward decls, modules)
  types.c           — Type system, method dispatch tables
  wyn_runtime.h     — Runtime library (all stdlib implementations)
  lsp.c             — Language server protocol
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
