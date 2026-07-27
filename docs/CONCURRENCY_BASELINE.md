# Concurrency baseline (S0) — awaited-concurrency path

**Stage:** S0 of the coroutine-backed await epic (see
`repos/internal-docs/ROADMAP.md` → "Coroutine-backed await").
**Purpose:** pin down and document the CURRENT behavior of the awaited-concurrency
path (`spawn`+`await`, `await_all`, `parallel { }`) so S1/S2 have a regression
baseline. **No scheduler/spawn/coroutine/future logic was changed in S0** — this
stage only ADDS probe tests, a timing script, and this document.

Measured on: macOS arm64 (Darwin 25.5.0), 12 CPUs, `main`-derived branch
`epic/await-coro-s0` off `c5aa828`. Regenerate the timing numbers with
`scripts/concurrency_timing_probe.sh`.

---

## Headline finding (updates a stale ROADMAP claim)

The ROADMAP epic text says awaited work "runs on the **thread pool**… so
cooperative primitives do NOT engage." **That is no longer true on `main`.** The
S1/S2/S3 work described in the staged plan has already landed in the tree:

- **The coroutine M:N scheduler is already the DEFAULT executor for awaited
  `spawn` / `await_all` / `parallel`.** `wyn_spawn_async_traced` reads the executor
  choice once and defaults `async_coro = 1`
  (**`src/spawn_fast.c:715-723`**), dispatching to `wyn_spawn_async_coro`
  (**`src/spawn_fast.c:546-570`**). The legacy thread pool is now an opt-in escape
  hatch behind `WYN_ASYNC_POOL=1` (or `WYN_ASYNC_CORO=0`).
- **Await-from-main already pumps the scheduler (M1).** Main-thread `future_get`
  runs `wyn_sched_pump_one()` (pop/steal/io_poll/execute_task) until the future is
  ready (**`src/future.c:228-244`**, pump impl **`src/spawn_fast.c:756-762`**), so
  awaited coroutines resume even with no idle worker processors.
- **Await-from-coroutine parks cooperatively.** When `wyn_coro_current()` is true,
  `future_get` registers the task as the future's single waiter via CAS, parks
  (`wyn_io_park`) and yields; `future_set` wakes it by re-enqueueing
  (**`src/future.c:164-205`**, wake path **`src/future.c:140-145`**).
- **Cooperative `Time::sleep` engages inside awaited tasks** because they now run as
  coroutines. `Time_sleep` takes the cooperative timer branch
  (`wyn_io_wait_timer` + park + yield) whenever `wyn_coro_current()` is true
  (**`src/wyn_runtime.h:3828-3843`**), which is now the case for awaited spawns, not
  just fire-and-forget ones.

**Net:** the awaited path is ALREADY cooperative today. What remains for the epic
on top of this baseline is hardening/verification and S4 cancellation, not the
core pool→coroutine migration (already done).

### The pool-vs-coroutine branch point S1 will modify

**`src/spawn_fast.c:704-723`**, `wyn_spawn_async_traced()` — specifically the
executor-selection block:

```c
// src/spawn_fast.c:715-723
static int async_coro = -1;
if (async_coro < 0) {
    const char* pool = getenv("WYN_ASYNC_POOL");
    const char* coro = getenv("WYN_ASYNC_CORO");
    if (pool && pool[0] == '1') async_coro = 0;
    else if (coro && coro[0] == '0') async_coro = 0;
    else async_coro = 1;   // default: coroutine scheduler
}
if (async_coro) return wyn_spawn_async_coro(func, arg, file, line);
```

The **matching** main-thread drain selection lives at **`src/future.c:231-241`**
(`async_coro ? wyn_sched_pump_one() : pool_try_run_one()`); S1/S3 changes must keep
these two selections in agreement or main-thread await will deadlock.

---

## Baseline scenario table

| Scenario | Executor (default = coroutine sched) | Behavior | Wall time (measured) | Correctness | ASan | TSan (default / pool) |
|---|---|---|---|---|---|---|
| single `spawn`+`await`, int/string/struct | coroutine sched; main pumps | correct value returned for all 3 types | n/a | ✅ 4950 / hi-5 / 10,11 | clean | clean / clean |
| `await_all` over N=9 futures | coroutine sched | sum=285; **index order preserved** (results[i]↔futs[i], not completion order) | n/a | ✅ | clean | clean / clean |
| `parallel { }` join (int + string) | coroutine sched | all spawns join at `}`; bound vars hold VALUEs, visible after block | n/a | ✅ 60 / task-1 task-2 / 29 | clean | clean / clean |
| **awaited** `spawn`+`Time::sleep`, N=64×100ms | **coroutine sched (default)** | **COOPERATIVE — sleeps overlap** | **~111 ms** (≈ one sleep) | ✅ sum=6400 | clean | clean / clean |
| same, `WYN_ASYNC_POOL=1` | legacy thread pool | **BLOCKING — pool-thread-bound** (≈NCPU concurrent) | **~515 ms** (≈ ⌈64/12⌉×100) | ✅ sum=6400 | clean | clean / clean |
| fire-and-forget `spawn`+`Time::sleep`, N=64×100ms | coroutine sched | COOPERATIVE (contrast, always was) | **~104 ms** | ✅ 64 done | clean | clean / clean |
| nested spawn (spawned task spawns+awaits child) | coroutine sched | await-from-coroutine parks cooperatively; correct | n/a | ✅ 37 / 34 | clean | clean / clean |

