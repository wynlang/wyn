#include "arc_runtime.h"
#include <stdlib.h>
#include <string.h>

WynArc* wyn_arc_new(size_t size, void* init_data) {
    WynArc* arc = malloc(sizeof(WynArc) + size);
    if (!arc) return NULL;
    arc->ref_count = 1;
    if (init_data) memcpy(arc->data, init_data, size);
    return arc;
}

WynArc* wyn_arc_retain_arc(WynArc* arc) {
    if (arc) arc->ref_count++;
    return arc;
}

void wyn_arc_release_arc(WynArc* arc) {
    if (arc && --arc->ref_count == 0) free(arc);
}

// Stub implementations for WynObject-based ARC (used by string_memory.c, etc.)
WynObject* wyn_arc_alloc(size_t size, uint32_t type_id, void (*destructor)(void*)) {
    WynObject* obj = malloc(sizeof(WynObject) + size);
    if (!obj) return NULL;
    obj->header.ref_count = 1;
    obj->header.type_id = type_id;
    obj->header.destructor = destructor;
    obj->header.size = size;
    obj->header.magic = 0xDEADBEEF;
    return obj;
}

WynObject* wyn_arc_retain(WynObject* obj) {
    if (obj) obj->header.ref_count++;
    return obj;
}

void wyn_arc_release(WynObject* obj) {
    if (obj && --obj->header.ref_count == 0) {
        if (obj->header.destructor) obj->header.destructor(obj);
        free(obj);
    }
}


// Additional stub functions
void* wyn_arc_get_data(WynObject* obj) {
    return obj ? obj->data : NULL;
}

uint32_t wyn_arc_get_ref_count(WynObject* obj) {
    return obj ? obj->header.ref_count : 0;
}

bool wyn_arc_is_valid(WynObject* obj) {
    return obj && obj->header.magic == 0xDEADBEEF;
}

WynObject* wyn_arc_weak_retain(WynObject* obj) {
    return obj;  // Stub: no weak reference support yet
}

void wyn_arc_weak_release(WynObject* obj) {
    // Stub: no weak reference support yet
}
