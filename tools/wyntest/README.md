# wyntest — the Wyn test runner, written in Wyn

`wyntest.wyn` is a from-scratch reimplementation of `tests/run_bdd.sh` **in Wyn
itself** — dogfooding the language on a real, load-bearing tool (our own test
suite). It is an *additive* artifact: `run_bdd.sh` remains the source of truth for
CI; wyntest is a candidate future replacement.

## Run it

From the repo root (needs the built `./wyn`):

```bash
./wyn run tools/wyntest/wyntest.wyn
echo $?          # exit code == number of failing tests (0 = all pass)

WYN_TEST_JOBS=8 ./wyn run tools/wyntest/wyntest.wyn   # cap concurrency
```

## What it does (matches run_bdd.sh)

- Globs `tests/expect/*.wyn` + `tests/regression/*.wyn`.
- Reads each file, parses the `// EXPECT:` lines (SKIPs files with none).
- Builds each test to a **unique** path in a per-run `mktemp -d` scratch dir,
  verifies the binary is present+executable (a real compiler diagnostic → an
  explicit `BUILD FAILED`, never a silent empty-output pass), retrying only
  transient failures (resource-exhaustion / empty-diagnostic).
- Runs the binary, filters compiler-noise lines (`Building`/`Built`/`Compiled
  in`/`Warning:`), compares the first N output lines to the N expected lines.
- Prints `  ✓ name` / `  ✗ name`, per-failure `expected:`/`actual:`, and a final
  `Results: N pass, M fail`.
- Cleans the stray `<src>.wyn.c` that `wyn build -o` drops next to each source.
- Exits nonzero (= fail count) so CI can gate on it.

## Concurrency

Per-test build+run runs on the **thread-pool path**: `await_all` over `spawn`ed
workers, in **bounded batches** of `~2 * ncpu` (override with `$WYN_TEST_JOBS`) to
avoid the `posix_spawn` EAGAIN fork-storm the shell runner hit on constrained
machines. The work is subprocess/CPU-bound (each test forks `wyn build`→clang), so
this is where the real parallelism win is — the language's coroutine speed is
irrelevant to it.

## Verdict parity & honest timing (12-core mac, 24 jobs)

Both runners report **192 pass, 0 fail** with an **identical pass-name set**
(197 files discovered, 5 SKIPped for having no `// EXPECT:`). Two clean back-to-back
samples each (no contention):

| Runner            | Wall s1 | Wall s2 | User CPU | Sys CPU |
|-------------------|---------|---------|----------|---------|
| wyntest (Wyn)     | 2:45.6  | 2:50.5  | ~71–73s  | ~62–64s |
| run_bdd.sh (bash) | 3:10.5  | 3:07.2  | ~72s     | ~55s    |

**User CPU is ~identical (~71–73s across both runners)** because the compute *is*
the same `wyn build`→clang subprocesses regardless of the runner's language. A
single `wyn build` is ~0.4–0.5s, so ~197 tests carry ~90–130s of unavoidable
compiler/clang cost that neither runner can avoid. wyntest is consistently ~20s
faster in *wall* time — but that gap is scheduling/batch-draining efficiency, not
language speed. **Do not read it as "Wyn is faster than bash":** the language the
runner is written in is nearly irrelevant here; the workload is dominated by the
child `wyn build` processes, and parallel scheduling is the only lever.

## Known language limitation found (feeds the roadmap)

`await_all(futures)` types its result elements as `int` regardless of the spawned
function's real return type, so string/struct methods on the results fail to
type-check. Minimal repro: `tools/wyntest/repro_await_all_type.wyn`
(`./wyn check` it). A *single* `spawn`+`await` of a string/struct-returning fn
type-checks fine — the defect is specific to arrays-of-futures / `await_all`.

Workaround used here: each worker does its full build+run+compare and writes its
verdict to a per-index result file; `await_all`'s int return is used only as a
join barrier. If the type were preserved, workers could return a rich value
directly and skip the result-file round-trip.
