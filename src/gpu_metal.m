// gpu_metal.m - GPU map spike: Metal compute backend (macOS only).
//
// Shape mirrors src/wyn_webview.m: a small Objective-C file compiled to
// src/gpu_metal.o and linked (with -framework Metal -framework Foundation)
// only into programs whose generated C actually references it. Everything
// here is plain-C ABI so the generated C and wyn_runtime.h can call it.
//
// Scope and honest limitations (see docs/GPU_DESIGN.md):
//   - Kernels are compiled AT RUNTIME with newLibraryWithSource. First use of
//     a kernel pays device init + MSL compile (measured ~8-40 ms). A future
//     path could precompile to a .metallib at wyn-build time - NOT done here.
//   - Metal has NO double type (probed on Apple Silicon: "'double' is not
//     supported in Metal"). Wyn float is a C double, so this path computes in
//     float32. That is a semantic difference (precision), not just a perf
//     trade - hence the explicit `[gpu] float32 = true` opt-in in wyn.toml.
//   - float32 [float].map ONLY. There is no int64 path: MSL `long` could carry
//     an exact [int] map, but it is NOT implemented (noted in KNOWN
//     LIMITATIONS). This file exposes float32 entry points only.
//   - Single in-flight job, not thread-safe (static state). Concurrent GPU
//     dispatches from multiple threads are unsafe; a per-thread or pooled job
//     context is future work, out of scope here.
//
// API (called from wyn_runtime.h's dispatch wrapper and the benchmark):
//   wyn_gpu_available()                  -> 1 if a Metal device exists
//   wyn_gpu_should_dispatch(n, src)      -> 1 if the cost model wants the GPU
//   wyn_gpu_map_f32_begin(n, &in_ptr)    -> 0 ok; caller packs into in_ptr
//   wyn_gpu_map_f32_run(n, src, &out)    -> 0 ok; result readable at out
//   (buffers are cached and reused across calls; nothing to free per call)

#ifdef __APPLE__

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include <string.h>

static id<MTLDevice> _gpu_dev = nil;
static id<MTLCommandQueue> _gpu_queue = nil;
static int _gpu_init_state = 0;   // 0 = not tried, 1 = ok, -1 = unavailable

// Pipeline cache: kernel source string -> compiled pipeline state. Map sites
// pass string literals, so pointer identity usually hits; strcmp is the
// fallback (e.g. the same lambda emitted in two translation units).
#define WYN_GPU_PSO_CACHE 16
static struct { const char* src; void* pso; } _gpu_psos[WYN_GPU_PSO_CACHE];
static int _gpu_pso_count = 0;

// Cached shared-storage buffers (float32), grown as needed and reused across
// calls.
static id<MTLBuffer> _gpu_in_buf = nil;
static id<MTLBuffer> _gpu_out_buf = nil;
static size_t _gpu_buf_cap = 0;   // capacity in bytes

static int _gpu_init(void) {
    if (_gpu_init_state != 0) return _gpu_init_state;
    @autoreleasepool {
        _gpu_dev = MTLCreateSystemDefaultDevice();
        if (!_gpu_dev) { _gpu_init_state = -1; return -1; }
        _gpu_queue = [_gpu_dev newCommandQueue];
        if (!_gpu_queue) { _gpu_dev = nil; _gpu_init_state = -1; return -1; }
    }
    _gpu_init_state = 1;
    return 1;
}

int wyn_gpu_available(void) {
    return _gpu_init() == 1;
}

