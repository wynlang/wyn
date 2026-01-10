#ifndef WYN_COLLECTIONS_H
#define WYN_COLLECTIONS_H

#include <stdbool.h>
#include <stddef.h>

// Forward declarations
typedef struct WynVec WynVec;
typedef struct WynVecIterator WynVecIterator;
typedef struct WynHashMap WynHashMap;
typedef struct WynHashMapIterator WynHashMapIterator;
typedef struct WynHashSet WynHashSet;
typedef struct WynHashSetIterator WynHashSetIterator;

// Hash function type
typedef size_t (*WynHashFn)(const void* key);
typedef bool (*WynEqualsFn)(const void* a, const void* b);

// Dynamic array (Vec) structure
typedef struct WynVec {
    void* data;
    size_t len;
    size_t cap;
    size_t item_size;
    void (*drop_fn)(void*);  // Optional destructor for items
} WynVec;

// Vector iterator
typedef struct WynVecIterator {
    WynVec* vec;
    size_t index;
} WynVecIterator;

// Vec creation and destruction
WynVec* wyn_vec_new(size_t item_size);
WynVec* wyn_vec_with_capacity(size_t item_size, size_t capacity);
void wyn_vec_free(WynVec* vec);
void wyn_vec_set_drop_fn(WynVec* vec, void (*drop_fn)(void*));

// Vec operations
bool wyn_vec_push(WynVec* vec, const void* item);
bool wyn_vec_pop(WynVec* vec, void* out_item);
bool wyn_vec_get(WynVec* vec, size_t index, void* out_item);
bool wyn_vec_set(WynVec* vec, size_t index, const void* item);
bool wyn_vec_insert(WynVec* vec, size_t index, const void* item);
bool wyn_vec_remove(WynVec* vec, size_t index, void* out_item);
void wyn_vec_clear(WynVec* vec);
bool wyn_vec_reserve(WynVec* vec, size_t additional);
void wyn_vec_shrink_to_fit(WynVec* vec);

// Vec properties
size_t wyn_vec_len(const WynVec* vec);
size_t wyn_vec_capacity(const WynVec* vec);
bool wyn_vec_is_empty(const WynVec* vec);
void* wyn_vec_as_ptr(WynVec* vec);
const void* wyn_vec_as_const_ptr(const WynVec* vec);

// Iterator operations
WynVecIterator* wyn_vec_iter(WynVec* vec);
bool wyn_vec_iter_next(WynVecIterator* iter, void* out_item);
void wyn_vec_iter_free(WynVecIterator* iter);

// Utility functions
bool wyn_vec_contains(WynVec* vec, const void* item, bool (*eq_fn)(const void*, const void*));
void wyn_vec_sort(WynVec* vec, int (*cmp_fn)(const void*, const void*));
WynVec* wyn_vec_clone(const WynVec* vec, void* (*clone_fn)(const void*));

// Hash Map bucket for Robin Hood hashing
typedef struct {
    void* key;
    void* value;
    size_t distance;  // Distance from ideal position
    bool occupied;
} WynHashMapBucket;

// Hash Map structure
typedef struct WynHashMap {
    WynHashMapBucket* buckets;
    size_t len;
    size_t cap;
    size_t key_size;
    size_t value_size;
    double load_factor;
    WynHashFn hash_fn;
    WynEqualsFn equals_fn;
    void (*key_drop_fn)(void*);
    void (*value_drop_fn)(void*);
} WynHashMap;

// Hash Map iterator
typedef struct WynHashMapIterator {
    WynHashMap* map;
    size_t index;
} WynHashMapIterator;

// HashMap creation and destruction
WynHashMap* wyn_hashmap_new(size_t key_size, size_t value_size, WynHashFn hash_fn, WynEqualsFn equals_fn);
WynHashMap* wyn_hashmap_with_capacity(size_t key_size, size_t value_size, size_t capacity, WynHashFn hash_fn, WynEqualsFn equals_fn);
void wyn_hashmap_free(WynHashMap* map);
void wyn_hashmap_set_key_drop_fn(WynHashMap* map, void (*drop_fn)(void*));
void wyn_hashmap_set_value_drop_fn(WynHashMap* map, void (*drop_fn)(void*));

// HashMap operations
bool wyn_hashmap_insert(WynHashMap* map, const void* key, const void* value, void* old_value);
bool wyn_hashmap_get(WynHashMap* map, const void* key, void* out_value);
bool wyn_hashmap_remove(WynHashMap* map, const void* key, void* out_value);
bool wyn_hashmap_contains_key(WynHashMap* map, const void* key);
void wyn_hashmap_clear(WynHashMap* map);

// HashMap properties
size_t wyn_hashmap_len(const WynHashMap* map);
size_t wyn_hashmap_capacity(const WynHashMap* map);
bool wyn_hashmap_is_empty(const WynHashMap* map);
double wyn_hashmap_load_factor(const WynHashMap* map);

// HashMap iteration
WynHashMapIterator* wyn_hashmap_iter(WynHashMap* map);
bool wyn_hashmap_iter_next(WynHashMapIterator* iter, void* out_key, void* out_value);
void wyn_hashmap_iter_free(WynHashMapIterator* iter);

// Common hash functions
size_t wyn_hash_int(const void* key);
size_t wyn_hash_string(const void* key);
size_t wyn_hash_bytes(const void* data, size_t len);
bool wyn_equals_int(const void* a, const void* b);
bool wyn_equals_string(const void* a, const void* b);

// Hash Set structure (built on HashMap)
typedef struct WynHashSet {
    WynHashMap* map;  // Internal HashMap with dummy values
    size_t item_size;
} WynHashSet;

// Hash Set iterator
typedef struct WynHashSetIterator {
    WynHashMapIterator* map_iter;
    void* dummy_value;  // Temporary storage for map iterator
} WynHashSetIterator;

// HashSet creation and destruction
WynHashSet* wyn_hashset_new(size_t item_size, WynHashFn hash_fn, WynEqualsFn equals_fn);
WynHashSet* wyn_hashset_with_capacity(size_t item_size, size_t capacity, WynHashFn hash_fn, WynEqualsFn equals_fn);
void wyn_hashset_free(WynHashSet* set);
void wyn_hashset_set_drop_fn(WynHashSet* set, void (*drop_fn)(void*));

// HashSet operations
bool wyn_hashset_insert(WynHashSet* set, const void* item);
bool wyn_hashset_remove(WynHashSet* set, const void* item);
bool wyn_hashset_contains(WynHashSet* set, const void* item);
void wyn_hashset_clear(WynHashSet* set);

// HashSet properties
size_t wyn_hashset_len(const WynHashSet* set);
size_t wyn_hashset_capacity(const WynHashSet* set);
bool wyn_hashset_is_empty(const WynHashSet* set);
double wyn_hashset_load_factor(const WynHashSet* set);

// HashSet iteration
WynHashSetIterator* wyn_hashset_iter(WynHashSet* set);
bool wyn_hashset_iter_next(WynHashSetIterator* iter, void* out_item);
void wyn_hashset_iter_free(WynHashSetIterator* iter);

// HashSet operations
WynHashSet* wyn_hashset_union(WynHashSet* set1, WynHashSet* set2);
WynHashSet* wyn_hashset_intersection(WynHashSet* set1, WynHashSet* set2);
WynHashSet* wyn_hashset_difference(WynHashSet* set1, WynHashSet* set2);
bool wyn_hashset_is_subset(WynHashSet* subset, WynHashSet* superset);
bool wyn_hashset_is_disjoint(WynHashSet* set1, WynHashSet* set2);

#endif // WYN_COLLECTIONS_H
