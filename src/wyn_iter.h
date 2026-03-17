// wyn_iter.h — Iterator/Generator runtime support (L3)
#ifndef WYN_ITER_H
#define WYN_ITER_H

#include <stdlib.h>
#include <string.h>
#include "coroutine.h"

typedef struct WynIter {
    WynCoroutine* coro;
    long long current_value;
    int has_value;
    int done;
    void (*gen_fn)(void*);
    void* gen_arg;
} WynIter;

// The currently yielding iterator — set by trampoline, read by wyn_yield
// This works because wyn_yield is called from WITHIN the coroutine that owns the iterator
static WynIter* _wyn_yielding_iter = NULL;

static void _wyn_iter_trampoline(void* arg) {
    WynIter* it = (WynIter*)arg;
    WynIter* prev = _wyn_yielding_iter;
    _wyn_yielding_iter = it;
    it->gen_fn(it->gen_arg);
    _wyn_yielding_iter = prev;
    it->done = 1;
    it->has_value = 0;
}

static inline void wyn_yield(long long value) {
    if (_wyn_yielding_iter) {
        _wyn_yielding_iter->current_value = value;
        _wyn_yielding_iter->has_value = 1;
        wyn_coro_yield();
    }
}

static inline WynIter* wyn_iter_create(void (*fn)(void*), void* arg) {
    WynIter* it = (WynIter*)calloc(1, sizeof(WynIter));
    it->gen_fn = fn;
    it->gen_arg = arg;
    it->coro = wyn_coro_create(_wyn_iter_trampoline, it);
    return it;
}

static inline int wyn_iter_next(WynIter* it) {
    if (!it || it->done) return 0;
    it->has_value = 0;
    WynIter* prev = _wyn_yielding_iter;
    _wyn_yielding_iter = it;  // set BEFORE resume so yield writes to correct iter
    wyn_coro_resume(it->coro);
    _wyn_yielding_iter = prev;
    return it->has_value;
}

static inline long long wyn_iter_value(WynIter* it) {
    return it->current_value;
}

static inline void wyn_iter_destroy(WynIter* it) {
    if (!it) return;
    if (it->coro) wyn_coro_destroy(it->coro);
    free(it);
}

static inline WynArray wyn_iter_collect(WynIter* it) {
    WynArray arr = {0};
    while (wyn_iter_next(it)) {
        array_push_int(&arr, wyn_iter_value(it));
    }
    wyn_iter_destroy(it);
    return arr;
}

// Take
typedef struct { WynIter* src; int remaining; } _WynIterTake;
static void _wyn_iter_take_fn(void* arg) {
    _WynIterTake* t = (_WynIterTake*)arg;
    int count = 0;
    while (count < t->remaining && wyn_iter_next(t->src)) {
        wyn_yield(wyn_iter_value(t->src));
        count++;
    }
    wyn_iter_destroy(t->src);
    free(t);
}
static inline WynIter* wyn_iter_take(WynIter* src, int n) {
    _WynIterTake* t = (_WynIterTake*)malloc(sizeof(_WynIterTake));
    t->src = src; t->remaining = n;
    return wyn_iter_create(_wyn_iter_take_fn, t);
}

// Map
typedef struct { WynIter* src; long long (*fn)(long long); } _WynIterMap;
static void _wyn_iter_map_fn(void* arg) {
    _WynIterMap* m = (_WynIterMap*)arg;
    while (wyn_iter_next(m->src)) {
        wyn_yield(m->fn(wyn_iter_value(m->src)));
    }
    wyn_iter_destroy(m->src);
    free(m);
}
static inline WynIter* wyn_iter_map(WynIter* src, long long (*fn)(long long)) {
    _WynIterMap* m = (_WynIterMap*)malloc(sizeof(_WynIterMap));
    m->src = src; m->fn = fn;
    return wyn_iter_create(_wyn_iter_map_fn, m);
}

// Filter
typedef struct { WynIter* src; long long (*fn)(long long); } _WynIterFilter;
static void _wyn_iter_filter_fn(void* arg) {
    _WynIterFilter* f = (_WynIterFilter*)arg;
    while (wyn_iter_next(f->src)) {
        long long v = wyn_iter_value(f->src);
        if (f->fn(v)) wyn_yield(v);
    }
    wyn_iter_destroy(f->src);
    free(f);
}
static inline WynIter* wyn_iter_filter(WynIter* src, long long (*fn)(long long)) {
    _WynIterFilter* f = (_WynIterFilter*)malloc(sizeof(_WynIterFilter));
    f->src = src; f->fn = fn;
    return wyn_iter_create(_wyn_iter_filter_fn, f);
}

#endif // WYN_ITER_H
