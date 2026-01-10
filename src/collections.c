#include "collections.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Default initial capacity for vectors
#define WYN_VEC_DEFAULT_CAPACITY 4

// Create new vector with specified item size
WynVec* wyn_vec_new(size_t item_size) {
    return wyn_vec_with_capacity(item_size, 0);
}

// Create vector with initial capacity
WynVec* wyn_vec_with_capacity(size_t item_size, size_t capacity) {
    if (item_size == 0) return NULL;
    
    WynVec* vec = malloc(sizeof(WynVec));
    if (!vec) return NULL;
    
    vec->len = 0;
    vec->item_size = item_size;
    vec->drop_fn = NULL;
    
    if (capacity > 0) {
        vec->data = malloc(item_size * capacity);
        if (!vec->data) {
            free(vec);
            return NULL;
        }
        vec->cap = capacity;
    } else {
        vec->data = NULL;
        vec->cap = 0;
    }
    
    return vec;
}

// Free vector and all its data
void wyn_vec_free(WynVec* vec) {
    if (!vec) return;
    
    // Call destructor for each item if provided
    if (vec->drop_fn) {
        for (size_t i = 0; i < vec->len; i++) {
            void* item = (char*)vec->data + (i * vec->item_size);
            vec->drop_fn(item);
        }
    }
    
    free(vec->data);
    free(vec);
}

// Set destructor function for items
void wyn_vec_set_drop_fn(WynVec* vec, void (*drop_fn)(void*)) {
    if (vec) {
        vec->drop_fn = drop_fn;
    }
}

// Grow vector capacity using exponential growth strategy
static bool wyn_vec_grow(WynVec* vec) {
    size_t new_cap = vec->cap == 0 ? WYN_VEC_DEFAULT_CAPACITY : vec->cap * 2;
    
    void* new_data = realloc(vec->data, vec->item_size * new_cap);
    if (!new_data) return false;
    
    vec->data = new_data;
    vec->cap = new_cap;
    return true;
}

// Push item to end of vector
bool wyn_vec_push(WynVec* vec, const void* item) {
    if (!vec || !item) return false;
    
    // Grow if necessary
    if (vec->len >= vec->cap) {
        if (!wyn_vec_grow(vec)) return false;
    }
    
    // Copy item to end
    void* dest = (char*)vec->data + (vec->len * vec->item_size);
    memcpy(dest, item, vec->item_size);
    vec->len++;
    
    return true;
}

// Pop item from end of vector
bool wyn_vec_pop(WynVec* vec, void* out_item) {
    if (!vec || vec->len == 0) return false;
    
    vec->len--;
    void* src = (char*)vec->data + (vec->len * vec->item_size);
    
    if (out_item) {
        memcpy(out_item, src, vec->item_size);
    } else if (vec->drop_fn) {
        vec->drop_fn(src);
    }
    
    return true;
}

// Get item at index
bool wyn_vec_get(WynVec* vec, size_t index, void* out_item) {
    if (!vec || !out_item || index >= vec->len) return false;
    
    void* src = (char*)vec->data + (index * vec->item_size);
    memcpy(out_item, src, vec->item_size);
    return true;
}

// Set item at index
bool wyn_vec_set(WynVec* vec, size_t index, const void* item) {
    if (!vec || !item || index >= vec->len) return false;
    
    void* dest = (char*)vec->data + (index * vec->item_size);
    
    // Call destructor on old item if provided
    if (vec->drop_fn) {
        vec->drop_fn(dest);
    }
    
    memcpy(dest, item, vec->item_size);
    return true;
}

// Insert item at index
bool wyn_vec_insert(WynVec* vec, size_t index, const void* item) {
    if (!vec || !item || index > vec->len) return false;
    
    // Grow if necessary
    if (vec->len >= vec->cap) {
        if (!wyn_vec_grow(vec)) return false;
    }
    
    // Shift elements to the right
    if (index < vec->len) {
        void* src = (char*)vec->data + (index * vec->item_size);
        void* dest = (char*)vec->data + ((index + 1) * vec->item_size);
        size_t bytes_to_move = (vec->len - index) * vec->item_size;
        memmove(dest, src, bytes_to_move);
    }
    
    // Insert new item
    void* dest = (char*)vec->data + (index * vec->item_size);
    memcpy(dest, item, vec->item_size);
    vec->len++;
    
    return true;
}