// Cost model. Constants measured on this machine (M3 Pro, macOS, Metal 4,
// 2026-07; see docs/GPU_DESIGN.md for the raw benchmark tables):
//   - steady state (kernel pipeline cached): GPU beats the existing CPU map
//     loop from roughly N = 250K-300K floats up (measured crossover between
//     200K and 300K for both a 2-flop and a 20-flop lambda). Per-element
//     saving above that is ~5 ns (CPU ~6.4 ns/elem for a simple lambda vs
//     GPU ~1.3 ns/elem including pack/unpack).
//   - a kernel's FIRST runtime compile costs ~14 ms once the Metal compiler
//     service is warm; the FIRST compile in the process pays the service
//     spinup too, ~120 ms total.
// Compilation is an INVESTMENT amortized across calls: each eligible call
// adds its N to the kernel's cumulative counter, and we compile once the
// cumulative saving (~5 ns x N) covers the compile cost. 14 ms / 5 ns ~ 3M
// elements; 120 ms / 5 ns ~ 24M. Rounded up to be conservative:
//   - first-ever compile in the process: cumulative N >= 40M
//   - later kernels:                     cumulative N >= 8M
// A single 60M-element call compiles immediately; a hot loop at 20M compiles
// on its second call; small-N programs never pay compilation at all.
// Production note: precompiled .metallib kernels remove both cold tiers -
// the cached threshold becomes the only one.
#define WYN_GPU_MIN_N_CACHED        300000
#define WYN_GPU_CUM_N_KERNEL_COLD   8000000
#define WYN_GPU_CUM_N_FIRST_EVER    40000000

static int _gpu_compiled_any = 0;

// Cumulative eligible-N per not-yet-compiled kernel (same identity rules as
// the pipeline cache: pointer first, strcmp fallback).
#define WYN_GPU_PENDING_CACHE 16
static struct { const char* src; long long cum_n; } _gpu_pending[WYN_GPU_PENDING_CACHE];
static int _gpu_pending_count = 0;

static int _gpu_kernel_cached(const char* src) {
    for (int i = 0; i < _gpu_pso_count; i++) {
        if (_gpu_psos[i].src == src || strcmp(_gpu_psos[i].src, src) == 0) return 1;
    }
    return 0;
}

static long long _gpu_pending_add(const char* src, int n) {
    for (int i = 0; i < _gpu_pending_count; i++) {
        if (_gpu_pending[i].src == src || strcmp(_gpu_pending[i].src, src) == 0) {
            _gpu_pending[i].cum_n += n;
            return _gpu_pending[i].cum_n;
        }
    }
    if (_gpu_pending_count < WYN_GPU_PENDING_CACHE) {
        _gpu_pending[_gpu_pending_count].src = src;
        _gpu_pending[_gpu_pending_count].cum_n = n;
        _gpu_pending_count++;
        return n;
    }
    return n;
}

int wyn_gpu_should_dispatch(int n, const char* kernel_src) {
    if (_gpu_init() != 1) return 0;
    // WYN_GPU_FORCE=1 bypasses the cost model and dispatches ANY eligible map
    // to the GPU (subject to a usable Metal device). It exists for tests and
    // debugging - it lets the float32 path be exercised on small arrays so
    // correctness can be checked without allocating tens of millions of
    // elements. It never changes results, only whether the GPU is used.
    if (getenv("WYN_GPU_FORCE") && n > 0) {
        if (getenv("WYN_GPU_DEBUG"))
            fprintf(stderr, "[wyn-gpu] n=%d FORCE -> GPU\n", n);
        return 1;
    }
    int yes;
    int cached = _gpu_kernel_cached(kernel_src);
    if (cached) {
        yes = n >= WYN_GPU_MIN_N_CACHED;
    } else if (n < WYN_GPU_MIN_N_CACHED) {
        yes = 0;   // too small to ever win; do not accumulate
    } else {
        long long cum = _gpu_pending_add(kernel_src, n);
        yes = cum >= (_gpu_compiled_any ? WYN_GPU_CUM_N_KERNEL_COLD
                                        : WYN_GPU_CUM_N_FIRST_EVER);
    }
    // WYN_GPU_DEBUG=1 traces cost-model decisions to stderr (spike aid).
    if (getenv("WYN_GPU_DEBUG"))
        fprintf(stderr, "[wyn-gpu] n=%d cached=%d -> %s\n", n, cached, yes ? "GPU" : "cpu");
    return yes;
}

