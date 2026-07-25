# Transparent GPU dispatch — design and honest limitations

Status: **experimental, opt-in, cross-platform by design.** Metal backend
(macOS) verified; OpenCL backend (Linux/Windows) builds + falls back to CPU
verified, GPU-on-hardware correctness pending GPU CI. Branch:
`feat/gpu-crossplatform`.

This document describes what the `[gpu]` feature *actually does today* — not what
it could become. Anything aspirational is called out explicitly as "not
implemented" or listed under [Known limitations](#known-limitations).

## Cross-platform architecture

GPU acceleration is gated by the **TARGET** platform, not the compiler host.
Every backend implements one small C ABI (`src/gpu_backend.h`, six functions:
`wyn_gpu_available` / `should_dispatch` / `map_f32_begin` / `map_f32_run` /
`lock` / `unlock`). Codegen (`src/codegen_gpu.c`) is backend-neutral: at each
eligible `[float].map` site it emits **both** an MSL kernel string (Metal) and
an OpenCL-C kernel string, wrapped in `WYN_GPU_KERNEL(msl, ocl)` — a compile-time
selector (`src/wyn_runtime.h`) that hands the linked backend its own shader
language. The runtime shim `wyn_gpu_try_map_float` drives whichever backend is
linked; if none is (or the backend finds no device at run time) the **CPU
fallback always runs** — the invariant that makes results correct everywhere.

| Target            | Backend                | Shader   | Link                               |
|-------------------|------------------------|----------|------------------------------------|
| macOS             | `src/gpu_metal.m`      | MSL      | `-framework Metal` (host-built .o) |
| Linux / Windows   | `src/gpu_opencl.c`     | OpenCL-C | dlopen `libOpenCL` at run time     |
| ios/android/wasm  | none (yet)             | —        | CPU-only                           |

Backend selection lives in `wyn_gpu_link_flags_for(target, cc, …)` in
`src/main.c`, keyed off the target OS. Both the native build/run path and the
`wyn cross <target>` path call it, so **`wyn cross linux` from a Mac wires the
OpenCL backend into the Linux binary** (this was the parity gap the old
`#ifdef __APPLE__` host-gating left open).

### Why OpenCL for the portable path

OpenCL is a single C API present on most Linux GPU drivers (NVIDIA/AMD/Intel)
and on Windows, and — critically — it is **dlopen-able at run time**. So a Wyn
binary built with the OpenCL backend needs **no OpenCL SDK to link** (cross-
compile friendly) and **still runs on a box with no GPU/driver**: `dlopen` finds
no `libOpenCL` (or `clGetPlatformIDs` returns 0 devices) → `wyn_gpu_available()`
returns 0 → the CPU fallback runs. CUDA is NVIDIA-only; Vulkan compute needs
SPIR-V + far more code. OpenCL is the pragmatic first cross-platform backend.

The OpenCL backend needs the OpenCL **headers** (`CL/cl.h`) at build time for
types/enum values only — never the loader library. A minimal ABI-accurate
subset is vendored at `vendor/opencl/CL/cl.h` so the backend compiles on hosts
without the SDK (e.g. the macOS dev box when cross-building, or a bare Linux
build). On a Linux box with `opencl-headers` installed, dropping
`-Ivendor/opencl` uses the system header instead (the vendored subset is
compatible — verified to compile against the real `CL/cl.h`).

### Kill-switch

`WYN_GPU=0` in the environment forces the CPU path for **any** backend at run
time, no recompile — checked both in the runtime shim (short-circuits before any
device work) and inside each backend. This is in addition to the compile-time
`[gpu] enabled = false` opt-out.

### Building & running on Linux (for the orchestrator / GPU CI)

To exercise the OpenCL GPU path on real hardware (e.g. a Linux + NVIDIA T4 EC2
box), rsync this worktree over and:

