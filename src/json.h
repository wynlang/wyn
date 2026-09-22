#ifndef WYN_JSON_H
#define WYN_JSON_H

// RETIRED. Wyn's JSON object model is ONE node arena addressed by a `long long`
// handle, defined in src/wyn_runtime.h under "=== JSON Parsing ===" and declared
// for --release builds in src/wyn_runtime_slim.h.
//
// This header used to declare a SECOND model: `WynJson*`, a flat array of
// key/(string|int) pairs implemented in src/json.c. `Json.new`/`Json.set_*`/
// `Json.stringify` used it while `Json.parse`/`Json.get_*` used the arena, and the
// checker types both TYPE_JSON and cannot tell them apart. Consequences, all
// measured:
//   * `Json.stringify(Json.parse(s))` passed an integer arena index to a function
//     that dereferenced it as a pointer -> SIGSEGV on the most obvious one-liner
//     in the language, after `wyn check` reported no errors;
//   * `Json.new()` + `Json.set_*` then `Json.get_string(...)` returned "" and
//     `Json.keys(...)` returned nothing, because the write and the read went to
//     different stores;
//   * the pairs model could represent neither nesting, nor floats, nor real
//     booleans, nor null -- so `Json.set_bool` serialised `1`.
//
// src/json.c and src/json_runtime.c are now empty. They are still named by eleven
// build-source lists (Makefile CORE_SRCS / RT_SRCS / TCC_RT_SRCS, main.c's
// wyn_runtime_sources[] plus its inline-runtime, linux, windows, iOS and wasm
// lists, cmd_compile.c's gcc fallback, tcc_backend.c). THREE of those are
// positional snprintf command strings in which dropping a source also means
// dropping one matching `wyn_root` argument -- a miscount there is undefined
// behaviour on a fallback path that local builds never take. Removing the files
// and those eleven entries is therefore a separate, purely mechanical change.

#endif
