// C-package manager: `wyn add <name>`.
//
// Resolves a library from the curated registry (registry/c-packages.toml under
// wyn_root), generates Wyn bindings for its headers via `wyn bind`, writes the
// generated `.wyn` into ./packages/<name>/, and merges the recipe's link flags
// into the project's wyn.toml [ffi] section so a subsequent `wyn build`/`run`
// links the library. Curated-recipes-first (see the registry file); the design
// intentionally reuses the existing [ffi] link wiring and bindgen.
#ifndef WYN_CPKG_H
#define WYN_CPKG_H

// Implement `wyn add <name>`.
//   name     : registry package name (e.g. "z", "curl").
//   wyn_root : install root (for locating registry/c-packages.toml).
//   cc       : C compiler command for bindgen preprocessing.
// Returns 0 on success, nonzero on failure. Prints human-facing progress.
int wyn_cpkg_add(const char* name, const char* wyn_root, const char* cc);

// Implement `wyn add` with no name: list the available curated packages.
int wyn_cpkg_list(const char* wyn_root);

// Return 1 if `name` is a curated C-library recipe (present in
// registry/c-packages.toml under wyn_root), 0 otherwise. Lets `wyn add <arg>`
// dispatch bare names to the C-library path only when a recipe actually exists.
int wyn_cpkg_has_recipe(const char* name, const char* wyn_root);

// Interactive picker for `wyn add` with no name on a TTY: browse/filter the
// registry, preview a recipe, confirm, then pull it (over the same add engine).
// Line-oriented (works over pipes/CI/all platforms). Returns wyn_cpkg_add's code.
int wyn_cpkg_tui(const char* wyn_root, const char* cc);

#endif // WYN_CPKG_H
