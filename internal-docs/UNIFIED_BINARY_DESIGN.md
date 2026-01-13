# Wyn Compiler - Unified Binary Design
## January 13, 2026

---

## Overview

The Wyn compiler is a unified binary that provides all development tools through subcommands, similar to `cargo`, `go`, or `rustc`.

---

## Command Structure

```bash
wyn [command] [options] [arguments]
```

### Core Commands

#### `wyn compile [file.wyn]`
Compile Wyn source code to executable binary.

**Behavior:**
- With filename: Compiles specified file
- Without filename: Compiles current directory (finds `fn main()` and pulls in all dependencies)

**Examples:**
```bash
wyn compile main.wyn          # Compile specific file
wyn compile                   # Compile current directory
wyn compile src/lib.wyn       # Compile library file
```

**Options:**
- `-o <output>` - Specify output filename
- `-O <level>` - Optimization level (0-3)
- `--emit-c` - Emit C code only
- `--verbose` - Show compilation steps

#### `wyn test [directory]`
Run test suite.

**Behavior:**
- Finds all `test_*.wyn` files
- Compiles and executes each test
- Reports pass/fail statistics
- Exits with code 0 if all pass, 1 if any fail

**Examples:**
```bash
wyn test                      # Run tests in ./tests
wyn test tests/unit           # Run tests in specific directory
```

#### `wyn fmt [file.wyn]`
Format Wyn source code.

**Behavior:**
- Parses AST and pretty-prints with consistent style
- Modifies file in-place (or use `--check` to verify only)

**Examples:**
```bash
wyn fmt main.wyn              # Format specific file
wyn fmt src/*.wyn             # Format all files in directory
wyn fmt --check main.wyn      # Check if formatted (CI mode)
```

#### `wyn repl`
Interactive Read-Eval-Print Loop.

**Behavior:**
- Evaluates expressions line-by-line
- Maintains state across lines
- Supports multi-line input

**Examples:**
```bash
wyn repl                      # Start interactive shell
```

#### `wyn doc [file.wyn]`
Generate documentation.

**Behavior:**
- Parses source files
- Extracts doc comments
- Generates markdown documentation

**Examples:**
```bash
wyn doc main.wyn              # Generate docs for file
wyn doc src/                  # Generate docs for directory
```

#### `wyn pkg <subcommand>`
Package management.

**Subcommands:**
- `init` - Initialize new package (creates `wyn.toml`)
- `add <package>` - Add dependency
- `remove <package>` - Remove dependency
- `update` - Update dependencies
- `build` - Build package
- `publish` - Publish to registry

**Examples:**
```bash
wyn pkg init                  # Create new package
wyn pkg add http              # Add dependency
wyn pkg build                 # Build package
```

#### `wyn lsp`
Language Server Protocol server.

**Behavior:**
- Starts LSP server on stdio
- Provides IDE integration (hover, completion, diagnostics, etc.)

**Examples:**
```bash
wyn lsp                       # Start LSP server
```

#### `wyn debug <program>`
Debug Wyn programs.

**Behavior:**
- Interactive debugger with breakpoints
- Step through execution
- Inspect variables

**Examples:**
```bash
wyn debug program.wyn         # Start debugger
```

### Utility Commands

#### `wyn init`
Initialize new Wyn project in current directory.

**Creates:**
- `wyn.toml` - Package manifest
- `src/main.wyn` - Main source file
- `.gitignore` - Git ignore file

#### `wyn version`
Display version information.

```bash
wyn version                   # Show version
```

#### `wyn help [command]`
Display help information.

```bash
wyn help                      # Show all commands
wyn help compile              # Show compile command help
```

---

## Implementation Architecture

### Main Entry Point (`main.c`)

