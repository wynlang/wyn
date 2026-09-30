#define _POSIX_C_SOURCE 200809L
#include "hashset.h"
#include "wyn_write_guard.h"
#include <stdlib.h>
#include <string.h>

#define HASHSET_SIZE 128

// V-38 (#391): a TAGGED entry. The tag participates in both the hash and the
// equality test, which is what keeps `{:1}` and `{:"1"}` distinct - the reason
// stringify-into-the-string-table was rejected as an implementation.
typedef struct Entry {
    HashSetElemType type;
    union {
        char* as_string;      // owned (strdup); freed on remove/clear/free
        long long as_int;
        double as_float;
        int as_bool;
    } v;
    struct Entry* next;
} Entry;

struct WynHashSet {
    Entry* buckets[HASHSET_SIZE];
    // Concurrent-mutation flag - see hashmap.c and wyn_write_guard.h.
    int writing;
};

static unsigned int hash_str(const char* key) {
    unsigned int h = 0;
    while (*key) {
        h = h * 31 + (unsigned char)*key++;
    }
    return h;
}

// Hash of the raw bytes of a scalar. A double is hashed by its BIT PATTERN, so
// 1.0 and 1 land in different buckets (different tags) and 1.0 always lands in its
// own - no float-to-string rounding is involved anywhere in the set.
static unsigned int hash_bytes(const void* p, size_t n) {
    const unsigned char* b = (const unsigned char*)p;
    unsigned int h = 2166136261u;      // FNV-1a
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
    return h;
}

// The tag is mixed in so that the string "1" and the int 1 cannot collide into the
// same entry even when their payload hashes agree.
static unsigned int bucket_of(HashSetElemType t, unsigned int payload_hash) {
    return ((payload_hash * 31u) + (unsigned int)t) % HASHSET_SIZE;
}

static unsigned int entry_bucket(const Entry* e) {
    switch (e->type) {
        case HASHSET_STRING: return bucket_of(e->type, hash_str(e->v.as_string));
        case HASHSET_INT:    return bucket_of(e->type, hash_bytes(&e->v.as_int, sizeof(long long)));
        case HASHSET_FLOAT:  return bucket_of(e->type, hash_bytes(&e->v.as_float, sizeof(double)));
        case HASHSET_BOOL:   default: {
            int b = e->v.as_bool ? 1 : 0;
            return bucket_of(HASHSET_BOOL, hash_bytes(&b, sizeof(int)));
        }
    }
}

static int entry_equals(const Entry* e, const Entry* probe) {
    if (e->type != probe->type) return 0;
    switch (e->type) {
        case HASHSET_STRING: return strcmp(e->v.as_string, probe->v.as_string) == 0;
        case HASHSET_INT:    return e->v.as_int == probe->v.as_int;
        // Bit-pattern equality, deliberately: it is the only comparison that agrees
        // with the hash. (Consequence to know: NaN never equals itself under `==`
        // but DOES match itself here, which is the behaviour a set wants.)
        case HASHSET_FLOAT:  return memcmp(&e->v.as_float, &probe->v.as_float, sizeof(double)) == 0;
        case HASHSET_BOOL:   default: return (e->v.as_bool ? 1 : 0) == (probe->v.as_bool ? 1 : 0);
    }
}

static void entry_free_payload(Entry* e) {
    if (e->type == HASHSET_STRING) free(e->v.as_string);
}

WynHashSet* hashset_new(void) {
    WynHashSet* set = calloc(1, sizeof(WynHashSet));
    return set;
}

// ONE add, ONE contains, ONE remove over the tagged entry. The nine public
// add/contains/remove entry points below are thin wrappers that build the probe -
// so an element kind cannot be handled correctly by `add` and wrongly by `remove`.
static void hashset_add_entry(WynHashSet* set, const Entry* probe) {
    unsigned int idx = entry_bucket(probe);
    Entry* entry = set->buckets[idx];
    while (entry) {
        if (entry_equals(entry, probe)) return;  // already in set
        entry = entry->next;
    }
    Entry* ne = malloc(sizeof(Entry));
    if (!ne) return;
    *ne = *probe;
    if (probe->type == HASHSET_STRING) ne->v.as_string = strdup(probe->v.as_string);
    ne->next = set->buckets[idx];
    set->buckets[idx] = ne;
}

