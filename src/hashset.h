#ifndef WYN_HASHSET_H
#define WYN_HASHSET_H

// For size_t in hashset_format's signature. hashmap.h already includes it for the
// same reason; this header had no need for it until the formatter arrived (#427).
#include <stddef.h>

typedef struct WynHashSet WynHashSet;

// V-38 (#391): the set stores a TAGGED element, the way HashMap already stores a
// tagged HashMapValue. Before this it stored `char*` only, so the honest answer to
// `{:1}` was to refuse it (#374): the cheap alternative - stringify the int into the
// same table - collapses `{:1}` and `{:"1"}` into one set, a silently wrong answer
// and worse than the crash it replaced. The tag is part of both the hash and the
// equality test, which is exactly what keeps those two sets distinct.
typedef enum {
    HASHSET_STRING,
    HASHSET_INT,
    HASHSET_FLOAT,
    HASHSET_BOOL
} HashSetElemType;

// A read-only view of one element, for the iteration path (`for x in s`). Callers
// must not free `as_string`; it belongs to the set.
typedef struct {
    HashSetElemType type;
    const char* as_string;
    long long as_int;
    double as_float;
    int as_bool;
} HashSetElem;

WynHashSet* hashset_new(void);

// String elements. `hashset_add` keeps its original name and signature because
// every existing call site emits it.
void hashset_add(WynHashSet* set, const char* key);
int hashset_contains(WynHashSet* set, const char* key);
void hashset_remove(WynHashSet* set, const char* key);

// Int / float / bool elements. One family per element kind rather than a single
// tagged-argument entry point, because codegen knows the element type statically
// and a C function per kind is what lets the C compiler type-check the call.
void hashset_add_int(WynHashSet* set, long long v);
int  hashset_contains_int(WynHashSet* set, long long v);
void hashset_remove_int(WynHashSet* set, long long v);
void hashset_add_float(WynHashSet* set, double v);
int  hashset_contains_float(WynHashSet* set, double v);
void hashset_remove_float(WynHashSet* set, double v);
void hashset_add_bool(WynHashSet* set, int v);
int  hashset_contains_bool(WynHashSet* set, int v);
void hashset_remove_bool(WynHashSet* set, int v);

// src/types.c has advertised `.add_int()` / `.contains_int()` under these names
// since before the element type existed, lowering them to symbols NO runtime
// source defined (`nm runtime/libwyn_rt.a` had neither) - so the language's answer
// to the two methods that sounded like int support was an internal codegen error.
// They are real now, and are the int family under its advertised spelling.
void wyn_hashset_add_int(WynHashSet* set, long long v);
int  wyn_hashset_contains_int(WynHashSet* set, long long v);

// #427: render a set as its literal spelling - `{:1, 2}`, `{:"a"}`, `{:}` when empty.
// snprintf semantics: returns the length that WOULD be written, so size then fill.
int hashset_format(WynHashSet* set, char* out, size_t cap);

void hashset_free(WynHashSet* set);

// Iteration support: index-addressed walk over the tagged elements. Bucket order,
// NOT insertion order - same caveat as hashmap_keys(). Returns 0 when `index` is
// out of range. O(n) per call by construction (the buckets are linked lists), which
// the `for x in s` lowering avoids by materialising the elements once per loop.
int hashset_count(WynHashSet* set);
int hashset_elem_at(WynHashSet* set, int index, HashSetElem* out);

// Wrapper functions
void wyn_hashset_insert(WynHashSet* set, const char* key);
int wyn_hashset_contains(WynHashSet* set, const char* key);
void wyn_hashset_remove(WynHashSet* set, const char* key);
int wyn_hashset_len(WynHashSet* set);
int wyn_hashset_is_empty(WynHashSet* set);
void wyn_hashset_clear(WynHashSet* set);
WynHashSet* wyn_hashset_union(WynHashSet* set1, WynHashSet* set2);
WynHashSet* wyn_hashset_intersection(WynHashSet* set1, WynHashSet* set2);
WynHashSet* wyn_hashset_difference(WynHashSet* set1, WynHashSet* set2);
int wyn_hashset_is_subset(WynHashSet* set1, WynHashSet* set2);
int wyn_hashset_is_disjoint(WynHashSet* set1, WynHashSet* set2);

#endif
