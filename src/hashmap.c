#define _POSIX_C_SOURCE 200809L
#include "hashmap.h"
#include <stdlib.h>
#include <string.h>

#define HASHMAP_SIZE 128

typedef struct Entry {
    char* key;
    HashMapValue value;
    struct Entry* next;
} Entry;

struct WynHashMap {
    Entry* buckets[HASHMAP_SIZE];
};

static unsigned int hash(const char* key) {
    unsigned int h = 0;
    while (*key) {
        h = h * 31 + *key++;
    }
    return h % HASHMAP_SIZE;
}

WynHashMap* hashmap_new(void) {
    WynHashMap* map = calloc(1, sizeof(WynHashMap));
    return map;
}

void hashmap_insert_int(WynHashMap* map, const char* key, int value) {
    unsigned int idx = hash(key);
    Entry* entry = map->buckets[idx];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value.type = HASHMAP_INT;
            entry->value.value.as_int = value;
            return;
        }
        entry = entry->next;
    }
    
    Entry* new_entry = malloc(sizeof(Entry));
    new_entry->key = strdup(key);
    new_entry->value.type = HASHMAP_INT;
    new_entry->value.value.as_int = value;
    new_entry->next = map->buckets[idx];
    map->buckets[idx] = new_entry;
}

void hashmap_insert_float(WynHashMap* map, const char* key, double value) {
    unsigned int idx = hash(key);
    Entry* entry = map->buckets[idx];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value.type = HASHMAP_FLOAT;
            entry->value.value.as_float = value;
            return;
        }
        entry = entry->next;
    }
    
    Entry* new_entry = malloc(sizeof(Entry));
    new_entry->key = strdup(key);
    new_entry->value.type = HASHMAP_FLOAT;
    new_entry->value.value.as_float = value;
    new_entry->next = map->buckets[idx];
    map->buckets[idx] = new_entry;
}

void hashmap_insert_string(WynHashMap* map, const char* key, const char* value) {
    unsigned int idx = hash(key);
    Entry* entry = map->buckets[idx];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (entry->value.type == HASHMAP_STRING) {
                free(entry->value.value.as_string);
            }
            entry->value.type = HASHMAP_STRING;
            entry->value.value.as_string = strdup(value);
            return;
        }
        entry = entry->next;
    }
    
    Entry* new_entry = malloc(sizeof(Entry));
    new_entry->key = strdup(key);
    new_entry->value.type = HASHMAP_STRING;
    new_entry->value.value.as_string = strdup(value);
    new_entry->next = map->buckets[idx];
    map->buckets[idx] = new_entry;
}

void hashmap_insert_bool(WynHashMap* map, const char* key, int value) {
    unsigned int idx = hash(key);
    Entry* entry = map->buckets[idx];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value.type = HASHMAP_BOOL;
            entry->value.value.as_bool = value;
            return;
        }
        entry = entry->next;
    }
    
    Entry* new_entry = malloc(sizeof(Entry));
    new_entry->key = strdup(key);
    new_entry->value.type = HASHMAP_BOOL;
    new_entry->value.value.as_bool = value;
    new_entry->next = map->buckets[idx];
    map->buckets[idx] = new_entry;
}

HashMapValue hashmap_get(WynHashMap* map, const char* key) {
    unsigned int idx = hash(key);
    Entry* entry = map->buckets[idx];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    
    // Return default value (int 0) if not found
    HashMapValue default_val;
    default_val.type = HASHMAP_INT;
    default_val.value.as_int = -1;
    return default_val;
}

int hashmap_get_int(WynHashMap* map, const char* key) {
    HashMapValue val = hashmap_get(map, key);
    if (val.type == HASHMAP_INT) {
        return val.value.as_int;
    }
    return -1;
}

double hashmap_get_float(WynHashMap* map, const char* key) {
    HashMapValue val = hashmap_get(map, key);
    if (val.type == HASHMAP_FLOAT) {
        return val.value.as_float;
    }
    return 0.0;
}

char* hashmap_get_string(WynHashMap* map, const char* key) {
    HashMapValue val = hashmap_get(map, key);
    if (val.type == HASHMAP_STRING) {
        return val.value.as_string;
    }
    return "";
}

int hashmap_get_bool(WynHashMap* map, const char* key) {
    HashMapValue val = hashmap_get(map, key);
    if (val.type == HASHMAP_BOOL) {
        return val.value.as_bool;
    }
    return 0;
}

int hashmap_has(WynHashMap* map, const char* key) {
    unsigned int idx = hash(key);
    Entry* entry = map->buckets[idx];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return 1;
        }
        entry = entry->next;
    }
    
    return 0;
}

void hashmap_remove(WynHashMap* map, const char* key) {
    unsigned int idx = hash(key);
    Entry* entry = map->buckets[idx];
    Entry* prev = NULL;
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (prev) {
                prev->next = entry->next;
            } else {
                map->buckets[idx] = entry->next;
            }
            free(entry->key);
            if (entry->value.type == HASHMAP_STRING) {
                free(entry->value.value.as_string);
            }
            free(entry);
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

void hashmap_free(WynHashMap* map) {
    for (int i = 0; i < HASHMAP_SIZE; i++) {
        Entry* entry = map->buckets[i];
        while (entry) {
            Entry* next = entry->next;
            free(entry->key);
            if (entry->value.type == HASHMAP_STRING) {
                free(entry->value.value.as_string);
            }
            free(entry);
            entry = next;
        }
    }
    free(map);
}

// Legacy compatibility
void hashmap_insert(WynHashMap* map, const char* key, int value) {
    hashmap_insert_int(map, key, value);
}
