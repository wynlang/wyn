// growable.h - shared realloc-doubling helper for compiler registries.
//
// The compiler keeps many per-program registries (lambdas, spawn wrappers,
// enum/struct name tables, RC scope tracking, ...). These used to be
// fixed-size arrays with silent `if (count >= CAP) return;` guards, which
// meant a program with more than CAP entries was silently miscompiled.
// Every registry now grows on demand through this macro.
//
// Usage:
//   static Foo* items = NULL;
//   static int item_count = 0, item_cap = 0;
//   ...
//   WYN_ENSURE_CAP(items, item_count, item_cap);
//   items[item_count++] = ...;
//
// `arr` must be a heap pointer (NULL initially), `count`/`cap` ints.
// Growth is realloc-doubling starting at 64. Allocation failure is fatal:
// a compiler with a half-populated registry would miscompile, so exit loudly.
#ifndef WYN_GROWABLE_H
#define WYN_GROWABLE_H

#include <stdio.h>
#include <stdlib.h>

#define WYN_ENSURE_CAP(arr, count, cap) do { \
    if ((count) >= (cap)) { \
        int _wyn_newcap = (cap) > 0 ? (cap) * 2 : 64; \
        void* _wyn_tmp = realloc((arr), (size_t)_wyn_newcap * sizeof *(arr)); \
        if (!_wyn_tmp) { \
            fprintf(stderr, "wyn: out of memory growing compiler registry (%s)\n", #arr); \
            exit(1); \
        } \
        (arr) = _wyn_tmp; \
        (cap) = _wyn_newcap; \
    } \
} while (0)

#endif // WYN_GROWABLE_H
