// gpu_opencl.c - GPU compute backend via OpenCL, for linux/windows targets.
//
// Implements the same six-function C ABI as src/gpu_metal.m (see
// src/gpu_backend.h), so Wyn's backend-agnostic codegen and the runtime shim in
// wyn_runtime.h drive it identically. This is the portable cross-platform path:
// OpenCL is a single C API present on most Linux GPU drivers (NVIDIA/AMD/Intel)
// and Windows, and - crucially - we load it with dlopen AT RUNTIME rather than
// link-time. That means:
//   - No OpenCL SDK is needed to LINK a Wyn program (cross-compile friendly).
//   - A binary built with this backend still RUNS on a box with no GPU/driver:
//     dlopen finds no libOpenCL (or clGetPlatformIDs returns 0 devices) ->
//     wyn_gpu_available() returns 0 -> the runtime shim takes the CPU fallback.
// This is the invariant: correct everywhere, GPU-accelerated where a device
// exists at run time.
//
// COMPILE GATING: this file is compiled ONLY when the target is linux/windows
// (wyn_gpu_link_flags in src/main.c compiles it with -DWYN_GPU_OPENCL). The
// whole body is under `#ifdef WYN_GPU_OPENCL`, so it is a no-op TU on any other
// target - in particular the macOS dev box, which has no CL headers, never
// compiles the body.
//
// BUILD DEPENDENCY: needs the OpenCL headers (CL/cl.h) for types and enum
// constants only - NOT the loader library, because we dlopen it. On Debian/
// Ubuntu: `apt-get install opencl-headers`. We do not call any CL function
// directly; every entry point is resolved via dlsym through our own function
// pointer typedefs, so there is no link-time dependency on -lOpenCL and no
// deprecation warnings from clCreateCommandQueue.
//
// HONEST STATUS: the OpenCL kernel path's correctness+speed ON A REAL non-Apple
// GPU is NOT verified on the macOS dev box (no OpenCL device here). It must be
// checked on GPU hardware (a Linux box with a GPU + driver). What IS verified
// here: this file compiles under glibc gcc with opencl-headers, and a program
// built with it falls back to CPU cleanly when no device is present.
//
// Precision: computes in float32 (Wyn float is a C double), matching the Metal
// path - the deliberate, opted-into loss gated by `[gpu] float32 = true`.

#ifdef WYN_GPU_OPENCL

#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "gpu_backend.h"   // the 6-function C ABI this file implements

// Portable dynamic-loader shim: dlopen/dlsym on POSIX, LoadLibrary/GetProcAddress
// on Windows. Either way the OpenCL loader is resolved AT RUN TIME, so a Wyn
// binary needs no OpenCL SDK at link time and still runs where none is present.
#ifdef _WIN32
#  include <windows.h>
   typedef HMODULE wyn_dl_handle;
#  define WYN_DLOPEN(name)     ((wyn_dl_handle)LoadLibraryA(name))
#  define WYN_DLSYM(h, name)   ((void*)GetProcAddress((HMODULE)(h), (name)))
#else
#  include <dlfcn.h>
   typedef void* wyn_dl_handle;
#  define WYN_DLOPEN(name)     dlopen((name), RTLD_NOW | RTLD_LOCAL)
#  define WYN_DLSYM(h, name)   dlsym((h), (name))
#endif

