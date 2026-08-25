#define _POSIX_C_SOURCE 200809L
#include "hashset.h"
#include "wyn_write_guard.h"
#include <stdlib.h>
#include <string.h>

#define HASHSET_SIZE 128

typedef struct Entry {
    char* key;
    struct Entry* next;
} Entry;

struct WynHashSet {
    Entry* buckets[HASHSET_SIZE];
    // Concurrent-mutation flag - see hashmap.c and wyn_write_guard.h.
    int writing;
};

static unsigned int hash(const char* key) {
    unsigned int h = 0;
    while (*key) {
        h = h * 31 + *key++;
    }
    return h % HASHSET_SIZE;
}

WynHashSet* hashset_new(void) {
    WynHashSet* set = calloc(1, sizeof(WynHashSet));
    return set;
}

static void hashset_add_impl(WynHashSet* set, const char* key) {
    unsigned int idx = hash(key);
    Entry* entry = set->buckets[idx];
    
    // Check if already exists
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return; // Already in set
        }
        entry = entry->next;
    }
    
    // Add new entry
    Entry* new_entry = malloc(sizeof(Entry));
    new_entry->key = strdup(key);
    new_entry->next = set->buckets[idx];
    set->buckets[idx] = new_entry;
}

int hashset_contains(WynHashSet* set, const char* key) {
    unsigned int idx = hash(key);
    Entry* entry = set->buckets[idx];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return 1;
        }
        entry = entry->next;
    }
    
    return 0;
}

static void hashset_remove_impl(WynHashSet* set, const char* key) {
    unsigned int idx = hash(key);
    Entry* entry = set->buckets[idx];
    Entry* prev = NULL;
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (prev) {
                prev->next = entry->next;
            } else {
                set->buckets[idx] = entry->next;
            }
            free(entry->key);
            free(entry);
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

void hashset_free(WynHashSet* set) {
    for (int i = 0; i < HASHSET_SIZE; i++) {
        Entry* entry = set->buckets[i];
        while (entry) {
            Entry* next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    free(set);
}

// Wrapper functions
void wyn_hashset_insert(WynHashSet* set, const char* key) {
    hashset_add(set, key);
}

int wyn_hashset_contains(WynHashSet* set, const char* key) {
    return hashset_contains(set, key);
}

void wyn_hashset_remove(WynHashSet* set, const char* key) {
    hashset_remove(set, key);
}

int wyn_hashset_len(WynHashSet* set) {
    int count = 0;
    for (int i = 0; i < HASHSET_SIZE; i++) {
        Entry* entry = set->buckets[i];
        while (entry) {
            count++;
            entry = entry->next;
        }
    }
    return count;
}

int wyn_hashset_is_empty(WynHashSet* set) {
    return wyn_hashset_len(set) == 0;
}

void wyn_hashset_clear(WynHashSet* set) {
    for (int i = 0; i < HASHSET_SIZE; i++) {
        Entry* entry = set->buckets[i];
        while (entry) {
            Entry* next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
        set->buckets[i] = NULL;
    }
}

WynHashSet* wyn_hashset_union(WynHashSet* set1, WynHashSet* set2) {
    WynHashSet* result = hashset_new();
    for (int i = 0; i < HASHSET_SIZE; i++) {
        Entry* entry = set1->buckets[i];
        while (entry) {
            hashset_add(result, entry->key);
            entry = entry->next;
        }
        entry = set2->buckets[i];
        while (entry) {
            hashset_add(result, entry->key);
            entry = entry->next;
        }
    }
    return result;
}

WynHashSet* wyn_hashset_intersection(WynHashSet* set1, WynHashSet* set2) {
    WynHashSet* result = hashset_new();
    for (int i = 0; i < HASHSET_SIZE; i++) {
        Entry* entry = set1->buckets[i];
        while (entry) {
            if (hashset_contains(set2, entry->key)) {
                hashset_add(result, entry->key);
            }
            entry = entry->next;
        }
    }
    return result;
}

WynHashSet* wyn_hashset_difference(WynHashSet* set1, WynHashSet* set2) {
    WynHashSet* result = hashset_new();
    for (int i = 0; i < HASHSET_SIZE; i++) {
        Entry* entry = set1->buckets[i];
        while (entry) {
            if (!hashset_contains(set2, entry->key)) {
                hashset_add(result, entry->key);
            }
            entry = entry->next;
        }
    }
    return result;
}

int wyn_hashset_is_subset(WynHashSet* set1, WynHashSet* set2) {
    for (int i = 0; i < HASHSET_SIZE; i++) {
        Entry* entry = set1->buckets[i];
        while (entry) {
            if (!hashset_contains(set2, entry->key)) {
                return 0;
            }
            entry = entry->next;
        }
    }
    return 1;
}

int wyn_hashset_is_disjoint(WynHashSet* set1, WynHashSet* set2) {
    for (int i = 0; i < HASHSET_SIZE; i++) {
        Entry* entry = set1->buckets[i];
        while (entry) {
            if (hashset_contains(set2, entry->key)) {
                return 0;
            }
            entry = entry->next;
        }
    }
    return 1;
}

void hashset_add(WynHashSet* set, const char* key) {
    if (!set) return;
    WYN_COLL_WRITE_ENTER(&set->writing, "HashSet");
    hashset_add_impl(set, key);
    WYN_COLL_WRITE_EXIT(&set->writing);
}

void hashset_remove(WynHashSet* set, const char* key) {
    if (!set) return;
    WYN_COLL_WRITE_ENTER(&set->writing, "HashSet");
    hashset_remove_impl(set, key);
    WYN_COLL_WRITE_EXIT(&set->writing);
}
