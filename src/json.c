// RETIRED - see src/json.h for what was here and why it is gone.
//
// Deliberately empty. Wyn's one JSON object model (the json_nodes[] arena and every
// Json_*/json_* entry point) lives in src/wyn_runtime.h, which is inlined into each
// compiled program and also compiled into libwyn_rt.a via src/runtime_exports.c.
// Anything defined here would be a second definition of those symbols in the same
// archive, and the linker would pick whichever member it scanned first.
#include "json.h"

// ISO C requires a translation unit to contain at least one declaration.
typedef int wyn_json_retired_tu;