// ------------------------------------------------------------------ dlopen ---
// We resolve every OpenCL entry point at runtime. Function-pointer typedefs
// mirror the CL API signatures (types come from <CL/cl.h>). We deliberately do
// NOT declare/link the real symbols so a Wyn binary needs no libOpenCL at link
// time and still runs where the loader is absent.
typedef cl_int      (*fn_clGetPlatformIDs)(cl_uint, cl_platform_id*, cl_uint*);
typedef cl_int      (*fn_clGetDeviceIDs)(cl_platform_id, cl_device_type, cl_uint, cl_device_id*, cl_uint*);
typedef cl_context  (*fn_clCreateContext)(const cl_context_properties*, cl_uint, const cl_device_id*, void (*)(const char*, const void*, size_t, void*), void*, cl_int*);
typedef cl_command_queue (*fn_clCreateCommandQueue)(cl_context, cl_device_id, cl_command_queue_properties, cl_int*);
typedef cl_program  (*fn_clCreateProgramWithSource)(cl_context, cl_uint, const char**, const size_t*, cl_int*);
typedef cl_int      (*fn_clBuildProgram)(cl_program, cl_uint, const cl_device_id*, const char*, void (*)(cl_program, void*), void*);
typedef cl_int      (*fn_clGetProgramBuildInfo)(cl_program, cl_device_id, cl_program_build_info, size_t, void*, size_t*);
typedef cl_kernel   (*fn_clCreateKernel)(cl_program, const char*, cl_int*);
typedef cl_mem      (*fn_clCreateBuffer)(cl_context, cl_mem_flags, size_t, void*, cl_int*);
typedef cl_int      (*fn_clSetKernelArg)(cl_kernel, cl_uint, size_t, const void*);
typedef cl_int      (*fn_clEnqueueWriteBuffer)(cl_command_queue, cl_mem, cl_bool, size_t, size_t, const void*, cl_uint, const cl_event*, cl_event*);
typedef cl_int      (*fn_clEnqueueReadBuffer)(cl_command_queue, cl_mem, cl_bool, size_t, size_t, void*, cl_uint, const cl_event*, cl_event*);
typedef cl_int      (*fn_clEnqueueNDRangeKernel)(cl_command_queue, cl_kernel, cl_uint, const size_t*, const size_t*, const size_t*, cl_uint, const cl_event*, cl_event*);
typedef cl_int      (*fn_clFinish)(cl_command_queue);
typedef cl_int      (*fn_clReleaseMemObject)(cl_mem);

static struct {
    fn_clGetPlatformIDs          GetPlatformIDs;
    fn_clGetDeviceIDs            GetDeviceIDs;
    fn_clCreateContext           CreateContext;
    fn_clCreateCommandQueue      CreateCommandQueue;
    fn_clCreateProgramWithSource CreateProgramWithSource;
    fn_clBuildProgram            BuildProgram;
    fn_clGetProgramBuildInfo     GetProgramBuildInfo;
    fn_clCreateKernel            CreateKernel;
    fn_clCreateBuffer            CreateBuffer;
    fn_clSetKernelArg            SetKernelArg;
    fn_clEnqueueWriteBuffer      EnqueueWriteBuffer;
    fn_clEnqueueReadBuffer       EnqueueReadBuffer;
    fn_clEnqueueNDRangeKernel    EnqueueNDRangeKernel;
    fn_clFinish                  Finish;
    fn_clReleaseMemObject        ReleaseMemObject;
} cl;

// ------------------------------------------------------------ thread-safety ---
// All state below (loader handle, device/context/queue, kernel cache, shared
// buffers) is process-global and reused across calls. The runtime shim holds
// this lock across the whole begin->pack->run->unpack critical section, so
// concurrent maps serialize instead of racing (same contract as gpu_metal.m).
static pthread_mutex_t _gpu_lock = PTHREAD_MUTEX_INITIALIZER;
void wyn_gpu_lock(void)   { pthread_mutex_lock(&_gpu_lock); }
void wyn_gpu_unlock(void) { pthread_mutex_unlock(&_gpu_lock); }

// ------------------------------------------------------------- device init ---
static wyn_dl_handle     _cl_handle = NULL;
static cl_platform_id    _cl_platform = NULL;
static cl_device_id      _cl_device = NULL;
static cl_context        _cl_context = NULL;
static cl_command_queue  _cl_queue = NULL;
static int _gpu_init_state = 0;   // 0 = not tried, 1 = ok, -1 = unavailable

// Resolve one symbol; returns 0 on failure so init can bail to CPU cleanly.
static int _cl_sym(void** slot, const char* name) {
    *slot = WYN_DLSYM(_cl_handle, name);
    return *slot != NULL;
}

