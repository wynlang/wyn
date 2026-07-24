// bench_gpu_map.c - honest GPU-vs-CPU benchmark for the transparent GPU
// dispatch spike (see docs/GPU_DESIGN.md).
//
// CPU side replicates the REAL runtime path a Wyn program takes today:
// a WynArray of 16-byte tagged WynValue cells, walked with a function
// pointer per element (wyn_array_map_float's exact loop, including the
// array_push_float growth pattern).
//
// GPU side replicates the REAL dispatch path the spike wires in: pack the
// tagged array into a shared MTLBuffer of float32, run the kernel, unpack
// back into a fresh WynArray. Pack/unpack IS the price of transparency, so
// it is measured inside the GPU time, not excluded.
//
// Build (from repo root):
//   clang -O2 -std=c11 -c benchmarks/gpu/bench_gpu_map.c -o /tmp/bench_map.o
//   clang -fobjc-arc -O2 -c src/gpu_metal.m -o /tmp/gpu_metal.o
//   clang -O2 -o /tmp/bench_map /tmp/bench_map.o /tmp/gpu_metal.o \
//         -framework Metal -framework Foundation
//   /tmp/bench_map

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ---- Minimal replica of the runtime's array representation ----
typedef enum { WT_INT, WT_FLOAT } WTypeId;
typedef struct { WTypeId type; union { long long i; double f; } data; } WVal;
typedef struct { WVal* data; int count; int capacity; } WArr;

static void warr_push_f(WArr* a, double v) {
    if (a->count >= a->capacity) {
        int nc = a->capacity == 0 ? 4 : a->capacity * 2;
        a->data = realloc(a->data, sizeof(WVal) * (size_t)nc);
        a->capacity = nc;
    }
    a->data[a->count].type = WT_FLOAT;
    a->data[a->count].data.f = v;
    a->count++;
}

// Exact shape of wyn_array_map_float: fresh result array, fn per element.
static WArr warr_map_f(WArr arr, double (*fn)(double)) {
    WArr r = {0};
    for (int i = 0; i < arr.count; i++) warr_push_f(&r, fn(arr.data[i].data.f));
    return r;
}

// ---- The two lambdas, as the compiled C the codegen emits today ----
static double lam_simple(double x) { return x * 2.5 + 1.0; }
static double lam_heavy(double x) {
    // ~20 flops of pure arithmetic (no libm calls - those would be a
    // different, even more GPU-favorable comparison).
    double a = x * 1.1 + 0.1; double b = a * a - x;
    double c = b * 0.5 + a;   double d = c * c + b * 1.3;
    double e = d - c * 0.7;   double f = e * 1.01 + d * 0.02;
    double g = f * f - e;     double h = g * 0.25 + f;
    return h * 1.5 - g * 0.33 + x;
}

// Same lambdas as MSL kernel bodies (what codegen would emit second).
static const char* K_SIMPLE =
    "#include <metal_stdlib>\nusing namespace metal;\n"
    "kernel void wyn_map_kernel(device const float* in [[buffer(0)]],"
    " device float* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {"
    " float x = in[i]; out[i] = x * 2.5f + 1.0f; }";
static const char* K_HEAVY =
    "#include <metal_stdlib>\nusing namespace metal;\n"
    "kernel void wyn_map_kernel(device const float* in [[buffer(0)]],"
    " device float* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {"
    " float x = in[i];"
    " float a = x * 1.1f + 0.1f; float b = a * a - x;"
    " float c = b * 0.5f + a;   float d = c * c + b * 1.3f;"
    " float e = d - c * 0.7f;   float f = e * 1.01f + d * 0.02f;"
    " float g = f * f - e;      float h = g * 0.25f + f;"
    " out[i] = h * 1.5f - g * 0.33f + x; }";

// ---- GPU entry points (src/gpu_metal.m) ----
extern int wyn_gpu_available(void);
extern int wyn_gpu_map_f32_begin(int n, float** in_ptr);
extern int wyn_gpu_map_f32_run(int n, const char* kernel_src, float** out_ptr);

