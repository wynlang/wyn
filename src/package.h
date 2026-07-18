// Git-URL dependency management. A dependency is a git repo (see pkgspec.h);
// there is no central registry. Packages are cloned once into a global cache
// (~/.wyn/pkg) and pinned in wyn.lock for reproducible builds.
#ifndef PACKAGE_H
#define PACKAGE_H

#include <stddef.h>

// `wyn add <input> [--as <name>]`: resolve the spec, clone it into the global
// cache (if not already present), record it in wyn.toml [dependencies] and pin
// its commit in wyn.lock. `override_name` may be NULL. Returns 0 on success.
int wyn_pkg_add(const char* input, const char* override_name);

// `wyn install` / `wyn pkg install` / `wyn restore`: ensure every dependency in
// wyn.lock (or wyn.toml [dependencies] if there is no lock) is present in the
// cache at its pinned commit. Returns 0 on success.
int wyn_pkg_install(void);

// `wyn remove <name>`: drop the dependency from wyn.toml [dependencies] and
// wyn.lock. The cache copy is left in place (shared with other projects).
int wyn_pkg_remove(const char* name);

// `wyn list` / `wyn pkg list`: print the project's declared dependencies.
int wyn_pkg_list(void);

// `wyn pkg audit`: supply-chain verification from wyn.lock + git — lock↔remote
// (moved tags = error), lock↔cache integrity, pin quality (branch<tag<sha),
// ffi surface flags. `offline` skips the ls-remote checks. Returns the worst
// finding: 0 clean, 1 warnings, 2 errors.
int wyn_pkg_audit(int offline);

// Resolve an import name to its on-disk package directory in the cache, using
// the current project's wyn.toml [dependencies]. Returns 1 and fills `dir_out`
// if `import_name` is a declared, present dependency; 0 otherwise.
int wyn_dep_resolve(const char* import_name, char* dir_out, size_t n);

// Append the union of every dependency's wyn.toml [ffi] link flags (-I/-L/-l)
// to `out`, so a program importing a C-binding package links its library.
// Safe to call with no dependencies (leaves `out` unchanged).
void wyn_deps_ffi_flags(char* out, size_t out_size);

#endif // PACKAGE_H