static int _cl_load_lib(void) {
    // Try the common loader names across OSes/drivers. If none loads (no GPU
    // driver installed), we fall back to CPU - the whole point of dlopen.
    const char* names[] = { "libOpenCL.so.1", "libOpenCL.so", "OpenCL.dll",
                            "libOpenCL.dylib", NULL };
    for (int i = 0; names[i]; i++) {
        _cl_handle = WYN_DLOPEN(names[i]);
        if (_cl_handle) break;
    }
    if (!_cl_handle) return 0;
    int ok = 1;
    ok &= _cl_sym((void**)&cl.GetPlatformIDs,          "clGetPlatformIDs");
    ok &= _cl_sym((void**)&cl.GetDeviceIDs,            "clGetDeviceIDs");
    ok &= _cl_sym((void**)&cl.CreateContext,           "clCreateContext");
    ok &= _cl_sym((void**)&cl.CreateCommandQueue,      "clCreateCommandQueue");
    ok &= _cl_sym((void**)&cl.CreateProgramWithSource, "clCreateProgramWithSource");
    ok &= _cl_sym((void**)&cl.BuildProgram,            "clBuildProgram");
    ok &= _cl_sym((void**)&cl.GetProgramBuildInfo,     "clGetProgramBuildInfo");
    ok &= _cl_sym((void**)&cl.CreateKernel,            "clCreateKernel");
    ok &= _cl_sym((void**)&cl.CreateBuffer,            "clCreateBuffer");
    ok &= _cl_sym((void**)&cl.SetKernelArg,            "clSetKernelArg");
    ok &= _cl_sym((void**)&cl.EnqueueWriteBuffer,      "clEnqueueWriteBuffer");
    ok &= _cl_sym((void**)&cl.EnqueueReadBuffer,       "clEnqueueReadBuffer");
    ok &= _cl_sym((void**)&cl.EnqueueNDRangeKernel,    "clEnqueueNDRangeKernel");
    ok &= _cl_sym((void**)&cl.Finish,                  "clFinish");
    ok &= _cl_sym((void**)&cl.ReleaseMemObject,        "clReleaseMemObject");
    return ok;
}

static int _gpu_init(void) {
    if (_gpu_init_state != 0) return _gpu_init_state;
    if (!_cl_load_lib()) { _gpu_init_state = -1; return -1; }

    cl_uint nplat = 0;
    if (cl.GetPlatformIDs(1, &_cl_platform, &nplat) != CL_SUCCESS || nplat == 0) {
        _gpu_init_state = -1; return -1;
    }
    // Prefer a GPU device; fall back to any device type the platform offers.
    cl_uint ndev = 0;
    if (cl.GetDeviceIDs(_cl_platform, CL_DEVICE_TYPE_GPU, 1, &_cl_device, &ndev) != CL_SUCCESS || ndev == 0) {
        if (cl.GetDeviceIDs(_cl_platform, CL_DEVICE_TYPE_DEFAULT, 1, &_cl_device, &ndev) != CL_SUCCESS || ndev == 0) {
            _gpu_init_state = -1; return -1;
        }
    }
    cl_int err = CL_SUCCESS;
    _cl_context = cl.CreateContext(NULL, 1, &_cl_device, NULL, NULL, &err);
    if (!_cl_context || err != CL_SUCCESS) { _gpu_init_state = -1; return -1; }
    _cl_queue = cl.CreateCommandQueue(_cl_context, _cl_device, 0, &err);
    if (!_cl_queue || err != CL_SUCCESS) { _cl_context = NULL; _gpu_init_state = -1; return -1; }

    _gpu_init_state = 1;
    return 1;
}

int wyn_gpu_available(void) {
    // WYN_GPU=0 is a hard kill-switch: report no device so everything is CPU.
    const char* off = getenv("WYN_GPU");
    if (off && off[0] == '0') return 0;
    return _gpu_init() == 1;
}

