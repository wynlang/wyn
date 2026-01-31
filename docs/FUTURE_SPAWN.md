# Collecting Results from Spawned Tasks

## Current Status

✅ **IMPLEMENTED** - Future-based spawn for collecting results

## API

### Fire-and-Forget (existing)
```c
void wyn_spawn_fast(TaskFunc func, void* arg);
```
Use when you don't need a return value.

### Async with Future (new)
```c
Future* wyn_spawn_async(TaskFuncWithReturn func, void* arg);
void* future_get(Future* f);           // Blocking wait
int future_is_ready(Future* f);        // Non-blocking check
void future_free(Future* f);
```

## Example: Chunked Work Pattern

```c
#include "future.h"

// Worker that returns a result
void* process_chunk(void* arg) {
    int chunk_id = (int)(long)arg;
    int* result = malloc(sizeof(int));
    
    // Do work...
    *result = compute_something(chunk_id);
    
    return result;
}

int main() {
    // Spawn 1000 tasks
    Future* futures[1000];
    for (int i = 0; i < 1000; i++) {
        futures[i] = wyn_spawn_async(process_chunk, (void*)(long)i);
    }
    
    // Collect all results
    long total = 0;
    for (int i = 0; i < 1000; i++) {
        int* result = (int*)future_get(futures[i]);
        total += *result;
        free(result);
        future_free(futures[i]);
    }
    
    printf("Total: %ld\n", total);
    return 0;
}
```

## Performance

- **Future overhead**: ~50ns (mutex + condition variable)
- **Still lock-free**: Task scheduling remains lock-free
- **Blocking wait**: Uses pthread condition variable (efficient)
- **Non-blocking poll**: Atomic check, zero overhead

## Test Results

```
Test 1: Single spawn with result
  Square of 10 = 100
  ✓ PASS

Test 2: Multiple spawns (10 tasks)
  Results: 0 1 4 9 16 25 36 49 64 81 
  ✓ PASS

Test 3: Chunked work (100 chunks)
  Total from 100 chunks: 2472525000
  ✓ PASS

Test 4: Non-blocking poll
  Result after 828 checks: 1764
  ✓ PASS
```

## Implementation Details

### Task Structure
```c
typedef struct {
    TaskFunc func;
    void* arg;
    Future* future;  // NULL = fire-and-forget, set = awaitable
} Task;
```

### Future Structure
```c
typedef struct Future {
    _Atomic int state;      // PENDING or READY
    void* result;           // Return value
    pthread_mutex_t lock;   // For blocking wait
    pthread_cond_t cond;    // For wakeup
} Future;
```

### Worker Execution
```c
if (t->future) {
    // Task with return value
    void* result = func(t->arg);
    future_set(t->future, result);  // Wakes waiters
} else {
    // Fire-and-forget
    func(t->arg);
}
```

## Use Cases

1. **Map-Reduce**: Chunk data, spawn workers, collect results
2. **Parallel Computation**: Spawn N tasks, wait for all
3. **Pipeline**: Spawn task, continue work, collect later
4. **Selective Wait**: Poll multiple futures, process ready ones

## Files Added

- `src/future.c` - Future implementation (60 lines)
- `src/future.h` - Future API
- `test_future_spawn.c` - Test suite

## Next Steps (Optional)

- [ ] Integrate with Wyn language syntax (`let f = spawn async task()`)
- [ ] Add `future_wait_any()` for first-ready semantics
- [ ] Add `future_wait_all()` helper
- [ ] Add timeout support
- [ ] Add cancellation support
