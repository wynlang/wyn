#ifndef FUTURE_H
#define FUTURE_H

typedef struct Future Future;
typedef void* (*MapFunc)(void*);

// Core API
Future* future_new(void);
void future_set(Future* f, void* result);
void* future_get(Future* f);          // memoizing: await twice returns the same value
void* future_get_consume(Future* f);  // single-reader: get + recycle (temps, joins)
void* future_get_timeout(Future* f, int timeout_ms);
int future_is_ready(Future* f);
void future_free(Future* f);

// S4 cooperative cancellation
void future_cancel(Future* f);          // request cancellation of the awaitee
int  future_is_cancelled(Future* f);    // is this future's task cancelled?
int  wyn_current_task_cancelled(void);  // is the running coroutine task cancelled?

// Combinators
Future* future_map(Future* f, MapFunc map_fn);
Future* future_all(Future** futures, int count);
Future* future_race(Future** futures, int count);
Future* future_select(Future** futures, int count);

// Spawn API
typedef void (*TaskFunc)(void*);
typedef void* (*TaskFuncWithReturn)(void*);
void wyn_spawn_fast(TaskFunc func, void* arg);
void wyn_spawn_fast_traced(TaskFunc func, void* arg, const char* file, int line);
Future* wyn_spawn_async(TaskFuncWithReturn func, void* arg);
Future* wyn_spawn_async_traced(TaskFuncWithReturn func, void* arg, const char* file, int line);

// Spawn origin query (for coroutine-aware error messages)
const char* wyn_spawn_origin_file(void);
int wyn_spawn_origin_line(void);
long wyn_spawn_origin_id(void);

#endif
