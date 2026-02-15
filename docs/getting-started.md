# Getting Started with Wyn

## Installation

Wyn is built from source:

```bash
git clone https://github.com/AO-Design-Inc/wyn.git
cd wyn
make
```

This produces the `wyn` binary in the current directory.

## Your First Program

Create `hello.wyn`:

```wyn
fn main() -> int {
    println("Hello, World!")
    return 0
}
```

Run it:

```bash
./wyn run hello.wyn
```

## How It Works

Wyn is a **C transpiler**. When you run `./wyn run hello.wyn`:

1. `hello.wyn` → parsed into AST
2. AST → `hello.wyn.c` (generated C code)
3. `hello.wyn.c` → `hello.wyn.out` (compiled by system `cc`)
4. `hello.wyn.out` → executed

## CLI Commands

```bash
./wyn <file.wyn>           # Compile only
./wyn run <file.wyn>       # Compile and run
./wyn build <dir>          # Build all .wyn files
./wyn test                 # Run tests
./wyn fmt <file.wyn>       # Format/validate
./wyn watch <file.wyn>     # Watch and auto-rebuild
./wyn install <package>    # Install package (path or git URL)
./wyn install              # Install deps from wyn.toml
./wyn pkg list             # List installed packages
./wyn cross <os> <file>    # Cross-compile (linux/windows/macos)
```

## Next Steps

- [Language Guide](language-guide.md) — Full syntax reference
- [Standard Library](stdlib-reference.md) — All available modules
- [Tutorial](tutorial.md) — Step-by-step learning
- [Examples](examples.md) — Real-world programs
