# Internal Documentation

**Last Updated:** 2026-01-30  
**Status:** Organized and current

This directory contains internal development documentation for the Wyn compiler project.

## Directory Structure

```
internal/
├── testing/              # Testing documentation
│   ├── PARALLEL_TESTING.md
│   ├── TESTING_QUICK_REFERENCE.md
│   ├── README_TDD.md
│   ├── TDD_IMPLEMENTATION_GUIDE.md
│   └── QUICK_START.md
│
├── development/          # Development notes
│   ├── LLVM_PATH_TO_100.md
│   ├── V1.6.0_STABILIZATION_COMPLETE.md
│   └── IMPLEMENTATION_SUMMARY.md
│
└── README.md            # This file
```

## Testing Documentation

### [PARALLEL_TESTING.md](testing/PARALLEL_TESTING.md)
Comprehensive guide to the parallel test infrastructure:
- 4.1x speedup over sequential testing
- 12 parallel workers
- Test isolation and safety

### [TESTING_QUICK_REFERENCE.md](testing/TESTING_QUICK_REFERENCE.md)
Quick reference for running tests:
- Common test commands
- Test organization
- Debugging tips

### [README_TDD.md](testing/README_TDD.md)
Test-Driven Development guide:
- TDD methodology
- Writing tests first
- Red-Green-Refactor cycle

### [TDD_IMPLEMENTATION_GUIDE.md](testing/TDD_IMPLEMENTATION_GUIDE.md)
Detailed TDD implementation guide:
- Test structure
- Best practices
- Examples

### [QUICK_START.md](testing/QUICK_START.md)
Quick start guide for testing:
- Running tests
- Creating new tests
- Common patterns

## Development Documentation

### [LLVM_PATH_TO_100.md](development/LLVM_PATH_TO_100.md)
Journey to 100% LLVM backend:
- Migration from C codegen
- LLVM implementation details
- Performance improvements

### [V1.6.0_STABILIZATION_COMPLETE.md](development/V1.6.0_STABILIZATION_COMPLETE.md)
Version 1.6.0 stabilization notes:
- Bug fixes
- Feature completions
- Test coverage

### [IMPLEMENTATION_SUMMARY.md](development/IMPLEMENTATION_SUMMARY.md)
Implementation summary:
- Architecture overview
- Key components
- Design decisions

## Quick Links

### Running Tests
```bash
# Run all unit tests (parallel)
./parallel_unit_tests.sh

# Run single test
./wyn tests/unit/test_strings.wyn
./tests/unit/test_strings.out
```

### Building Compiler
```bash
# Build with LLVM backend
make clean && make wyn-llvm && mv wyn-llvm wyn

# Verify build
./wyn --version
```

### Test Status
- **Unit Tests:** 198/198 passing ✅
- **Coverage:** All edge cases tested
- **Performance:** 3-4x speedup with parallel testing

## Related Documentation

- **User Docs:** `../` (parent directory)
- **Session Summaries:** `../sessions/`
- **Language Spec:** `../spec/`
- **Test Suite:** `../../tests/`

## Contributing

When adding internal documentation:
1. Place testing docs in `testing/`
2. Place development notes in `development/`
3. Update this README with links
4. Use clear, descriptive filenames
5. Include date and status in documents