// Remove item at index
bool wyn_vec_remove(WynVec* vec, size_t index, void* out_item) {
    if (!vec || index >= vec->len) return false;
    
    void* item_ptr = (char*)vec->data + (index * vec->item_size);
    
    // Copy item to output if requested
    if (out_item) {
        memcpy(out_item, item_ptr, vec->item_size);
    } else if (vec->drop_fn) {
        vec->drop_fn(item_ptr);
    }
    
    // Shift elements to the left
    if (index < vec->len - 1) {
        void* src = (char*)vec->data + ((index + 1) * vec->item_size);
        void* dest = item_ptr;
        size_t bytes_to_move = (vec->len - index - 1) * vec->item_size;
        memmove(dest, src, bytes_to_move);
    }
    
    vec->len--;
    return true;
}

// Clear all items from vector
void wyn_vec_clear(WynVec* vec) {
    if (!vec) return;
    
    // Call destructor for each item if provided
    if (vec->drop_fn) {
        for (size_t i = 0; i < vec->len; i++) {
            void* item = (char*)vec->data + (i * vec->item_size);
            vec->drop_fn(item);
        }
    }
    
    vec->len = 0;
}

// Reserve additional capacity
bool wyn_vec_reserve(WynVec* vec, size_t additional) {
    if (!vec) return false;
    
    size_t required_cap = vec->len + additional;
    if (required_cap <= vec->cap) return true;
    
    void* new_data = realloc(vec->data, vec->item_size * required_cap);
    if (!new_data) return false;
    
    vec->data = new_data;
    vec->cap = required_cap;
    return true;
}

// Shrink capacity to fit current length
void wyn_vec_shrink_to_fit(WynVec* vec) {
    if (!vec || vec->len == vec->cap) return;
    
    if (vec->len == 0) {
        free(vec->data);
        vec->data = NULL;
        vec->cap = 0;
        return;
    }
    
    void* new_data = realloc(vec->data, vec->item_size * vec->len);
    if (new_data) {
        vec->data = new_data;
        vec->cap = vec->len;
    }
}

// Get vector length
size_t wyn_vec_len(const WynVec* vec) {
    return vec ? vec->len : 0;
}

// Get vector capacity
size_t wyn_vec_capacity(const WynVec* vec) {
    return vec ? vec->cap : 0;
}

// Check if vector is empty
bool wyn_vec_is_empty(const WynVec* vec) {
    return vec ? (vec->len == 0) : true;
}

// Get mutable pointer to data
void* wyn_vec_as_ptr(WynVec* vec) {
    return vec ? vec->data : NULL;
}

// Get const pointer to data
const void* wyn_vec_as_const_ptr(const WynVec* vec) {
    return vec ? vec->data : NULL;
}

// Create iterator for vector
WynVecIterator* wyn_vec_iter(WynVec* vec) {
    if (!vec) return NULL;
    
    WynVecIterator* iter = malloc(sizeof(WynVecIterator));
    if (!iter) return NULL;
    
    iter->vec = vec;
    iter->index = 0;
    return iter;
}

// Get next item from iterator
bool wyn_vec_iter_next(WynVecIterator* iter, void* out_item) {
    if (!iter || !out_item || iter->index >= iter->vec->len) return false;
    
    void* src = (char*)iter->vec->data + (iter->index * iter->vec->item_size);
    memcpy(out_item, src, iter->vec->item_size);
    iter->index++;
    
    return true;
}

// Free iterator
void wyn_vec_iter_free(WynVecIterator* iter) {
    free(iter);
}

// Check if vector contains item
bool wyn_vec_contains(WynVec* vec, const void* item, bool (*eq_fn)(const void*, const void*)) {
    if (!vec || !item || !eq_fn) return false;
    
    for (size_t i = 0; i < vec->len; i++) {
        void* vec_item = (char*)vec->data + (i * vec->item_size);
        if (eq_fn(vec_item, item)) {
            return true;
        }
    }
    
    return false;
}