static int hashset_contains_entry(WynHashSet* set, const Entry* probe) {
    if (!set) return 0;
    unsigned int idx = entry_bucket(probe);
    Entry* entry = set->buckets[idx];
    while (entry) {
        if (entry_equals(entry, probe)) return 1;
        entry = entry->next;
    }
    return 0;
}

static void hashset_remove_entry(WynHashSet* set, const Entry* probe) {
    unsigned int idx = entry_bucket(probe);
    Entry* entry = set->buckets[idx];
    Entry* prev = NULL;
    while (entry) {
        if (entry_equals(entry, probe)) {
            if (prev) prev->next = entry->next;
            else set->buckets[idx] = entry->next;
            entry_free_payload(entry);
            free(entry);
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

// --- string elements -------------------------------------------------------
static Entry probe_str(const char* key) {
    Entry e; memset(&e, 0, sizeof(e));
    e.type = HASHSET_STRING; e.v.as_string = (char*)key; e.next = NULL;
    return e;
}
void hashset_add(WynHashSet* set, const char* key) {
    if (!set || !key) return;
    WYN_COLL_WRITE_ENTER(&set->writing, "HashSet");
    Entry p = probe_str(key);
    hashset_add_entry(set, &p);
    WYN_COLL_WRITE_EXIT(&set->writing);
}
int hashset_contains(WynHashSet* set, const char* key) {
    if (!set || !key) return 0;
    Entry p = probe_str(key);
    return hashset_contains_entry(set, &p);
}
void hashset_remove(WynHashSet* set, const char* key) {
    if (!set || !key) return;
    WYN_COLL_WRITE_ENTER(&set->writing, "HashSet");
    Entry p = probe_str(key);
    hashset_remove_entry(set, &p);
    WYN_COLL_WRITE_EXIT(&set->writing);
}

// --- int elements ----------------------------------------------------------
static Entry probe_int(long long v) {
    Entry e; memset(&e, 0, sizeof(e));
    e.type = HASHSET_INT; e.v.as_int = v; e.next = NULL;
    return e;
}
void hashset_add_int(WynHashSet* set, long long v) {
    if (!set) return;
    WYN_COLL_WRITE_ENTER(&set->writing, "HashSet");
    Entry p = probe_int(v);
    hashset_add_entry(set, &p);
    WYN_COLL_WRITE_EXIT(&set->writing);
}
int hashset_contains_int(WynHashSet* set, long long v) {
    Entry p = probe_int(v);
    return hashset_contains_entry(set, &p);
}
void hashset_remove_int(WynHashSet* set, long long v) {
    if (!set) return;
    WYN_COLL_WRITE_ENTER(&set->writing, "HashSet");
    Entry p = probe_int(v);
    hashset_remove_entry(set, &p);
    WYN_COLL_WRITE_EXIT(&set->writing);
}
void wyn_hashset_add_int(WynHashSet* set, long long v) { hashset_add_int(set, v); }
int  wyn_hashset_contains_int(WynHashSet* set, long long v) { return hashset_contains_int(set, v); }

// --- float elements --------------------------------------------------------
static Entry probe_float(double v) {
    Entry e; memset(&e, 0, sizeof(e));
    e.type = HASHSET_FLOAT; e.v.as_float = v; e.next = NULL;
    return e;
}
void hashset_add_float(WynHashSet* set, double v) {
    if (!set) return;
    WYN_COLL_WRITE_ENTER(&set->writing, "HashSet");
    Entry p = probe_float(v);
    hashset_add_entry(set, &p);
    WYN_COLL_WRITE_EXIT(&set->writing);
}
int hashset_contains_float(WynHashSet* set, double v) {
    Entry p = probe_float(v);
    return hashset_contains_entry(set, &p);
}
void hashset_remove_float(WynHashSet* set, double v) {
    if (!set) return;
    WYN_COLL_WRITE_ENTER(&set->writing, "HashSet");
    Entry p = probe_float(v);
    hashset_remove_entry(set, &p);
    WYN_COLL_WRITE_EXIT(&set->writing);
}

// --- bool elements ---------------------------------------------------------
static Entry probe_bool(int v) {
    Entry e; memset(&e, 0, sizeof(e));
    e.type = HASHSET_BOOL; e.v.as_bool = v ? 1 : 0; e.next = NULL;
    return e;
}
void hashset_add_bool(WynHashSet* set, int v) {
    if (!set) return;
    WYN_COLL_WRITE_ENTER(&set->writing, "HashSet");
    Entry p = probe_bool(v);
    hashset_add_entry(set, &p);
    WYN_COLL_WRITE_EXIT(&set->writing);
}
int hashset_contains_bool(WynHashSet* set, int v) {
    Entry p = probe_bool(v);
    return hashset_contains_entry(set, &p);
}
void hashset_remove_bool(WynHashSet* set, int v) {
    if (!set) return;
    WYN_COLL_WRITE_ENTER(&set->writing, "HashSet");
    Entry p = probe_bool(v);
    hashset_remove_entry(set, &p);
    WYN_COLL_WRITE_EXIT(&set->writing);
}

void hashset_free(WynHashSet* set) {
    if (!set) return;
    for (int i = 0; i < HASHSET_SIZE; i++) {
        Entry* entry = set->buckets[i];
        while (entry) {
            Entry* next = entry->next;
            entry_free_payload(entry);
            free(entry);
            entry = next;
        }
    }
    free(set);
}

// #427: render a set. `print(s)` used to print the set POINTER as a decimal
// (e.g. 4383976288) - check-clean, exit 0, silently meaningless - because the
// print path had no set arm and fell through to the integer one. hashmap_format
// already existed for the map half and its comment says "Same for HashMap and
// HashSet"; this is that other half.
//
// Written here, beside the map's, rather than in wyn_runtime.h, for the same
// reason: only this file can see `struct WynHashSet` and the real element tags.
// Going through hashset_elements() would not do - that bridge pushes every
// element into a WynArray, so an int element comes back as the string "1" and the
// tags that #391 added to keep `{:1}` and `{:"1"}` apart would be thrown away at
// exactly the moment they matter.
//
// SPELLING: `{:1, 2}` with the leading colon, which is the set LITERAL syntax, so
// what prints can be pasted back into a program. An EMPTY set is `{:}` for the
// same reason - `{}` is the empty MAP literal, and printing a set as `{}` would
// render two different values identically. A string element is quoted, matching
// how the map renders a string value.
//
// ORDER IS BUCKET ORDER, not insertion order - the same caveat hashmap_format and
// hashset_elements already carry. Tests must not assert a multi-element ordering.
//
// Returns the number of bytes that WOULD be written (snprintf semantics), so a
// caller sizes with one call and fills with a second.
int hashset_format(WynHashSet* set, char* out, size_t cap) {
    size_t pos = 0;
    #define HS_EMIT(...) do { \
        int _n = snprintf(out && pos < cap ? out + pos : NULL, \
                          out && pos < cap ? cap - pos : 0, __VA_ARGS__); \
        if (_n > 0) pos += (size_t)_n; \
    } while (0)
    HS_EMIT("{:");
    int first = 1;
    if (set) {
        for (int i = 0; i < HASHSET_SIZE; i++) {
            for (Entry* e = set->buckets[i]; e; e = e->next) {
                if (!first) HS_EMIT(", ");
                first = 0;
                switch (e->type) {
                    case HASHSET_INT:    HS_EMIT("%lld", e->v.as_int); break;
                    case HASHSET_BOOL:   HS_EMIT("%s", e->v.as_bool ? "true" : "false"); break;
                    case HASHSET_FLOAT:  HS_EMIT("%g", e->v.as_float); break;
                    case HASHSET_STRING: HS_EMIT("\"%s\"", e->v.as_string ? e->v.as_string : ""); break;
                    default:             HS_EMIT("<?>"); break;
                }
            }
        }
    }
    HS_EMIT("}");
    #undef HS_EMIT
    if (out && cap > 0) out[pos < cap ? pos : cap - 1] = 0;
    return (int)pos;
}

// --- iteration -------------------------------------------------------------
int hashset_count(WynHashSet* set) {
    return wyn_hashset_len(set);
}

int hashset_elem_at(WynHashSet* set, int index, HashSetElem* out) {
    if (!set || !out || index < 0) return 0;
    int seen = 0;
    for (int i = 0; i < HASHSET_SIZE; i++) {
        for (Entry* e = set->buckets[i]; e; e = e->next) {
            if (seen++ != index) continue;
            memset(out, 0, sizeof(*out));
            out->type = e->type;
            switch (e->type) {
                case HASHSET_STRING: out->as_string = e->v.as_string; break;
                case HASHSET_INT:    out->as_int = e->v.as_int; break;
                case HASHSET_FLOAT:  out->as_float = e->v.as_float; break;
                case HASHSET_BOOL:   out->as_bool = e->v.as_bool; break;
            }
            return 1;
        }
    }
    return 0;
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
    if (!set) return 0;
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
    if (!set) return;
    for (int i = 0; i < HASHSET_SIZE; i++) {
        Entry* entry = set->buckets[i];
        while (entry) {
            Entry* next = entry->next;
            entry_free_payload(entry);
            free(entry);
            entry = next;
        }
        set->buckets[i] = NULL;
    }
}

// The set algebra copies TAGGED entries. It used to read `entry->key` directly, so
// an int-element set would have been unioned as if every element were a char* -
// which is why these five had to move at the same time as the element type, not
// after it.
WynHashSet* wyn_hashset_union(WynHashSet* set1, WynHashSet* set2) {
    WynHashSet* result = hashset_new();
    if (!result) return NULL;
    WynHashSet* srcs[2] = { set1, set2 };
    for (int s = 0; s < 2; s++) {
        if (!srcs[s]) continue;
        for (int i = 0; i < HASHSET_SIZE; i++)
            for (Entry* e = srcs[s]->buckets[i]; e; e = e->next)
                hashset_add_entry(result, e);
    }
    return result;
}

WynHashSet* wyn_hashset_intersection(WynHashSet* set1, WynHashSet* set2) {
    WynHashSet* result = hashset_new();
    if (!result || !set1) return result;
    for (int i = 0; i < HASHSET_SIZE; i++)
        for (Entry* e = set1->buckets[i]; e; e = e->next)
            if (set2 && hashset_contains_entry(set2, e)) hashset_add_entry(result, e);
    return result;
}

WynHashSet* wyn_hashset_difference(WynHashSet* set1, WynHashSet* set2) {
    WynHashSet* result = hashset_new();
    if (!result || !set1) return result;
    for (int i = 0; i < HASHSET_SIZE; i++)
        for (Entry* e = set1->buckets[i]; e; e = e->next)
            if (!set2 || !hashset_contains_entry(set2, e)) hashset_add_entry(result, e);
    return result;
}

int wyn_hashset_is_subset(WynHashSet* set1, WynHashSet* set2) {
    if (!set1) return 1;
    for (int i = 0; i < HASHSET_SIZE; i++)
        for (Entry* e = set1->buckets[i]; e; e = e->next)
            if (!set2 || !hashset_contains_entry(set2, e)) return 0;
    return 1;
}

int wyn_hashset_is_disjoint(WynHashSet* set1, WynHashSet* set2) {
    if (!set1 || !set2) return 1;
    for (int i = 0; i < HASHSET_SIZE; i++)
        for (Entry* e = set1->buckets[i]; e; e = e->next)
            if (hashset_contains_entry(set2, e)) return 0;
    return 1;
}
