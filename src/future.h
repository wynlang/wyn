#ifndef FUTURE_H
#define FUTURE_H

typedef struct Future Future;
typedef void* (*MapFunc)(void*);

// Core API
Future* future_new();
void future_set(Future* f, void* result);
void* future_get(Future* f);
void* future_get_timeout(Future* f, int timeout_ms);
int future_is_ready(Future* f);
void future_free(Future* f);

// Combinators
Future* future_map(Future* f, MapFunc map_fn);
Future* future_all(Future** futures, int count);
Future* future_race(Future** futures, int count);
Future* future_select(Future** futures, int count);

// Spawn API
typedef void* (*TaskFuncWithReturn)(void*);
Future* wyn_spawn_async(TaskFuncWithReturn func, void* arg);

#endif
