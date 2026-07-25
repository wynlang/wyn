// gpu_backend.h - stable C ABI implemented by every Wyn GPU compute backend.
//
// Wyn's transparent GPU dispatch (see docs/GPU_DESIGN.md) is backend-agnostic:
// codegen emits ONE call - `wyn_gpu_try_map_float(src, "<kernel>", &res)` - and
// the runtime shim in wyn_runtime.h drives whichever backend was linked in
// through the six functions declared here. The CPU fallback is ALWAYS compiled
// in at the call site, so if no backend is linked (or a backend's
// wyn_gpu_available() returns 0 at run time), the program computes on the CPU
// and is correct everywhere - the invariant this whole feature is built around.
//
// A backend is one translation unit implementing this ABI:
//   - src/gpu_metal.m   (Metal)  -> macOS targets            [-DWYN_GPU_METAL]
//   - src/gpu_opencl.c  (OpenCL) -> linux/windows targets    [-DWYN_GPU_OPENCL]
//   - (future) CUDA / Vulkan     -> same six functions
// EXACTLY ONE backend is compiled+linked per built program, chosen by the
// *target* (native or `wyn cross <target>`), NOT by the compiler host - see
// wyn_gpu_link_flags in src/main.c. Each backend #defines its own
// WYN_GPU_<NAME> macro on the compile line; the runtime shim keys on
// `defined(WYN_GPU_METAL) || defined(WYN_GPU_OPENCL)` to compile the real path.
//
// Thread-safety contract: the backend keeps process-global state (device,
// queue/context, kernel/pipeline cache, shared in/out buffers). The runtime
// shim holds wyn_gpu_lock()/unlock() across the whole begin -> pack -> run ->
// unpack critical section, so concurrent maps (e.g. from `parallel { }`)
// serialize instead of racing. Every backend implements these as a plain
// pthread mutex.
//
// Kernel source: the backend receives a kernel SOURCE STRING keyed by pointer
// identity (string literals from codegen) with a strcmp fallback, and caches
// the compiled program/pipeline per source. Because different backends consume
// different shader languages, codegen emits a dispatch-time selector that hands
// each backend ITS OWN kernel string (MSL for Metal, OpenCL-C for OpenCL) via
// the WYN_GPU_KERNEL(msl, ocl) macro below. See src/codegen_gpu.c.
//
// Precision: the map path computes in float32 (Metal has no double; OpenCL
// double needs an optional extension). Wyn's float is a C double, so this is a
// deliberate, opted-into precision loss gated by `[gpu] float32 = true`.

#ifndef WYN_GPU_BACKEND_H
#define WYN_GPU_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

// 1. Availability probe. Returns 1 if a usable GPU device exists AT RUNTIME
//    (Metal: MTLCreateSystemDefaultDevice; OpenCL: dlopen libOpenCL +
//    clGetPlatformIDs/clGetDeviceIDs found a device). Returns 0 otherwise -
//    e.g. a Linux binary built with the OpenCL backend but run on a box with no
//    GPU/driver: available()==0 -> the runtime shim takes the CPU path. Cheap
//    to call repeatedly (result is cached after the first probe).
int  wyn_gpu_available(void);

// 2. Cost model. Given the element count `n` and the kernel source string,
//    returns 1 if this map is worth dispatching to the GPU, 0 to run on the
//    CPU. Backends mirror the same threshold shape (a cached-kernel minimum N
//    plus cumulative-N compile-amortization tiers). Honors WYN_GPU_FORCE=1
//    (force dispatch, test knob) and returns 0 when no device is available.
int  wyn_gpu_should_dispatch(int n, const char* kernel_src);

// 3. Begin: ensure device init + input buffer capacity for `n` float32
//    elements, and hand back a pointer to the (host-visible / mapped) input
//    buffer so the caller packs element data straight in - no staging copy.
//    Returns 0 on success (*in_ptr set), non-zero on failure (caller -> CPU).
int  wyn_gpu_map_f32_begin(int n, float** in_ptr);

// 4. Run: compile-or-fetch the kernel for `kernel_src`, dispatch `n` work
//    items, wait for completion, and hand back a pointer to the results in the
//    output buffer (valid until the next begin/run). Returns 0 on success
//    (*out_ptr set), non-zero on failure (caller -> CPU). Must be preceded by a
//    successful begin for the same `n`.
int  wyn_gpu_map_f32_run(int n, const char* kernel_src, float** out_ptr);

// 5/6. Serialize access to the backend's process-global state. The runtime
//    shim locks across the whole begin/pack/run/unpack sequence.
void wyn_gpu_lock(void);
void wyn_gpu_unlock(void);

#ifdef __cplusplus
}
#endif

#endif // WYN_GPU_BACKEND_H