// Sort vector using comparison function
void wyn_vec_sort(WynVec* vec, int (*cmp_fn)(const void*, const void*)) {
    if (!vec || !cmp_fn || vec->len <= 1) return;
    
    qsort(vec->data, vec->len, vec->item_size, cmp_fn);
}

// Clone vector
WynVec* wyn_vec_clone(const WynVec* vec, void* (*clone_fn)(const void*)) {
    if (!vec) return NULL;
    
    WynVec* new_vec = wyn_vec_with_capacity(vec->item_size, vec->cap);
    if (!new_vec) return NULL;
    
    new_vec->drop_fn = vec->drop_fn;
    
    if (clone_fn) {
        // Use custom clone function
        for (size_t i = 0; i < vec->len; i++) {
            void* src = (char*)vec->data + (i * vec->item_size);
            void* cloned = clone_fn(src);
            if (!wyn_vec_push(new_vec, cloned)) {
                wyn_vec_free(new_vec);
                return NULL;
            }
        }
    } else {
        // Simple memory copy
        if (vec->len > 0) {
            memcpy(new_vec->data, vec->data, vec->len * vec->item_size);
            new_vec->len = vec->len;
        }
    }
    
    return new_vec;
}

// Default initial capacity for hash maps
#define WYN_HASHMAP_DEFAULT_CAPACITY 16
#define WYN_HASHMAP_DEFAULT_LOAD_FACTOR 0.75

// Create new hash map
WynHashMap* wyn_hashmap_new(size_t key_size, size_t value_size, WynHashFn hash_fn, WynEqualsFn equals_fn) {
    return wyn_hashmap_with_capacity(key_size, value_size, WYN_HASHMAP_DEFAULT_CAPACITY, hash_fn, equals_fn);
}

// Create hash map with initial capacity
WynHashMap* wyn_hashmap_with_capacity(size_t key_size, size_t value_size, size_t capacity, WynHashFn hash_fn, WynEqualsFn equals_fn) {
    if (key_size == 0 || value_size == 0 || !hash_fn || !equals_fn) return NULL;
    
    WynHashMap* map = malloc(sizeof(WynHashMap));
    if (!map) return NULL;
    
    map->buckets = calloc(capacity, sizeof(WynHashMapBucket));
    if (!map->buckets) {
        free(map);
        return NULL;
    }
    
    // Initialize buckets
    for (size_t i = 0; i < capacity; i++) {
        map->buckets[i].key = malloc(key_size);
        map->buckets[i].value = malloc(value_size);
        map->buckets[i].occupied = false;
        map->buckets[i].distance = 0;
        
        if (!map->buckets[i].key || !map->buckets[i].value) {
            // Cleanup on failure
            for (size_t j = 0; j <= i; j++) {
                free(map->buckets[j].key);
                free(map->buckets[j].value);
            }
            free(map->buckets);
            free(map);
            return NULL;
        }
    }
    
    map->len = 0;
    map->cap = capacity;
    map->key_size = key_size;
    map->value_size = value_size;
    map->load_factor = WYN_HASHMAP_DEFAULT_LOAD_FACTOR;
    map->hash_fn = hash_fn;
    map->equals_fn = equals_fn;
    map->key_drop_fn = NULL;
    map->value_drop_fn = NULL;
    
    return map;
}

// Free hash map
void wyn_hashmap_free(WynHashMap* map) {
    if (!map) return;
    
    // Call destructors for occupied buckets
    for (size_t i = 0; i < map->cap; i++) {
        if (map->buckets[i].occupied) {
            if (map->key_drop_fn) {
                map->key_drop_fn(map->buckets[i].key);
            }
            if (map->value_drop_fn) {
                map->value_drop_fn(map->buckets[i].value);
            }
        }
        free(map->buckets[i].key);
        free(map->buckets[i].value);
    }
    
    free(map->buckets);
    free(map);
}

// Set key destructor
void wyn_hashmap_set_key_drop_fn(WynHashMap* map, void (*drop_fn)(void*)) {
    if (map) {
        map->key_drop_fn = drop_fn;
    }
}

