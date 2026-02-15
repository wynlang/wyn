# Testing Flow - Quick Reference

## Commands

```bash
# Parallel testing (recommended)
./parallel_unit_tests.sh                    # 4.1x faster (139s)

# Sequential testing (for debugging)
for f in tests/unit/*.wyn; do               # Slower (568s)
    ./wyn "$f" >/dev/null 2>&1 && echo "✅ $f" || echo "❌ $f"
done

# Single test
./wyn tests/unit/test_name.wyn             # Compile and check
./tests/unit/test_name.out                 # Run if compiled

# Custom worker count
NUM_JOBS=8 ./parallel_unit_tests.sh        # Use 8 workers
```

## Performance

| Method | Time | Speedup | Use Case |
|--------|------|---------|----------|
| **Parallel** | 2m19s | 4.1x | Development, CI/CD |
| **Sequential** | 9m28s | 1x | Debugging |
| **Single Test** | ~3s | N/A | Focused testing |

## Test Locations

```
tests/unit/          # 181 unit tests (parallel tested)
tests/              # 128 integration tests
```

## Workflow

### Development Cycle

```bash
# 1. Make changes to compiler
vim src/checker.c

# 2. Rebuild
make clean && make wyn-llvm && mv wyn-llvm wyn

# 3. Run parallel tests
./parallel_unit_tests.sh

# 4. If all pass, commit
git add . && git commit -m "Feature: ..."
```

### Debugging Failed Test

```bash
# 1. Run test individually
./wyn tests/unit/failing_test.wyn

# 2. Check output
cat tests/unit/failing_test.ll    # LLVM IR
cat tests/unit/failing_test.o     # Object file

# 3. Run executable
./tests/unit/failing_test.out

# 4. Fix and retest
./wyn tests/unit/failing_test.wyn
```

## Exit Codes

- `0` - All tests passed
- `1` - Some tests failed

## Common Issues

### Linking Errors

```bash
# Symptom: "duplicate symbol" errors
# Fix: Check for duplicate function definitions
grep -r "function_name" src/*.c
```

### Compilation Errors

```bash
# Symptom: Tests fail to compile
# Fix: Rebuild compiler
make clean && make wyn-llvm && mv wyn-llvm wyn
```

### Performance Issues

```bash
# Symptom: Parallel tests slower than expected
# Fix: Reduce workers
NUM_JOBS=4 ./parallel_unit_tests.sh
```

## Statistics

- **Total Tests:** 181
- **Pass Rate:** 100%
- **Parallel Time:** 139s (2m19s)
- **Sequential Time:** 568s (9m28s)
- **Speedup:** 4.1x
- **Workers:** 12 (auto-detected)

## Integration

### Pre-commit Hook

```bash
#!/bin/bash
# .git/hooks/pre-commit
cd wyn
./parallel_unit_tests.sh || exit 1
```

### CI/CD Pipeline

```yaml
# .github/workflows/test.yml
- name: Run Tests
  run: |
    cd wyn
    make clean && make wyn-llvm && mv wyn-llvm wyn
    ./parallel_unit_tests.sh
```

## See Also

- [PARALLEL_TESTING.md](PARALLEL_TESTING.md) - Full documentation
- [LLVM_PATH_TO_100.md](LLVM_PATH_TO_100.md) - LLVM backend
- [V1.6.0_STABILIZATION_COMPLETE.md](V1.6.0_STABILIZATION_COMPLETE.md) - Stability

---

**Quick Tip:** Always use parallel testing for full test runs. It saves 7+ minutes per run!