```c
int main(int argc, char** argv) {
    if (argc < 2) {
        // No arguments: compile current directory
        return cmd_compile(".", argc, argv);
    }
    
    const char* cmd = argv[1];
    
    if (strcmp(cmd, "compile") == 0) {
        return cmd_compile(argc > 2 ? argv[2] : ".", argc, argv);
    } else if (strcmp(cmd, "test") == 0) {
        return cmd_test(argc > 2 ? argv[2] : "tests", argc, argv);
    } else if (strcmp(cmd, "fmt") == 0) {
        return cmd_format(argc > 2 ? argv[2] : NULL, argc, argv);
    } else if (strcmp(cmd, "repl") == 0) {
        return cmd_repl(argc, argv);
    } else if (strcmp(cmd, "doc") == 0) {
        return cmd_doc(argc > 2 ? argv[2] : NULL, argc, argv);
    } else if (strcmp(cmd, "pkg") == 0) {
        return cmd_pkg(argc, argv);
    } else if (strcmp(cmd, "lsp") == 0) {
        return cmd_lsp(argc, argv);
    } else if (strcmp(cmd, "debug") == 0) {
        return cmd_debug(argc > 2 ? argv[2] : NULL, argc, argv);
    } else if (strcmp(cmd, "init") == 0) {
        return cmd_init(argc, argv);
    } else if (strcmp(cmd, "version") == 0) {
        return cmd_version(argc, argv);
    } else if (strcmp(cmd, "help") == 0) {
        return cmd_help(argc > 2 ? argv[2] : NULL, argc, argv);
    } else {
        // No recognized command: treat as filename to compile
        return cmd_compile(argv[1], argc, argv);
    }
}
```

### Command Implementations

Each command is implemented in its own module:
- `cmd_compile.c` - Compilation logic
- `cmd_test.c` - Test runner
- `cmd_format.c` - Code formatter
- `cmd_repl.c` - REPL
- `cmd_doc.c` - Documentation generator
- `cmd_pkg.c` - Package manager
- `cmd_lsp.c` - LSP server
- `cmd_debug.c` - Debugger

---

## Migration Plan

### Phase 1: Unified Binary
1. Create `cmd_*.c` files for each command
2. Update `main.c` to dispatch to commands
3. Update Makefile to build single binary
4. Remove separate tool binaries

### Phase 2: Fix Stubs
1. Implement real formatter (AST-based)
2. Implement real test runner (parse results)
3. Implement real REPL (expression evaluation)
4. Implement real doc generator (parse doc comments)
5. Implement real package manager (dependency resolution)
6. Implement real LSP server (full protocol)
7. Implement real debugger (breakpoints + execution)

### Phase 3: Directory Compilation
1. Implement module discovery in `cmd_compile.c`
2. Find `fn main()` entry point
3. Resolve imports and dependencies
4. Compile all required modules

---

## Testing Strategy

Each command must have comprehensive tests:

```bash
# Test compile command
wyn compile tests/unit/test_01_hello.wyn
./tests/unit/test_01_hello.wyn.out
echo $?  # Should be 0

# Test directory compile
cd examples/hello
wyn compile
./main.out
echo $?  # Should be 0

# Test test runner
wyn test tests/unit
echo $?  # Should be 0 if all pass

# Test formatter
wyn fmt --check tests/unit/test_01_hello.wyn
echo $?  # Should be 0 if formatted

# Test REPL
echo "return 42;" | wyn repl
echo $?  # Should be 42

# Test doc generator
wyn doc src/main.wyn
test -f docs/main.md
echo $?  # Should be 0

# Test package manager
wyn pkg init
test -f wyn.toml
echo $?  # Should be 0

# Test LSP server
echo '{"jsonrpc":"2.0","method":"initialize"}' | wyn lsp
# Should return valid JSON-RPC response

# Test debugger
wyn debug tests/unit/test_01_hello.wyn
# Should start interactive debugger
```

---

## Success Criteria

- ✅ Single `wyn` binary (no separate tool binaries)
- ✅ All commands work with real implementations (no stubs)
- ✅ `wyn compile` works with and without filename
- ✅ `wyn compile` in directory finds main() and dependencies
- ✅ All 118 regression tests pass
- ✅ Comprehensive test coverage for each command
- ✅ Documentation complete and accurate
