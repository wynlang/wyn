#include "hashmap.h"
#include <stdlib.h>
#include <string.h>

#define HASHMAP_SIZE 128

typedef struct Entry {
    char* key;
    int value;
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

void hashmap_insert(WynHashMap* map, const char* key, int value) {
    unsigned int idx = hash(key);
    Entry* entry = map->buckets[idx];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    
    Entry* new_entry = malloc(sizeof(Entry));
    new_entry->key = strdup(key);
    new_entry->value = value;
    new_entry->next = map->buckets[idx];
    map->buckets[idx] = new_entry;
}

int hashmap_get(WynHashMap* map, const char* key) {
    unsigned int idx = hash(key);
    Entry* entry = map->buckets[idx];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    
    return -1;
}

void hashmap_free(WynHashMap* map) {
    for (int i = 0; i < HASHMAP_SIZE; i++) {
        Entry* entry = map->buckets[i];
        while (entry) {
            Entry* next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    free(map);
}
