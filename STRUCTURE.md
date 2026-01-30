# Wyn Repository Structure

**Last Updated:** 2026-01-30  
**Status:** Organized and cleaned

## Directory Structure

```
wyn/
├── src/                    # Compiler source code
│   ├── *.c                 # C implementation files
│   └── *.h                 # Header files
│
├── tests/                  # Test suite
│   ├── unit/               # Unit tests (198 tests) ✅
│   ├── integration/        # Integration tests
│   ├── benchmarks/         # Performance benchmarks
│   ├── edge_cases/         # Edge case tests
│   ├── old-tests/          # Legacy test files (archived)
│   ├── scratch/            # Temporary test files
│   └── shell-scripts/      # Test automation scripts
│
├── docs/                   # Documentation
│   ├── sessions/           # Development session summaries
│   ├── internal/           # Internal documentation
│   │   ├── PARALLEL_TESTING.md
│   │   ├── TESTING_QUICK_REFERENCE.md
│   │   └── V1.6.0_STABILIZATION_COMPLETE.md
│   ├── spec/               # Language specification
│   ├── getting-started.md
│   ├── language-guide.md
│   ├── stdlib-reference.md
│   └── tutorial.md
│
├── scripts/                # Build and utility scripts
│   ├── testing/            # Test scripts
│   │   ├── parallel_unit_tests.sh
│   │   └── test_*.sh
│   ├── merge_compiler.sh
│   ├── release.sh
│   └── update-version.sh
│
├── examples/               # Example programs
│
├── Makefile               # Build configuration
├── README.md              # Project README
├── TODO.md                # Known issues and roadmap
├── CHANGELOG.md           # Version history
├── LICENSE                # License file
├── install.sh             # Installation script
└── uninstall.sh           # Uninstallation script
```

## Key Files

### Root Directory
- `Makefile` - Build the compiler with `make wyn-llvm`
- `parallel_unit_tests.sh` - Symlink to test runner (runs 198 tests)
- `TODO.md` - Current status and known issues
- `wyn` - Compiled binary

### Test Suite
- **Unit Tests**: `tests/unit/` - 198 passing tests
- **Run Tests**: `./parallel_unit_tests.sh` (3-4x faster than sequential)
- **Test Scripts**: `tests/shell-scripts/` - Various test automation

### Documentation
- **User Docs**: `docs/` - Getting started, tutorials, language guide
- **Internal Docs**: `docs/internal/` - Development notes, testing guides
- **Session Summaries**: `docs/sessions/` - Development session records

### Scripts
- **Build Scripts**: `scripts/` - Compiler build and release automation
- **Test Scripts**: `scripts/testing/` - Test execution and validation

## Quick Start

```bash
# Build compiler
make clean && make wyn-llvm && mv wyn-llvm wyn

# Run all tests
./parallel_unit_tests.sh

# Run single test
./wyn tests/unit/test_strings.wyn
./tests/unit/test_strings.out

# Install
sudo ./install.sh
```

## Test Organization

### Active Tests
- `tests/unit/` - Current unit tests (198 tests, all passing)
- `tests/integration/` - Integration tests
- `tests/benchmarks/` - Performance tests

### Archived
- `tests/old-tests/` - Legacy test files (kept for reference)
- `tests/scratch/` - Temporary/experimental tests

## Documentation Organization

### User-Facing
- `docs/*.md` - User documentation
- `docs/spec/` - Language specification

### Developer-Facing
- `docs/internal/` - Internal development docs
- `docs/sessions/` - Session summaries and progress reports

## Cleanup Summary

**Files Moved:**
- Test files: `test_*.wyn` → `tests/scratch/` or `tests/old-tests/`
- Session docs: `SESSION_*.md` → `docs/sessions/`
- Test scripts: `test_*.sh` → `scripts/testing/`
- Build scripts: `*.sh` → `scripts/`
- Internal docs: `internal-docs/` → `docs/internal/`

**Result:**
- Clean root directory
- Organized test structure
- Consolidated documentation
- All 198 tests still passing ✅