// --------------------------------------------------------------- cost model ---
// Mirrors the Metal thresholds (see src/gpu_metal.m for the derivation). The
// absolute crossover differs per GPU/driver, but the SHAPE is the same: a
// cached-kernel minimum N, plus cumulative-N tiers that amortize the one-time
// clBuildProgram compile before dispatching a not-yet-built kernel.
#define WYN_GPU_MIN_N_CACHED        300000
#define WYN_GPU_CUM_N_KERNEL_COLD   8000000
#define WYN_GPU_CUM_N_FIRST_EVER    40000000

static int _gpu_compiled_any = 0;

// Kernel cache: source string -> built cl_kernel. Pointer identity first
// (codegen passes string literals), strcmp fallback.
#define WYN_GPU_KCACHE 16
static struct { const char* src; cl_kernel kern; } _cl_kerns[WYN_GPU_KCACHE];
static int _cl_kern_count = 0;

// Cumulative eligible-N per not-yet-compiled kernel.
#define WYN_GPU_PENDING_CACHE 16
static struct { const char* src; long long cum_n; } _gpu_pending[WYN_GPU_PENDING_CACHE];
static int _gpu_pending_count = 0;

static int _gpu_kernel_cached(const char* src) {
    for (int i = 0; i < _cl_kern_count; i++)
        if (_cl_kerns[i].src == src || strcmp(_cl_kerns[i].src, src) == 0) return 1;
    return 0;
}

static long long _gpu_pending_add(const char* src, int n) {
    for (int i = 0; i < _gpu_pending_count; i++)
        if (_gpu_pending[i].src == src || strcmp(_gpu_pending[i].src, src) == 0)
            return (_gpu_pending[i].cum_n += n);
    if (_gpu_pending_count < WYN_GPU_PENDING_CACHE) {
        _gpu_pending[_gpu_pending_count].src = src;
        _gpu_pending[_gpu_pending_count].cum_n = n;
        _gpu_pending_count++;
    }
    return n;
}

int wyn_gpu_should_dispatch(int n, const char* kernel_src) {
    const char* off = getenv("WYN_GPU");
    if (off && off[0] == '0') return 0;   // kill-switch
    if (_gpu_init() != 1) return 0;
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
        yes = 0;
    } else {
        long long cum = _gpu_pending_add(kernel_src, n);
        yes = cum >= (_gpu_compiled_any ? WYN_GPU_CUM_N_KERNEL_COLD
                                        : WYN_GPU_CUM_N_FIRST_EVER);
    }
    if (getenv("WYN_GPU_DEBUG"))
        fprintf(stderr, "[wyn-gpu] n=%d cached=%d -> %s\n", n, cached, yes ? "GPU" : "cpu");
    return yes;
}

// ------------------------------------------------------- kernel compilation ---
static cl_kernel _gpu_kernel_for(const char* src) {
    for (int i = 0; i < _cl_kern_count; i++)
        if (_cl_kerns[i].src == src || strcmp(_cl_kerns[i].src, src) == 0)
            return _cl_kerns[i].kern;

    cl_int err = CL_SUCCESS;
    const char* srcs[1] = { src };
    cl_program prog = cl.CreateProgramWithSource(_cl_context, 1, srcs, NULL, &err);
    if (!prog || err != CL_SUCCESS) return NULL;
    err = cl.BuildProgram(prog, 1, &_cl_device, "", NULL, NULL);
    if (err != CL_SUCCESS) {
        if (getenv("WYN_GPU_DEBUG")) {
            char log[4096]; size_t got = 0;
            if (cl.GetProgramBuildInfo(prog, _cl_device, CL_PROGRAM_BUILD_LOG,
                                       sizeof(log) - 1, log, &got) == CL_SUCCESS) {
                log[got < sizeof(log) ? got : sizeof(log) - 1] = '\0';
                fprintf(stderr, "[wyn-gpu] clBuildProgram failed:\n%s\n", log);
            }
        }
        return NULL;
    }
    cl_kernel kern = cl.CreateKernel(prog, "wyn_map_kernel", &err);
    if (!kern || err != CL_SUCCESS) return NULL;

    _gpu_compiled_any = 1;
    if (_cl_kern_count < WYN_GPU_KCACHE) {
        _cl_kerns[_cl_kern_count].src = src;
        _cl_kerns[_cl_kern_count].kern = kern;
        _cl_kern_count++;
    }
    return kern;
}

