#ifndef WYN_COROUTINE_H
#define WYN_COROUTINE_H
#include <stdbool.h>

typedef struct WynCoroutine WynCoroutine;

// Create a coroutine that runs fn(arg). Stack size = 8KB default.
WynCoroutine* wyn_coro_create(void (*fn)(void*), void* arg);

// Resume a suspended coroutine. Returns true if coroutine is still alive.
bool wyn_coro_resume(WynCoroutine* co);

// Yield the current coroutine (called from inside the coroutine).
void wyn_coro_yield(void);

// Check if coroutine has finished.
bool wyn_coro_done(WynCoroutine* co);

// Destroy a coroutine and free its stack.
void wyn_coro_destroy(WynCoroutine* co);

// Get the currently running coroutine (NULL if on main thread).
WynCoroutine* wyn_coro_current(void);

// Get number of live (not yet destroyed) coroutines.
long wyn_coro_get_live_count(void);

#endif