static id<MTLComputePipelineState> _gpu_pipeline_for(const char* src) {
    for (int i = 0; i < _gpu_pso_count; i++) {
        if (_gpu_psos[i].src == src || strcmp(_gpu_psos[i].src, src) == 0) {
            return (__bridge id<MTLComputePipelineState>)_gpu_psos[i].pso;
        }
    }
    @autoreleasepool {
        NSError* err = nil;
        NSString* nssrc = [NSString stringWithUTF8String:src];
        id<MTLLibrary> lib = [_gpu_dev newLibraryWithSource:nssrc options:nil error:&err];
        if (!lib) return nil;
        id<MTLFunction> fn = [lib newFunctionWithName:@"wyn_map_kernel"];
        if (!fn) return nil;
        id<MTLComputePipelineState> pso = [_gpu_dev newComputePipelineStateWithFunction:fn error:&err];
        if (!pso) return nil;
        _gpu_compiled_any = 1;
        if (_gpu_pso_count < WYN_GPU_PSO_CACHE) {
            _gpu_psos[_gpu_pso_count].src = src;
            _gpu_psos[_gpu_pso_count].pso = (__bridge_retained void*)pso;
            _gpu_pso_count++;
        }
        return pso;
    }
}

static int _gpu_ensure_bufs(int n) {
    if (n <= _gpu_buf_cap && _gpu_in_buf && _gpu_out_buf) return 0;
    int cap = n < 4096 ? 4096 : n;
    _gpu_in_buf = [_gpu_dev newBufferWithLength:(NSUInteger)cap * sizeof(float)
                                        options:MTLResourceStorageModeShared];
    _gpu_out_buf = [_gpu_dev newBufferWithLength:(NSUInteger)cap * sizeof(float)
                                         options:MTLResourceStorageModeShared];
    if (!_gpu_in_buf || !_gpu_out_buf) { _gpu_buf_cap = 0; return -1; }
    _gpu_buf_cap = cap;
    return 0;
}

// Phase 1: get a pointer to the (unified-memory) input buffer so the caller
// can pack element data straight into it - no intermediate staging copy.
int wyn_gpu_map_f32_begin(int n, float** in_ptr) {
    if (n <= 0 || !in_ptr) return -1;
    if (_gpu_init() != 1) return -1;
    if (_gpu_ensure_bufs(n) != 0) return -1;
    *in_ptr = (float*)[_gpu_in_buf contents];
    return 0;
}

// Phase 2: compile-or-fetch the kernel, dispatch n threads, wait, and hand
// back a pointer to the results (valid until the next begin/run).
int wyn_gpu_map_f32_run(int n, const char* kernel_src, float** out_ptr) {
    if (n <= 0 || !kernel_src || !out_ptr) return -1;
    if (_gpu_init() != 1) return -1;
    if (n > _gpu_buf_cap) return -1;   // begin() not called for this size

    id<MTLComputePipelineState> pso = _gpu_pipeline_for(kernel_src);
    if (!pso) return -1;

    @autoreleasepool {
        id<MTLCommandBuffer> cmd = [_gpu_queue commandBuffer];
        id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
        [enc setComputePipelineState:pso];
        [enc setBuffer:_gpu_in_buf offset:0 atIndex:0];
        [enc setBuffer:_gpu_out_buf offset:0 atIndex:1];
        NSUInteger tg = pso.maxTotalThreadsPerThreadgroup;
        if (tg > 256) tg = 256;
        [enc dispatchThreads:MTLSizeMake((NSUInteger)n, 1, 1)
             threadsPerThreadgroup:MTLSizeMake(tg, 1, 1)];
        [enc endEncoding];
        [cmd commit];
        [cmd waitUntilCompleted];
        if (cmd.status == MTLCommandBufferStatusError) return -1;
    }
    *out_ptr = (float*)[_gpu_out_buf contents];
    return 0;
}

#endif // __APPLE__