// ---------------------------------------------------------------- buffers ---
// OpenCL has no guaranteed unified memory, so we keep host staging arrays that
// begin() hands to the caller to pack into; run() writes them to device
// buffers, dispatches, and reads results back into the host-out array.
static cl_mem   _cl_in_buf = NULL, _cl_out_buf = NULL;
static float*   _host_in = NULL;
static float*   _host_out = NULL;
static size_t   _buf_cap = 0;   // capacity in elements

static int _gpu_ensure_bufs(int n) {
    if ((size_t)n <= _buf_cap && _cl_in_buf && _cl_out_buf) return 0;
    size_t cap = n < 4096 ? 4096 : (size_t)n;
    if (_cl_in_buf)  { cl.ReleaseMemObject(_cl_in_buf);  _cl_in_buf = NULL; }
    if (_cl_out_buf) { cl.ReleaseMemObject(_cl_out_buf); _cl_out_buf = NULL; }
    cl_int err = CL_SUCCESS;
    _cl_in_buf  = cl.CreateBuffer(_cl_context, CL_MEM_READ_ONLY,  cap * sizeof(float), NULL, &err);
    if (!_cl_in_buf || err != CL_SUCCESS) { _buf_cap = 0; return -1; }
    _cl_out_buf = cl.CreateBuffer(_cl_context, CL_MEM_WRITE_ONLY, cap * sizeof(float), NULL, &err);
    if (!_cl_out_buf || err != CL_SUCCESS) {
        cl.ReleaseMemObject(_cl_in_buf); _cl_in_buf = NULL; _buf_cap = 0; return -1;
    }
    float* ni = (float*)realloc(_host_in,  cap * sizeof(float));
    float* no = (float*)realloc(_host_out, cap * sizeof(float));
    if (!ni || !no) { free(ni); free(no); _host_in = _host_out = NULL; _buf_cap = 0; return -1; }
    _host_in = ni; _host_out = no;
    _buf_cap = cap;
    return 0;
}

int wyn_gpu_map_f32_begin(int n, float** in_ptr) {
    if (n <= 0 || !in_ptr) return -1;
    if (_gpu_init() != 1) return -1;
    if (_gpu_ensure_bufs(n) != 0) return -1;
    *in_ptr = _host_in;
    return 0;
}

int wyn_gpu_map_f32_run(int n, const char* kernel_src, float** out_ptr) {
    if (n <= 0 || !kernel_src || !out_ptr) return -1;
    if (_gpu_init() != 1) return -1;
    if ((size_t)n > _buf_cap) return -1;   // begin() not called for this size

    cl_kernel kern = _gpu_kernel_for(kernel_src);
    if (!kern) return -1;

    size_t bytes = (size_t)n * sizeof(float);
    if (cl.EnqueueWriteBuffer(_cl_queue, _cl_in_buf, CL_TRUE, 0, bytes, _host_in, 0, NULL, NULL) != CL_SUCCESS)
        return -1;
    if (cl.SetKernelArg(kern, 0, sizeof(cl_mem), &_cl_in_buf)  != CL_SUCCESS) return -1;
    if (cl.SetKernelArg(kern, 1, sizeof(cl_mem), &_cl_out_buf) != CL_SUCCESS) return -1;
    size_t global = (size_t)n;
    if (cl.EnqueueNDRangeKernel(_cl_queue, kern, 1, NULL, &global, NULL, 0, NULL, NULL) != CL_SUCCESS)
        return -1;
    if (cl.EnqueueReadBuffer(_cl_queue, _cl_out_buf, CL_TRUE, 0, bytes, _host_out, 0, NULL, NULL) != CL_SUCCESS)
        return -1;
    if (cl.Finish(_cl_queue) != CL_SUCCESS) return -1;

    *out_ptr = _host_out;
    return 0;
}

#endif // WYN_GPU_OPENCL
