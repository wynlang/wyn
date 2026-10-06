// arc_runtime.h - the only surviving declaration from the retired ARC epic.
// WynTypeId is used by wyn_runtime.h (the generated-C prelude) and by codegen.
#ifndef WYN_ARC_RUNTIME_H
#define WYN_ARC_RUNTIME_H
#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef enum {
    WYN_TYPE_INT = 1,
    WYN_TYPE_FLOAT = 2,
    WYN_TYPE_BOOL = 3,
    WYN_TYPE_STRING = 4,
    WYN_TYPE_ARRAY = 5,
    WYN_TYPE_STRUCT = 6
} WynTypeId;
#endif