// Set value destructor
void wyn_hashmap_set_value_drop_fn(WynHashMap* map, void (*drop_fn)(void*)) {
    if (map) {
        map->value_drop_fn = drop_fn;
    }
}

// Resize hash map (Robin Hood hashing)
static bool wyn_hashmap_resize(WynHashMap* map) {
    size_t old_cap = map->cap;
    WynHashMapBucket* old_buckets = map->buckets;
    
    // Double capacity
    map->cap = old_cap * 2;
    map->buckets = calloc(map->cap, sizeof(WynHashMapBucket));
    if (!map->buckets) {
        map->cap = old_cap;
        map->buckets = old_buckets;
        return false;
    }
    
    // Initialize new buckets
    for (size_t i = 0; i < map->cap; i++) {
        map->buckets[i].key = malloc(map->key_size);
        map->buckets[i].value = malloc(map->value_size);
        map->buckets[i].occupied = false;
        map->buckets[i].distance = 0;
        
        if (!map->buckets[i].key || !map->buckets[i].value) {
            // Cleanup on failure
            for (size_t j = 0; j <= i; j++) {
                free(map->buckets[j].key);
                free(map->buckets[j].value);
            }
            free(map->buckets);
            map->cap = old_cap;
            map->buckets = old_buckets;
            return false;
        }
    }
    
    // Rehash all elements
    map->len = 0;
    
    for (size_t i = 0; i < old_cap; i++) {
        if (old_buckets[i].occupied) {
            wyn_hashmap_insert(map, old_buckets[i].key, old_buckets[i].value, NULL);
        }
        free(old_buckets[i].key);
        free(old_buckets[i].value);
    }
    
    free(old_buckets);
    return true;
}

// Insert key-value pair (Robin Hood hashing)
bool wyn_hashmap_insert(WynHashMap* map, const void* key, const void* value, void* old_value) {
    if (!map || !key || !value) return false;
    
    // Check if resize is needed
    if ((double)map->len / (double)map->cap >= map->load_factor) {
        if (!wyn_hashmap_resize(map)) return false;
    }
    
    size_t hash = map->hash_fn(key);
    size_t index = hash % map->cap;
    size_t distance = 0;
    
    // Prepare insertion data
    void* insert_key = malloc(map->key_size);
    void* insert_value = malloc(map->value_size);
    if (!insert_key || !insert_value) {
        free(insert_key);
        free(insert_value);
        return false;
    }
    
    memcpy(insert_key, key, map->key_size);
    memcpy(insert_value, value, map->value_size);
    
    while (true) {
        WynHashMapBucket* bucket = &map->buckets[index];
        
        if (!bucket->occupied) {
            // Empty slot - insert here
            memcpy(bucket->key, insert_key, map->key_size);
            memcpy(bucket->value, insert_value, map->value_size);
            bucket->distance = distance;
            bucket->occupied = true;
            map->len++;
            
            free(insert_key);
            free(insert_value);
            return true;
        }
        
        // Check if key already exists
        if (map->equals_fn(bucket->key, insert_key)) {
            // Update existing key
            if (old_value) {
                memcpy(old_value, bucket->value, map->value_size);
            } else if (map->value_drop_fn) {
                map->value_drop_fn(bucket->value);
            }
            
            memcpy(bucket->value, insert_value, map->value_size);
            
            free(insert_key);
            free(insert_value);
            return true;
        }
        
        // Robin Hood hashing: if our distance is greater, steal this slot
        if (distance > bucket->distance) {
            // Swap our data with bucket data
            void* temp_key = malloc(map->key_size);
            void* temp_value = malloc(map->value_size);
            size_t temp_distance = distance;
            
            memcpy(temp_key, insert_key, map->key_size);
            memcpy(temp_value, insert_value, map->value_size);
            
            memcpy(insert_key, bucket->key, map->key_size);
            memcpy(insert_value, bucket->value, map->value_size);
            distance = bucket->distance;
            
            memcpy(bucket->key, temp_key, map->key_size);
            memcpy(bucket->value, temp_value, map->value_size);
            bucket->distance = temp_distance;
            
            free(temp_key);
            free(temp_value);
        }
        
        // Move to next slot
        index = (index + 1) % map->cap;
        distance++;
    }
}

