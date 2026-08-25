#ifndef WYN_WRITE_GUARD_H
#define WYN_WRITE_GUARD_H
// Concurrent structural-mutation guard for the COLLECTION types.
//
// WHY: Wyn had three different answers to one hazard - concurrent mutation of
// shared state - and PLAN_v1.21 §3 called that inconsistency the sharpest
// structural criticism of the v1.20 posture:
//
//   arrays          -> runtime panic naming the remedy   (WYN_ARR_WRITE_ENTER)
//   scalar globals  -> check-time error naming the remedy
//   HashMap/HashSet -> NOTHING
//
// A HashMap is a fixed bucket array of malloc'd Entry lists, so two writers
// splice the same bucket head concurrently: updates are lost, and the graveyard
// list (which defers freeing overwritten string values) can be corrupted, which
// is a use-after-free at the next read. There is no rehash to race, which is why
// this does not corrupt on every run - it corrupts on the unlucky interleaving,
// which is the worst kind of bug to ship.
//
// The overlap is real and measurable: the ARRAY barrier fires on the same
// program shape (two awaited spawns each mutating one global), so the tasks
// provably run concurrently - collections simply had no guard to fire.
//
// POSTURE: this DETECTS and panics, exactly like the array flag, rather than
// locking. That is deliberate and matches the language's stated stance - a
// memory-safe language must not quietly do something else and keep running - and
// making the three tiers agree is the entire point of the item. It costs one
// relaxed atomic exchange per mutation, negligible beside the malloc/strdup the
// mutators already do.
//
// LIMITS, stated plainly: this catches WRITER-vs-WRITER only, the same scope as
// the array flag. A read concurrent with a write is still unguarded, for both
// arrays and collections. Closing that needs a different mechanism (real
// ownership or a lock) and is not what this item claims.
//
// Unlike WynArray, no self-healing is needed: hashmap_new/hashset_new both use
// calloc, so `writing` starts at 0 rather than carrying malloc garbage.
#include <stdio.h>
#include <stdlib.h>

static inline void wyn_concurrent_collection_panic(const char* what) {
    fprintf(stderr,
            "panic: concurrent %s mutation detected - %s is not thread-safe; "
            "use a channel or Shared to coordinate writers\n", what, what);
    exit(1);
}

// TCC (the dev loop) lacks the __atomic builtins, so it degrades to a
// best-effort non-atomic check - same fallback the array guard uses.
#if defined(__TINYC__)
#define WYN_COLL_WRITE_ENTER(pflag, what) do { \
    if (*(pflag) == 1) wyn_concurrent_collection_panic(what); \
    *(pflag) = 1; \
} while (0)
#define WYN_COLL_WRITE_EXIT(pflag) (*(pflag) = 0)
#else
#define WYN_COLL_WRITE_ENTER(pflag, what) do { \
    if (__atomic_exchange_n((pflag), 1, __ATOMIC_RELAXED) == 1) \
        wyn_concurrent_collection_panic(what); \
} while (0)
#define WYN_COLL_WRITE_EXIT(pflag) __atomic_store_n((pflag), 0, __ATOMIC_RELAXED)
#endif

#endif // WYN_WRITE_GUARD_H
