// RETIRED - see src/json.h.
//
// This file held `Json_parse`/`Json_get_string`/`Json_get_int`/`Json_free` wrappers
// over the WynJson* pairs model, duplicating the same four names that
// src/wyn_runtime.h defines over the node arena. Both were compiled into the TCC
// runtime archive, so which definition a program got depended on archive member
// order. One model, one definition site: src/wyn_runtime.h.
#include "json.h"

// ISO C requires a translation unit to contain at least one declaration.
typedef int wyn_json_runtime_retired_tu;
