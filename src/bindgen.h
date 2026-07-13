// C-header binding generator: `wyn bind <header.h>`.
//
// Preprocesses a C header with the system C compiler (`cc -E`, portable across
// gcc/clang on every platform Wyn supports — NOT the bundled arm64-only tcc),
// then scans the declaration-level output and emits Wyn `extern fn` declarations,
// struct aliases, and integer/float/string `#define` constants for the subset the
// FFI type map can faithfully represent (functions over int/float/bool/string/
// void/ptr; scalar/pointer-field structs; literal object-like macros). Anything
// outside that subset (callbacks, unions, bitfields, by-value struct params,
// function-pointer typedefs) is skipped with a `// TODO:` note so the output is
// honest and hand-completable.
#ifndef WYN_BINDGEN_H
#define WYN_BINDGEN_H

// Generate Wyn bindings for `header_path`.
//   cc            : C compiler command for preprocessing (e.g. "cc"/"clang"/"gcc").
//   extra_iflags  : extra "-I..." flags (space-separated) for header resolution,
//                   or NULL. Typically from wyn.toml [ffi].include_dirs.
//   out           : FILE* to write the generated Wyn source to (e.g. stdout or a
//                   .wyn file). Must be open for writing.
// Returns 0 on success, nonzero on failure (preprocess failed / header missing).
int wyn_bindgen(const char* header_path, const char* cc, const char* extra_iflags, FILE* out);

#endif // WYN_BINDGEN_H
