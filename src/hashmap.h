#ifndef WYN_HASHMAP_H
#define WYN_HASHMAP_H

#include <stddef.h>
#include <stdbool.h>

typedef struct WynHashMap WynHashMap;

// Value types for HashMap
typedef enum {
    HASHMAP_INT,
    HASHMAP_FLOAT,
    HASHMAP_STRING,
    HASHMAP_BOOL,
    HASHMAP_PTR
} HashMapValueType;

// Tagged union for HashMap values
typedef struct {
    HashMapValueType type;
    union {
        int as_int;
        double as_float;
        char* as_string;
        int as_bool;
        void* as_ptr;
    } value;
} HashMapValue;

WynHashMap* hashmap_new(void);

// Type-specific insert functions
void hashmap_insert_int(WynHashMap* map, const char* key, int value);
void hashmap_insert_float(WynHashMap* map, const char* key, double value);
void hashmap_insert_string(WynHashMap* map, const char* key, const char* value);
void hashmap_insert_bool(WynHashMap* map, const char* key, int value);
void hashmap_insert_ptr(WynHashMap* map, const char* key, void* value);

// Render as {"k": v, ...} into a caller buffer; returns the length that WOULD be
// written, snprintf-style, so callers can size then fill. Defined in hashmap.c
// because only that file sees struct WynHashMap and the real value type tags.
int hashmap_format(WynHashMap* map, char* out, size_t cap);

// Generic get (returns HashMapValue)
HashMapValue hashmap_get(WynHashMap* map, const char* key);

// Type-specific get functions
int hashmap_get_int(WynHashMap* map, const char* key);
double hashmap_get_float(WynHashMap* map, const char* key);
char* hashmap_get_string(WynHashMap* map, const char* key);
int hashmap_get_bool(WynHashMap* map, const char* key);
void* hashmap_get_ptr(WynHashMap* map, const char* key);

// Index-read getters for `m[k]`: panic (with file/line/key) on a missing key,
// rather than silently returning 0/"". `.get`/`.has` do NOT use these.
int    hashmap_index_int_impl(WynHashMap* map, const char* key, const char* file, int line);
double hashmap_index_float_impl(WynHashMap* map, const char* key, const char* file, int line);
char*  hashmap_index_string_impl(WynHashMap* map, const char* key, const char* file, int line);
int    hashmap_index_bool_impl(WynHashMap* map, const char* key, const char* file, int line);

void hashmap_remove(WynHashMap* map, const char* key);
bool hashmap_has(WynHashMap* map, const char* key);
int hashmap_len(WynHashMap* map);
void hashmap_free(WynHashMap* map);

// Legacy compatibility (defaults to int)
void hashmap_insert(WynHashMap* map, const char* key, int value);

// Codegen aliases
void hashmap_set(WynHashMap* map, const char* key, const char* value);

#endif
