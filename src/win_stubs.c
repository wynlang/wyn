// Stubs for platforms without POSIX concurrency (Windows, WASM)
#if defined(_WIN32) || defined(__EMSCRIPTEN__) || defined(__wasi__)
#include <stdlib.h>
typedef struct { int dummy; } Future;
void* wyn_task_new(void) { return NULL; }
void wyn_task_free(void* t) { (void)t; }
void wyn_task_send(void* t, long long v) { (void)t; (void)v; }
long long wyn_task_recv(void* t) { (void)t; return 0; }
void wyn_task_close(void* t) { (void)t; }
void* wyn_coro_current(void) { return NULL; }
void wyn_coro_yield(void) {}
void* future_get(Future* f) { (void)f; return NULL; }
void* future_race(void** f, int n) { (void)f; (void)n; return NULL; }
void wyn_sched_enqueue(void* t) { (void)t; }
void* wyn_spawn_async_traced(void* fn, void* arg, const char* f, int l) { (void)fn; (void)arg; (void)f; (void)l; return NULL; }
long long wyn_spawn_inline(void* fn, void* arg) { (void)fn; (void)arg; return 0; }
const char* wyn_spawn_origin_file(void) { return NULL; }
int wyn_spawn_origin_line(void) { return 0; }
long wyn_spawn_origin_id(void) { return 0; }
#endif
