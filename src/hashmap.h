#ifndef WYN_HASHMAP_H
#define WYN_HASHMAP_H

#include <stddef.h>

typedef struct WynHashMap WynHashMap;

WynHashMap* hashmap_new(void);
void hashmap_insert(WynHashMap* map, const char* key, int value);
int hashmap_get(WynHashMap* map, const char* key);
void hashmap_free(WynHashMap* map);

#endif