```bash
# build tooling + OpenCL headers/loader (loader only needed to actually run on GPU)
sudo apt-get install -y build-essential opencl-headers ocl-icd-opencl-dev
# plus the vendor ICD for the GPU, e.g. NVIDIA: nvidia-opencl-dev / the driver's ICD
make                    # builds the wyn compiler natively (selects OpenCL backend for linux target)
cat > /tmp/g/wyn.toml <<'EOF'
[project]
name="g"
version="0.1.0"
[gpu]
enabled = true
float32 = true
EOF
# a large [float].map so the cost model dispatches (N >= 300k cached / cumulative tiers)
./wyn build /tmp/g/big.wyn
WYN_GPU_DEBUG=1 ./big          # expect "[wyn-gpu] n=… -> GPU" lines and correct output
clinfo                          # confirm a GPU device is visible to the ICD loader
```

`WYN_GPU_FORCE=1` bypasses the cost model to dispatch even small arrays (test
knob). Verify the GPU result matches the CPU result within float32 tolerance
(the suite's forced-dispatch check does this for the Metal path; the same check
should be run on the OpenCL GPU box).

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

- **OpenCL-on-GPU correctness/perf is NOT verified.** The macOS dev box has no
  non-Apple GPU or OpenCL device, so the OpenCL backend has only been verified to
  (a) compile (against both the vendored and the real system `CL/cl.h`) and
  (b) fall back to CPU cleanly when no device is present. That it computes
  *correctly and faster* on a real Linux/Windows GPU MUST be checked on GPU
  hardware (a Linux + GPU box / GPU-enabled CI) before it can be claimed working.
- **No CUDA/ROCm/Vulkan backend.** OpenCL is the only non-Metal backend.
- **ios/android/wasm have no GPU backend** — CPU-only there.
- **`[float].map` only.** No `filter`, `reduce`, `for` loops, or other methods.
- **4 arithmetic ops only** (`+ - * /`, unary `-`). No `%`, comparisons,
  transcendental functions, or conditionals in eligible bodies.
- **float32 precision** for the GPU path — lossy vs Wyn's float64 (hence the
  opt-in). No float64 GPU path exists (Metal has no `double`).
- **No int64 path.** MSL `long` could carry an exact `[int]` map, but it is *not
  implemented*. There are no `wyn_gpu_map_i64_*` entry points.
- **Runtime kernel compilation only.** No precompiled `.metallib`; the first use of
  each kernel pays the MSL compile cost.
- **Serialized global state.** Each backend uses process-global device/queue/
  buffer/kernel caches guarded by a pthread mutex the runtime shim holds across
  the whole dispatch. Concurrent maps (e.g. `parallel { }`) serialize (correct,
  not parallel). A per-job buffer pool for real GPU concurrency is future work.

## Files

| File | Role |
|------|------|
| `src/gpu_backend.h` | the stable 6-function C ABI every backend implements |
| `src/toml.h` / `src/toml.c` | `[gpu]` config (`enabled`, `float_enabled`) |
| `src/main.c` (`wyn_gpu_flag_from_toml`, `wyn_gpu_backend_for_target`, `wyn_gpu_link_flags_for`) | opt-in gate + **target-based** backend selection + lazy backend build (native + cross) |
| `src/codegen_gpu.c` | eligibility check + **dual** (MSL + OpenCL-C) kernel emission + dual-path codegen |
| `src/wyn_runtime.h` (`wyn_gpu_try_map_float`, `WYN_GPU_KERNEL`) | runtime dispatch shim / kernel selector / CPU-only stub / `WYN_GPU=0` kill-switch |
| `src/gpu_metal.m` | Metal backend (macOS): device init, cost model, kernel cache, dispatch |
| `src/gpu_opencl.c` | OpenCL backend (linux/windows): dlopen loader, device probe, program build+cache, buffer pack/unpack, cost model |
| `vendor/opencl/CL/cl.h` | minimal ABI-accurate OpenCL header subset (build without the SDK) |
| `benchmarks/gpu/bench_gpu_map.c` | standalone CPU-vs-GPU microbenchmark |
| `demos/gpu_map/` | runnable demo |
| `tests/gpu/run_gpu_test.sh` | the test suite described above |