// Get value by key
bool wyn_hashmap_get(WynHashMap* map, const void* key, void* out_value) {
    if (!map || !key || !out_value) return false;
    
    size_t hash = map->hash_fn(key);
    size_t index = hash % map->cap;
    size_t distance = 0;
    
    while (distance <= map->cap) {
        WynHashMapBucket* bucket = &map->buckets[index];
        
        if (!bucket->occupied || bucket->distance < distance) {
            // Key not found
            return false;
        }
        
        if (map->equals_fn(bucket->key, key)) {
            // Found key
            memcpy(out_value, bucket->value, map->value_size);
            return true;
        }
        
        index = (index + 1) % map->cap;
        distance++;
    }
    
    return false;
}

// Remove key-value pair
bool wyn_hashmap_remove(WynHashMap* map, const void* key, void* out_value) {
    if (!map || !key) return false;
    
    size_t hash = map->hash_fn(key);
    size_t index = hash % map->cap;
    size_t distance = 0;
    
    while (distance <= map->cap) {
        WynHashMapBucket* bucket = &map->buckets[index];
        
        if (!bucket->occupied || bucket->distance < distance) {
            // Key not found
            return false;
        }
        
        if (map->equals_fn(bucket->key, key)) {
            // Found key - remove it
            if (out_value) {
                memcpy(out_value, bucket->value, map->value_size);
            } else if (map->value_drop_fn) {
                map->value_drop_fn(bucket->value);
            }
            
            if (map->key_drop_fn) {
                map->key_drop_fn(bucket->key);
            }
            
            bucket->occupied = false;
            bucket->distance = 0;
            map->len--;
            
            // Shift subsequent elements back (Robin Hood cleanup)
            size_t next_index = (index + 1) % map->cap;
            while (map->buckets[next_index].occupied && map->buckets[next_index].distance > 0) {
                WynHashMapBucket* next_bucket = &map->buckets[next_index];
                WynHashMapBucket* current_bucket = &map->buckets[index];
                
                // Move next bucket to current position
                memcpy(current_bucket->key, next_bucket->key, map->key_size);
                memcpy(current_bucket->value, next_bucket->value, map->value_size);
                current_bucket->distance = next_bucket->distance - 1;
                current_bucket->occupied = true;
                
                // Clear next bucket
                next_bucket->occupied = false;
                next_bucket->distance = 0;
                
                index = next_index;
                next_index = (next_index + 1) % map->cap;
            }
            
            return true;
        }
        
        index = (index + 1) % map->cap;
        distance++;
    }
    
    return false;
}

// Check if key exists
bool wyn_hashmap_contains_key(WynHashMap* map, const void* key) {
    if (!map || !key) return false;
    
    size_t hash = map->hash_fn(key);
    size_t index = hash % map->cap;
    size_t distance = 0;
    
    while (distance <= map->cap) {
        WynHashMapBucket* bucket = &map->buckets[index];
        
        if (!bucket->occupied || bucket->distance < distance) {
            return false;
        }
        
        if (map->equals_fn(bucket->key, key)) {
            return true;
        }
        
        index = (index + 1) % map->cap;
        distance++;
    }
    
    return false;
}

// Clear all entries
void wyn_hashmap_clear(WynHashMap* map) {
    if (!map) return;
    
    for (size_t i = 0; i < map->cap; i++) {
        if (map->buckets[i].occupied) {
            if (map->key_drop_fn) {
                map->key_drop_fn(map->buckets[i].key);
            }
            if (map->value_drop_fn) {
                map->value_drop_fn(map->buckets[i].value);
            }
            map->buckets[i].occupied = false;
            map->buckets[i].distance = 0;
        }
    }
    
    map->len = 0;
}

// Get hash map length
size_t wyn_hashmap_len(const WynHashMap* map) {
    return map ? map->len : 0;
}

// Get hash map capacity
size_t wyn_hashmap_capacity(const WynHashMap* map) {
    return map ? map->cap : 0;
}

