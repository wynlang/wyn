# Transparent GPU dispatch — design and honest limitations

Status: **experimental, opt-in, macOS/Metal-only, not merged to main.**
Branch: `feat/gpu-dispatch-m2`.

This document describes what the `[gpu]` feature *actually does today* — not what
it could become. Anything aspirational is called out explicitly as "not
implemented" or listed under [Known limitations](#known-limitations).

## What it does

When a project's `wyn.toml` opts in (see [Opt-in](#opt-in-and-the-precision-contract)),
the compiler recognises an **eligible** `[float].map(lambda)` call site and emits a
*dual path*:

```c
// (schematic of the generated C at an eligible site)
({
    WynArray src = <object>;
    WynArray res;
    if (!wyn_gpu_try_map_float(src, "<MSL kernel>", &res))
        res = wyn_array_map_float(src, <the same lambda>);   // CPU fallback
    res;
})
```

At runtime a cost model (`wyn_gpu_should_dispatch`) decides per call whether the
array is large enough to be worth the GPU. If it says no — or if there is no Metal
device, or the kernel fails to compile — the CPU fallback runs. **The CPU fallback
is the exact same lambda**, so a program that never crosses the cost-model
threshold behaves identically to a non-GPU build.

The GPU backend (`src/gpu_metal.m`) is a small Objective-C file compiled to
`src/gpu_metal.o` and linked (with `-framework Metal -framework Foundation`) **only**
into programs whose generated C actually contains a dispatch site *and* whose
`wyn.toml` opted in. Everywhere else the `wyn_gpu_try_map_float` shim in
`wyn_runtime.h` is the CPU-only stub that always returns "no".

Measured on Apple Silicon (M3 Pro): a 20M-element `[float].map` dropped from
~326 ms (CPU) to ~100 ms (GPU, including pack/unpack) once the kernel is compiled.

## Opt-in and the precision contract

**This is the single most important correctness rule.** Metal has no `double`
type. Wyn's `float` is a C `double`. The GPU path therefore computes in **float32**,
which is *lossy* relative to the CPU's float64 semantics. For example, an identity
map of `16777217.0` (the first integer float32 cannot represent) returns
`16777216.0` on the GPU — a measured error of `1.0`.

Because this changes results, the GPU float path is **off by default** and requires
**two explicit keys**:

```toml
[gpu]
enabled = true     # master switch
float32 = true     # "I accept float32 precision for [float].map"
```

Both must be present and `true`. The gate lives in `wyn_gpu_flag_from_toml`
(`src/main.c`): it returns on only when `enabled && float_enabled`.

| `wyn.toml` state                         | float `[float].map` runs on |
|------------------------------------------|-----------------------------|
| no `wyn.toml`                            | **CPU** (exact float64)     |
| no `[gpu]` section                       | **CPU**                     |
| `enabled = false`                        | **CPU** (kill-switch)       |
| `enabled = true` only                    | **CPU** (float32 not opted in) |
| `float32 = true` only (no `enabled`)     | **CPU**                     |
| `enabled = true` **and** `float32 = true`| **GPU** (float32, lossy)    |

`float32` is the canonical spelling; `float` is accepted as an alias.

When the opt-in is absent, codegen for the GPU path is bypassed entirely and the
emitted C is byte-identical to a non-GPU build. This is proven two ways: the
golden-C snapshot suite (which has no `[gpu]` toml) and the flag-off equivalence
check in `tests/gpu/run_gpu_test.sh`.

## Eligibility rules

`gpu_lambda_eligible` / `gpu_expr_eligible` (`src/codegen_gpu.c`) accept a lambda
**only** when *all* of these hold — anything else falls through to CPU:

- exactly **one parameter**, and a **single expression body** (no statements);
- the body is **pure arithmetic** over: int/float literals, the parameter itself,
  binary `+ - * /`, and unary minus;
- **every identifier in the body is the parameter.** This one rule rules out
  captured variables, function calls, method calls, string ops, allocation, and
  I/O — none of which can appear in an eligible body.

Additionally the result element type must be `float` (a lambda the checker typed as
returning `int` routes to a different CPU helper and is rejected here).

Verified rejections (each falls back to CPU with correct results — see the test
suite): captured free variable, method call in body (`x.abs()`), multi-statement
body.

## Cost model and thresholds

Kernels are compiled **at runtime** on first use (`newLibraryWithSource`), which
costs ~14 ms warm / ~120 ms for the first compile in a process. Dispatch is only
worthwhile when the per-element saving (~5 ns/elem, measured) amortizes that cost.
Constants in `src/gpu_metal.m`:

