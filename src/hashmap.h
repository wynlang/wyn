#ifndef WYN_HASHMAP_H
#define WYN_HASHMAP_H

#include <stddef.h>

typedef struct WynHashMap WynHashMap;

// Value types for HashMap
typedef enum {
    HASHMAP_INT,
    HASHMAP_FLOAT,
    HASHMAP_STRING,
    HASHMAP_BOOL
} HashMapValueType;

// Tagged union for HashMap values
typedef struct {
    HashMapValueType type;
    union {
        int as_int;
        double as_float;
        char* as_string;
        int as_bool;
    } value;
} HashMapValue;

WynHashMap* hashmap_new(void);

// Type-specific insert functions
void hashmap_insert_int(WynHashMap* map, const char* key, int value);
void hashmap_insert_float(WynHashMap* map, const char* key, double value);
void hashmap_insert_string(WynHashMap* map, const char* key, const char* value);
void hashmap_insert_bool(WynHashMap* map, const char* key, int value);

// Generic get (returns HashMapValue)
HashMapValue hashmap_get(WynHashMap* map, const char* key);

// Type-specific get functions
int hashmap_get_int(WynHashMap* map, const char* key);
double hashmap_get_float(WynHashMap* map, const char* key);
char* hashmap_get_string(WynHashMap* map, const char* key);
int hashmap_get_bool(WynHashMap* map, const char* key);

void hashmap_remove(WynHashMap* map, const char* key);
int hashmap_has(WynHashMap* map, const char* key);
void hashmap_free(WynHashMap* map);

// Legacy compatibility (defaults to int)
void hashmap_insert(WynHashMap* map, const char* key, int value);

#endif
