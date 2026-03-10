# Concurrency Status

## What was fixed (2026-03-10)

### Race condition in M:N scheduler
Added atomic `running` flag to `Task` struct in `spawn_fast.c`. Prevents two processors
from resuming the same coroutine simultaneously. This was causing "attempt to resume a
coroutine that is not suspended" crashes at >5K concurrent spawns.

### WynCoroutine struct pool increased
Pool size increased from 4K to 64K entries in `coroutine.c`. Reduces malloc pressure
at high spawn counts.

### SpawnArgs pool increased
Pool size increased from 4K to 64K entries in `spawn_fast.c`.

## Current limits

- 5K concurrent spawns: reliable
- 20K total spawns (batched): reliable
- 10K sequential spawn+await: reliable
- >20K concurrent: crashes due to macOS VM region limits (8MB virtual per coroutine)

## What's needed for 100K+ concurrent

The bottleneck is per-coroutine `mmap` (8MB virtual each). At 100K that's 800GB virtual
address space and 100K VM region entries, which exceeds macOS per-process limits.

Options:
1. **Pool allocator**: Single large mmap, carve out fixed-size slots. Eliminates per-coroutine
   mmap overhead. Needs careful implementation — minicoro expects to manage the full
   coroutine struct + stack in one allocation.

2. **Stackless coroutines**: Compiler transforms `spawn fn()` into a state machine struct
   (~128-256 bytes). No stack allocation at all. This is the 1000x improvement path but
   requires significant compiler work.

3. **Smaller stacks**: `WYN_CORO_STACK=8192` allows ~40K concurrent on this system.
   But 8KB limits recursion depth in spawned functions.