| Constant                     | Value  | Meaning |
|------------------------------|--------|---------|
| `WYN_GPU_MIN_N_CACHED`       | 300 000 | Once a kernel's pipeline is cached, dispatch for `N ≥` this. Below it the GPU never wins. |
| `WYN_GPU_CUM_N_KERNEL_COLD`  | 8 000 000 | Cumulative N across calls before compiling a *later* kernel. |
| `WYN_GPU_CUM_N_FIRST_EVER`   | 40 000 000 | Cumulative N before paying the first-ever (cold-service) compile. |

Compilation is treated as an investment: each eligible call adds its N to the
kernel's pending counter, and the kernel is compiled once cumulative saving covers
the compile cost. So a hot loop over 20M elements compiles on its second pass; a
single 60M call compiles immediately; small-N programs never pay compilation.

`WYN_GPU_DEBUG=1` traces every dispatch decision to stderr:
`[wyn-gpu] n=<N> cached=<0|1> -> GPU|cpu`.

`WYN_GPU_FORCE=1` bypasses the cost model and dispatches any eligible map to the
GPU (subject to a usable Metal device). It is a **test/debug knob only** — it never
changes results, only whether the GPU is used, and it lets the float32 path be
exercised on small arrays.

## Kill-switch

`[gpu] enabled = false` turns the whole feature off regardless of `float32`. No
`wyn.toml` and no `[gpu]` section are equivalent to off. All three are covered by
the kill-switch tests.

## Tests

`tests/gpu/run_gpu_test.sh` (wired into `make test`) covers:

1. flag-off codegen/behavior equivalence (0 GPU sites, output matches);
2. the float32 opt-in gate (enabled-only and float32-only both stay CPU; both keys
   emit a dispatch site);
3. GPU correctness within float32 tolerance under `WYN_GPU_FORCE=1`, plus a
   visible demonstration of the precision contract (CPU residual `0.0`, GPU residual
   `-1.0` for `16777217`);
4. the eligibility gate rejecting captures / method calls / multi-statement bodies
   (CPU fallback, correct results);
5. the kill-switch (no toml / `enabled=false` / enabled-only all CPU-only).

On non-macOS the GPU-run checks are skipped; the codegen-site and gate assertions
still hold (they inspect emitted C, which is platform-independent).

A standalone CPU-vs-GPU microbenchmark lives at `benchmarks/gpu/bench_gpu_map.c`
(build instructions in its header).

## Known limitations

These are **out of scope** for this branch and must not be represented as working:

- **macOS / Metal only.** No CUDA/ROCm/Vulkan/OpenCL backend. On every other
  platform the runtime stub answers "no" and everything runs on CPU.
- **`[float].map` only.** No `filter`, `reduce`, `for` loops, or other methods.
- **4 arithmetic ops only** (`+ - * /`, unary `-`). No `%`, comparisons,
  transcendental functions, or conditionals in eligible bodies.
- **float32 precision** for the GPU path — lossy vs Wyn's float64 (hence the
  opt-in). No float64 GPU path exists (Metal has no `double`).
- **No int64 path.** MSL `long` could carry an exact `[int]` map, but it is *not
  implemented*. There are no `wyn_gpu_map_i64_*` entry points.
- **Runtime kernel compilation only.** No precompiled `.metallib`; the first use of
  each kernel pays the MSL compile cost.
- **Single-threaded global state.** `src/gpu_metal.m` uses static device/queue/
  buffer/pipeline caches and is **not thread-safe**. Concurrent GPU dispatch from
  multiple threads (e.g. `parallel { }`) is unsafe. A per-thread or pooled job
  context is future work.

## Files

| File | Role |
|------|------|
| `src/toml.h` / `src/toml.c` | `[gpu]` config (`enabled`, `float_enabled`) |
| `src/main.c` (`wyn_gpu_flag_from_toml`, `wyn_gpu_link_flags`) | opt-in gate + lazy Metal link |
| `src/codegen_gpu.c` | eligibility check + MSL kernel emission + dual-path codegen |
| `src/wyn_runtime.h` (`wyn_gpu_try_map_float`) | runtime dispatch shim / CPU-only stub |
| `src/gpu_metal.m` | Metal backend: device init, cost model, kernel cache, dispatch |
| `benchmarks/gpu/bench_gpu_map.c` | standalone CPU-vs-GPU microbenchmark |
| `demos/gpu_map/` | runnable demo |
| `tests/gpu/run_gpu_test.sh` | the test suite described above |