Timing is non-deterministic and is NOT asserted in the test suite; the correctness
columns above ARE asserted by the `// EXPECT:` probes (see below). "n/a" wall-time
rows are compute-bound and finish in a few ms — not a meaningful timing signal.

### Key timing conclusion

For **awaited** `Time::sleep`, the CURRENT default (coroutine scheduler) is
**cooperative**: 64 concurrent 100 ms sleeps complete in ~111 ms (≈ one sleep),
not 64×100 ms. Forcing the legacy pool (`WYN_ASYNC_POOL=1`) serializes them to
~515 ms (≈ ⌈64/12 CPUs⌉ × 100 ms = 6×100), confirming the pool caps concurrency at
the worker-thread count while the coroutine scheduler does not. Fire-and-forget
`spawn` is cooperative on both paths (~104 ms), as it always has been.

---

## Pre-existing races / leaks

- **TSan:** none flagged. The full `make tsan-runtime-test` set (channels,
  parallel, parallel_timeout, spawn_await, spawn_parallel, spawn_typed_args,
  concurrent_strings, await_twice, select_arms) is **clean under BOTH executor
  configs** (default coroutine + `WYN_ASYNC_POOL=1`). The three new S0 probes that
  most exercise the coroutine-park / nested-await / cooperative-sleep paths
  (`test_await_baseline_nested`, `_sleep_concurrent`, `_single`) were additionally
  hand-run under TSan in both configs — clean.
- **ASan:** none flagged. `make asan-runtime-test` (representative set incl.
  `test_channels`, `test_parallel`, `test_await_twice`, the `await_all_*_results`
  regressions) is clean; the new S0 probes are ASan-clean too.
- **Leaks:** LSan leak-at-exit is **not gated** — it is unsupported on macOS
  (`detect_leaks is not supported on this platform`) and the Linux ASan CI job runs
  with `detect_leaks=0` by design (keeps signal high). A known, documented,
  bounded leak exists by design: a **named future that is never consumed leaks its
  ~40-byte slab slot** (`src/future.c:148-155`) — `future_get` memoizes rather than
  recycles so `await f` twice is safe; single-use consumers
  (`future_get_consume`, used by `await_all`/parallel joins) recycle and keep the
  constant-memory property. This is a pre-existing, accepted baseline condition,
  not an S0 regression.

**S0 baseline verdict: the awaited path is race-free and UAF-free under ASan+TSan
on both executor substrates today. S1/S2 must not regress this.**

---

## Reproduce

```bash
cd repos/wyn
make                                   # build compiler (0 warnings)

# Correctness probes (also run by run_bdd.sh — they live in tests/regression/):
./wyn run tests/regression/test_await_baseline_single.wyn
./wyn run tests/regression/test_await_baseline_await_all.wyn
./wyn run tests/regression/test_await_baseline_parallel.wyn
./wyn run tests/regression/test_await_baseline_nested.wyn
./wyn run tests/regression/test_await_baseline_sleep_concurrent.wyn

# Timing (non-deterministic; prints wall_ms for coroutine vs pool):
scripts/concurrency_timing_probe.sh

# Sanitizer baseline:
make asan-runtime-test
make tsan-runtime-test
```

### New files added in S0 (additive only)
- `tests/regression/test_await_baseline_single.wyn` — single await: int/string/struct
- `tests/regression/test_await_baseline_await_all.wyn` — await_all sum + index order
- `tests/regression/test_await_baseline_parallel.wyn` — parallel {} joins (int/string)
- `tests/regression/test_await_baseline_nested.wyn` — nested spawn (await-from-coroutine)
- `tests/regression/test_await_baseline_sleep_concurrent.wyn` — cooperative-sleep correctness
- `scripts/concurrency_timing_probe.sh` — timing harness (coroutine vs pool)
- `docs/CONCURRENCY_BASELINE.md` — this file