static double now_ms(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

// Full transparent-dispatch GPU path: pack + run + unpack into a WynArray.
static WArr gpu_map(WArr arr, const char* kernel, int* ok) {
    WArr r = {0}; *ok = 0;
    float* in = NULL; float* out = NULL;
    if (wyn_gpu_map_f32_begin(arr.count, &in) != 0) return r;
    for (int i = 0; i < arr.count; i++) in[i] = (float)arr.data[i].data.f;
    if (wyn_gpu_map_f32_run(arr.count, kernel, &out) != 0) return r;
    r.data = malloc(sizeof(WVal) * (size_t)arr.count);
    r.capacity = arr.count; r.count = arr.count;
    for (int i = 0; i < arr.count; i++) {
        r.data[i].type = WT_FLOAT;
        r.data[i].data.f = (double)out[i];
    }
    *ok = 1;
    return r;
}

static double median3(double a, double b, double c) {
    if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
    if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
    return c;
}

int main(void) {
    if (!wyn_gpu_available()) { printf("no Metal device\n"); return 1; }

    // First-dispatch overhead, measured separately: device init happened in
    // wyn_gpu_available(); now time the first pipeline compile + tiny run.
    {
        WArr tiny = {0};
        for (int i = 0; i < 64; i++) warr_push_f(&tiny, (double)i);
        int ok = 0;
        double t0 = now_ms();
        WArr r = gpu_map(tiny, K_SIMPLE, &ok);
        double t1 = now_ms();
        printf("first-dispatch (simple kernel, N=64): %.2f ms  (pipeline compile + run)%s\n",
               t1 - t0, ok ? "" : "  FAILED");
        free(r.data);
        t0 = now_ms();
        WArr r2 = gpu_map(tiny, K_HEAVY, &ok);
        t1 = now_ms();
        printf("first-dispatch (heavy kernel,  N=64): %.2f ms\n\n", t1 - t0);
        free(r2.data);
        free(tiny.data);
    }

    long long sizes[] = {1000, 100000, 1000000, 10000000, 100000000};
    const char* size_names[] = {"1K", "100K", "1M", "10M", "100M"};

    printf("%-6s %-8s %12s %12s %8s %s\n",
           "N", "lambda", "CPU ms", "GPU ms", "speedup", "winner");
    printf("------------------------------------------------------------------\n");

    for (int si = 0; si < 5; si++) {
        int n = (int)sizes[si];

        WArr src = {0};
        src.data = malloc(sizeof(WVal) * (size_t)n);
        src.capacity = n; src.count = n;
        for (int i = 0; i < n; i++) {
            src.data[i].type = WT_FLOAT;
            src.data[i].data.f = (double)(i % 1000) * 0.001 + 0.5;
        }

        for (int li = 0; li < 2; li++) {
            double (*fn)(double) = li == 0 ? lam_simple : lam_heavy;
            const char* kernel = li == 0 ? K_SIMPLE : K_HEAVY;
            const char* lname = li == 0 ? "simple" : "heavy20";

            // CPU: median of 3 (1 warmup)
            { WArr w = warr_map_f(src, fn); free(w.data); }
            double ct[3];
            for (int t = 0; t < 3; t++) {
                double t0 = now_ms();
                WArr r = warr_map_f(src, fn);
                ct[t] = now_ms() - t0;
                free(r.data);
            }
            double cpu = median3(ct[0], ct[1], ct[2]);

            // GPU: median of 3 (1 warmup - includes pipeline compile on
            // first-ever use, already paid above)
            int ok = 0;
            { WArr w = gpu_map(src, kernel, &ok); free(w.data); }
            double gt[3]; int gok = ok;
            for (int t = 0; t < 3; t++) {
                double t0 = now_ms();
                WArr r = gpu_map(src, kernel, &ok);
                gt[t] = now_ms() - t0;
                gok = gok && ok;
                free(r.data);
            }
            double gpu = median3(gt[0], gt[1], gt[2]);

            // Correctness spot check (float32 tolerance)
            const char* corr = "";
            {
                WArr r = gpu_map(src, kernel, &ok);
                if (ok && n > 0) {
                    int bad = 0;
                    for (int i = 0; i < n; i += (n / 97) + 1) {
                        double want = fn(src.data[i].data.f);
                        double got = r.data[i].data.f;
                        double tol = (want == 0) ? 1e-4 : want * 1e-3;
                        if (tol < 0) tol = -tol;
                        double d = got - want; if (d < 0) d = -d;
                        if (d > tol) bad++;
                    }
                    if (bad) corr = "  MISMATCH";
                }
                free(r.data);
            }

            if (!gok) {
                printf("%-6s %-8s %12.3f %12s %8s GPU-FAILED\n", size_names[si], lname, cpu, "-", "-");
            } else {
                printf("%-6s %-8s %12.3f %12.3f %7.2fx %s%s\n",
                       size_names[si], lname, cpu, gpu, cpu / gpu,
                       cpu > gpu ? "GPU" : "cpu", corr);
            }
        }
        free(src.data);
    }
    return 0;
}