// Check if hash map is empty
bool wyn_hashmap_is_empty(const WynHashMap* map) {
    return map ? (map->len == 0) : true;
}

// Get current load factor
double wyn_hashmap_load_factor(const WynHashMap* map) {
    return map ? ((double)map->len / (double)map->cap) : 0.0;
}

// Create iterator for hash map
WynHashMapIterator* wyn_hashmap_iter(WynHashMap* map) {
    if (!map) return NULL;
    
    WynHashMapIterator* iter = malloc(sizeof(WynHashMapIterator));
    if (!iter) return NULL;
    
    iter->map = map;
    iter->index = 0;
    
    // Find first occupied bucket
    while (iter->index < map->cap && !map->buckets[iter->index].occupied) {
        iter->index++;
    }
    
    return iter;
}

// Get next key-value pair from iterator
bool wyn_hashmap_iter_next(WynHashMapIterator* iter, void* out_key, void* out_value) {
    if (!iter || !out_key || !out_value || iter->index >= iter->map->cap) return false;
    
    WynHashMapBucket* bucket = &iter->map->buckets[iter->index];
    if (!bucket->occupied) return false;
    
    // Copy current key-value pair
    memcpy(out_key, bucket->key, iter->map->key_size);
    memcpy(out_value, bucket->value, iter->map->value_size);
    
    // Find next occupied bucket
    iter->index++;
    while (iter->index < iter->map->cap && !iter->map->buckets[iter->index].occupied) {
        iter->index++;
    }
    
    return true;
}

// Free iterator
void wyn_hashmap_iter_free(WynHashMapIterator* iter) {
    free(iter);
}

// Common hash functions
size_t wyn_hash_int(const void* key) {
    int k = *(const int*)key;
    // Simple hash function for integers
    k = ((k >> 16) ^ k) * 0x45d9f3b;
    k = ((k >> 16) ^ k) * 0x45d9f3b;
    k = (k >> 16) ^ k;
    return (size_t)k;
}

size_t wyn_hash_string(const void* key) {
    const char* str = *(const char**)key;
    return wyn_hash_bytes(str, strlen(str));
}

size_t wyn_hash_bytes(const void* data, size_t len) {
    // FNV-1a hash
    const unsigned char* bytes = (const unsigned char*)data;
    size_t hash = 2166136261u;
    
    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    
    return hash;
}

bool wyn_equals_int(const void* a, const void* b) {
    return *(const int*)a == *(const int*)b;
}

bool wyn_equals_string(const void* a, const void* b) {
    const char* str_a = *(const char**)a;
    const char* str_b = *(const char**)b;
    return strcmp(str_a, str_b) == 0;
}

// Dummy value for HashSet (we only care about keys)
static const char HASHSET_DUMMY_VALUE = 1;

// Create new hash set
WynHashSet* wyn_hashset_new(size_t item_size, WynHashFn hash_fn, WynEqualsFn equals_fn) {
    return wyn_hashset_with_capacity(item_size, WYN_HASHMAP_DEFAULT_CAPACITY, hash_fn, equals_fn);
}

// Create hash set with initial capacity
WynHashSet* wyn_hashset_with_capacity(size_t item_size, size_t capacity, WynHashFn hash_fn, WynEqualsFn equals_fn) {
    if (item_size == 0 || !hash_fn || !equals_fn) return NULL;
    
    WynHashSet* set = malloc(sizeof(WynHashSet));
    if (!set) return NULL;
    
    // Create internal HashMap with dummy values
    set->map = wyn_hashmap_with_capacity(item_size, sizeof(char), capacity, hash_fn, equals_fn);
    if (!set->map) {
        free(set);
        return NULL;
    }
    
    set->item_size = item_size;
    return set;
}

// Free hash set
void wyn_hashset_free(WynHashSet* set) {
    if (!set) return;
    
    wyn_hashmap_free(set->map);
    free(set);
}

// Set destructor function for items
void wyn_hashset_set_drop_fn(WynHashSet* set, void (*drop_fn)(void*)) {
    if (set && set->map) {
        wyn_hashmap_set_key_drop_fn(set->map, drop_fn);
    }
}

