# Benchmarks

**The numbers live on one page: <https://wynlang.com/docs/guides/benchmarks>.**
This directory holds the *harness* that produces them, and nothing else.

There used to be a results table here as well. It reported v1.7.0 figures taken
on a different machine and had drifted so far from the published page that the
two contradicted each other by 5x on binary size (256 KB here vs ~50 KB
published) and disagreed on whether Wyn beat Go on `fib(35)`. Two lists of the
same measurement will keep diverging, and the stale one wins whenever a reader
finds it first - so this file no longer carries results. If a number is worth
quoting, quote it from the page.

## What is here

| Path | What it is |
|---|---|
| `harness/bench_exec.c` | `fork`/`exec`/`wait4` process timer. Median/min/max wall clock + the child's own CPU + peak RSS, as one line of JSON. POSIX (macOS/Linux). |
| `harness/run_bench.py` | Orchestrator: builds the fixtures with one or more compilers and re-measures every published row. |
| `harness/gen_scale_fixture.py` | Emits a realistic Wyn program of a requested size, for the compile-time scaling rows. |
| `harness/fx/*.wyn` | The fixtures the published rows are measured on. The in-process ones print their own `label us=N` timings. |
| `run.sh`, `bench.sh` | Older shell runners (compute / size / spawn / startup), kept because they are self-contained. |
| `http_load.sh` | HTTP req/s - the source of every published req/s figure. |
| `*.wyn`, `*.go` | Fixtures for `run.sh`. |

## Running

```bash
make                                          # the harness measures ./wyn
benchmarks/harness/run_bench.py               # re-measure every published row
benchmarks/harness/run_bench.py --quick       # smoke test the harness itself
benchmarks/harness/run_bench.py --scale-only  # just the compile-time table
benchmarks/harness/run_bench.py \
    cand=./wyn rel=/path/to/wyn-1.21.0/bin/wyn   # A/B two compilers

./benchmarks/run.sh                           # older runner: compute/size/spawn/startup
./benchmarks/http_load.sh                     # HTTP throughput (--quick for one config)
```

`run_bench.py` writes `benchmarks/harness/results/results.json` (gitignored) and
prints every row as it goes. It builds `bench_exec` on first use.

**None of this is a gate.** It measures the machine at least as much as the
compiler. Run it on an **idle** box - a parallel build or test run roughly halves
every result - and trust the **differential** between two compilers measured back
to back far more than any absolute value. `run_bench.py` prints a `max/min`
spread per row for exactly this reason: well above 1.0 means re-run before
quoting.

`http_load.sh` uses `ab` when installed and otherwise falls back to a bundled
Python load generator, whose numbers are *not* comparable to `ab`'s; the output
says which one ran. Correctness of the HTTP path under concurrent load is a
separate, always-on gate - `tests/errors/run_http_server_load_test.sh`, run by
`make test` - which asserts that every request completes and no fds leak rather
than asserting a rate, so it is not flaky on a shared CI box.

## Two traps these fixtures encode

Both of these produced a wrong published number before they were understood.
They are the reason the harness is committed rather than rebuilt from prose each
time.

### 1. A parallelism benchmark whose branches take identical arguments measures the C compiler, not parallelism

The C compiler treats a pure function as pure. Two calls with the same arguments
are one common subexpression, and a call whose result is discarded is dead code.

Measured on the C that `wyn build --release` generates, compiled at `-O2`
(arm64): a `main` containing **four** source-level `fib(35)` calls plus one
discarded `fib(35)` emits exactly **one** `bl _fib`. Change the calls to
`fib_off(35, 1)` … `fib_off(35, 4)` - same node count, distinct arguments - and
the same compiler emits **four**.

Re-verify it in a minute, on any machine:

```bash
./wyn build f.wyn --release --debug -o /tmp/f    # --debug keeps f.wyn.c
clang -O2 -I src -S -o /tmp/f.s f.wyn.c
awk '/^_wyn_main:/,/\.cfi_endproc/' /tmp/f.s | grep -c 'bl[[:space:]]*_fib'
```

So a "sequential baseline" written as four identical calls silently measures one
quarter of the work it claims to, and a working `parallel { }` then looks as
though it achieves nothing. Branches must either **sleep** (a call with side
effects cannot be folded) or **take distinct arguments**.
`harness/fx/conc_parallel.wyn` uses distinct offsets and says so at the top.

This only bites branches the compiler can see side by side. `parallel { }` and
`spawn` lower to a function pointer handed to `wyn_spawn_async_traced`, which is
opaque to the optimiser - it is the *baseline*, written inline, that collapses.
Which is the worse direction to be wrong in.

### 2. Discard the first run of a freshly built binary on macOS

macOS scans a new executable on first exec. The same 75 KB binary has been
observed taking **7.2s** on its first run and **0.16s** on every run after. That
one sample, averaged in, is larger than everything the benchmark is trying to
measure, and it once produced a confident, entirely false finding that
`wyn build` serialised awaited work.

`bench_exec` takes a mandatory `discard` count for this and the harness passes at
least 1 everywhere; the in-process path throws its first run away too.

## Fixture shape decides the compile-time answer

`wyn check` scales with the number of **declarations**, not with line count, and
mildly superlinearly at that. 5,000 lines of one repeated statement and 5,000
lines of realistic code differ by **3.8x**, measured back to back on one machine
(absolute figures: see the page). An earlier edition of the published table was
more than 2x too low for exactly this reason - the fixture, not the machine.

That is why the scaling fixtures are **generated, not checked in**:
`gen_scale_fixture.py` states the shape in one place. Each ~62-line unit
contributes a struct with an `impl` block, an enum with payload arms, a `match`
over it, an `Option`-returning lookup, a generic function and a driver, with
every symbol carrying the unit index so no two units share anything.

```bash
benchmarks/harness/gen_scale_fixture.py --lines 5065 -o /tmp/scale.wyn
```

It prints the unit and declaration counts it actually wrote, and `run_bench.py`
records them next to every timing. A unit is atomic, so an arbitrary line count
cannot be hit exactly - `--lines 1063` and `--lines 5065` reproduce the *shape*
of the published rows at that size, not the byte-identical original files, which
were never committed. Quote the declaration count with any number taken from
this, never the line count alone.
