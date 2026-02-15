# Parallel Testing Quick Reference

## Run Tests

```bash
make test                    # Run all tests (2 seconds)
./scripts/test_parallel.sh   # Same as above
```

## Add New Test

1. Create `tests/unit/test_myfeature.wyn`:
```wyn
fn main() -> int {
    // Your test
    return 0  // 0 = pass, non-zero = fail
}
```

2. Run `make test` - automatically included!

## Debug Test

```bash
./wyn tests/unit/test_myfeature.wyn
./tests/unit/test_myfeature.out
```

## Regenerate Runner

```bash
./scripts/gen_test_runner.sh
```

## Performance

- **90 tests** in **~2 seconds**
- **50 parallel workers**
- **20x faster** than bash

## Known Issues

76 tests segfault with redirected output (see `docs/issues/SEGFAULT_REDIRECTED_OUTPUT.md`)

## Files

- `src/thread_pool.c` - Thread pool implementation
- `scripts/gen_test_runner.sh` - Test discovery
- `scripts/test_parallel.sh` - Test runner
- `tests/run_all_tests.wyn` - Generated (don't edit)
- `docs/PARALLEL_TESTING.md` - Full documentation