// Insert item into set
bool wyn_hashset_insert(WynHashSet* set, const void* item) {
    if (!set || !item) return false;
    
    // Check if item already exists
    if (wyn_hashmap_contains_key(set->map, item)) {
        return false;  // Item already exists, not a new insertion
    }
    
    // Insert with dummy value
    char dummy;
    wyn_hashmap_insert(set->map, item, &HASHSET_DUMMY_VALUE, &dummy);
    return true;  // New insertion
}

// Remove item from set
bool wyn_hashset_remove(WynHashSet* set, const void* item) {
    if (!set || !item) return false;
    
    char dummy;
    return wyn_hashmap_remove(set->map, item, &dummy);
}

// Check if set contains item
bool wyn_hashset_contains(WynHashSet* set, const void* item) {
    if (!set || !item) return false;
    
    return wyn_hashmap_contains_key(set->map, item);
}

// Clear all items from set
void wyn_hashset_clear(WynHashSet* set) {
    if (set && set->map) {
        wyn_hashmap_clear(set->map);
    }
}

// Get set length
size_t wyn_hashset_len(const WynHashSet* set) {
    return set ? wyn_hashmap_len(set->map) : 0;
}

// Get set capacity
size_t wyn_hashset_capacity(const WynHashSet* set) {
    return set ? wyn_hashmap_capacity(set->map) : 0;
}

// Check if set is empty
bool wyn_hashset_is_empty(const WynHashSet* set) {
    return set ? wyn_hashmap_is_empty(set->map) : true;
}

// Get current load factor
double wyn_hashset_load_factor(const WynHashSet* set) {
    return set ? wyn_hashmap_load_factor(set->map) : 0.0;
}

// Create iterator for hash set
WynHashSetIterator* wyn_hashset_iter(WynHashSet* set) {
    if (!set) return NULL;
    
    WynHashSetIterator* iter = malloc(sizeof(WynHashSetIterator));
    if (!iter) return NULL;
    
    iter->map_iter = wyn_hashmap_iter(set->map);
    if (!iter->map_iter) {
        free(iter);
        return NULL;
    }
    
    iter->dummy_value = malloc(sizeof(char));
    if (!iter->dummy_value) {
        wyn_hashmap_iter_free(iter->map_iter);
        free(iter);
        return NULL;
    }
    
    return iter;
}

// Get next item from iterator
bool wyn_hashset_iter_next(WynHashSetIterator* iter, void* out_item) {
    if (!iter || !out_item) return false;
    
    // Use the map iterator, but only return the key (ignore dummy value)
    return wyn_hashmap_iter_next(iter->map_iter, out_item, iter->dummy_value);
}

// Free iterator
void wyn_hashset_iter_free(WynHashSetIterator* iter) {
    if (!iter) return;
    
    wyn_hashmap_iter_free(iter->map_iter);
    free(iter->dummy_value);
    free(iter);
}

// Union of two sets
WynHashSet* wyn_hashset_union(WynHashSet* set1, WynHashSet* set2) {
    if (!set1 || !set2 || set1->item_size != set2->item_size) return NULL;
    
    // Create new set with capacity for both sets
    size_t union_capacity = wyn_hashset_len(set1) + wyn_hashset_len(set2);
    WynHashSet* result = wyn_hashset_with_capacity(
        set1->item_size, 
        union_capacity,
        set1->map->hash_fn,
        set1->map->equals_fn
    );
    
    if (!result) return NULL;
    
    // Add all items from set1
    WynHashSetIterator* iter1 = wyn_hashset_iter(set1);
    if (iter1) {
        void* item = malloc(set1->item_size);
        if (item) {
            while (wyn_hashset_iter_next(iter1, item)) {
                wyn_hashset_insert(result, item);
            }
            free(item);
        }
        wyn_hashset_iter_free(iter1);
    }
    
    // Add all items from set2
    WynHashSetIterator* iter2 = wyn_hashset_iter(set2);
    if (iter2) {
        void* item = malloc(set2->item_size);
        if (item) {
            while (wyn_hashset_iter_next(iter2, item)) {
                wyn_hashset_insert(result, item);
            }
            free(item);
        }
        wyn_hashset_iter_free(iter2);
    }
    
    return result;
}

// Intersection of two sets
WynHashSet* wyn_hashset_intersection(WynHashSet* set1, WynHashSet* set2) {
    if (!set1 || !set2 || set1->item_size != set2->item_size) return NULL;
    
    // Create new set
    WynHashSet* result = wyn_hashset_new(
        set1->item_size,
        set1->map->hash_fn,
        set1->map->equals_fn
    );
    
    if (!result) return NULL;
    
    // Iterate through smaller set for efficiency
    WynHashSet* smaller = wyn_hashset_len(set1) <= wyn_hashset_len(set2) ? set1 : set2;
    WynHashSet* larger = (smaller == set1) ? set2 : set1;
    
    WynHashSetIterator* iter = wyn_hashset_iter(smaller);
    if (iter) {
        void* item = malloc(smaller->item_size);
        if (item) {
            while (wyn_hashset_iter_next(iter, item)) {
                if (wyn_hashset_contains(larger, item)) {
                    wyn_hashset_insert(result, item);
                }
            }
            free(item);
        }
        wyn_hashset_iter_free(iter);
    }
    
    return result;
}

// Difference of two sets (set1 - set2)
WynHashSet* wyn_hashset_difference(WynHashSet* set1, WynHashSet* set2) {
    if (!set1 || !set2 || set1->item_size != set2->item_size) return NULL;
    
    // Create new set
    WynHashSet* result = wyn_hashset_new(
        set1->item_size,
        set1->map->hash_fn,
        set1->map->equals_fn
    );
    
    if (!result) return NULL;
    
    // Add items from set1 that are not in set2
    WynHashSetIterator* iter = wyn_hashset_iter(set1);
    if (iter) {
        void* item = malloc(set1->item_size);
        if (item) {
            while (wyn_hashset_iter_next(iter, item)) {
                if (!wyn_hashset_contains(set2, item)) {
                    wyn_hashset_insert(result, item);
                }
            }
            free(item);
        }
        wyn_hashset_iter_free(iter);
    }
    
    return result;
}

// Check if subset is a subset of superset
bool wyn_hashset_is_subset(WynHashSet* subset, WynHashSet* superset) {
    if (!subset || !superset || subset->item_size != superset->item_size) return false;
    
    // Empty set is subset of any set
    if (wyn_hashset_is_empty(subset)) return true;
    
    // If subset is larger, it can't be a subset
    if (wyn_hashset_len(subset) > wyn_hashset_len(superset)) return false;
    
    // Check if all items in subset are in superset
    WynHashSetIterator* iter = wyn_hashset_iter(subset);
    if (!iter) return false;
    
    bool is_subset = true;
    void* item = malloc(subset->item_size);
    if (item) {
        while (wyn_hashset_iter_next(iter, item)) {
            if (!wyn_hashset_contains(superset, item)) {
                is_subset = false;
                break;
            }
        }
        free(item);
    } else {
        is_subset = false;
    }
    
    wyn_hashset_iter_free(iter);
    return is_subset;
}

// Check if two sets are disjoint (no common elements)
bool wyn_hashset_is_disjoint(WynHashSet* set1, WynHashSet* set2) {
    if (!set1 || !set2 || set1->item_size != set2->item_size) return true;
    
    // Empty sets are disjoint with any set
    if (wyn_hashset_is_empty(set1) || wyn_hashset_is_empty(set2)) return true;
    
    // Iterate through smaller set for efficiency
    WynHashSet* smaller = wyn_hashset_len(set1) <= wyn_hashset_len(set2) ? set1 : set2;
    WynHashSet* larger = (smaller == set1) ? set2 : set1;
    
    WynHashSetIterator* iter = wyn_hashset_iter(smaller);
    if (!iter) return true;
    
    bool is_disjoint = true;
    void* item = malloc(smaller->item_size);
    if (item) {
        while (wyn_hashset_iter_next(iter, item)) {
            if (wyn_hashset_contains(larger, item)) {
                is_disjoint = false;
                break;
            }
        }
        free(item);
    }
    
    wyn_hashset_iter_free(iter);
    return is_disjoint;
}
