#ifndef WYN_MODULE_H
#define WYN_MODULE_H

#include <time.h>   // time_t, for get_newest_module_mtime
#include "ast.h"

// Module resolution
Program* load_module(const char* module_name);
void add_module_path(const char* path);
void preload_imports(const char* source);
char* resolve_module_path(const char* module_name);
char* resolve_relative_module_name(const char* module_name);
bool is_builtin_module(const char* module_name);
bool has_circular_import(void);
// True if any `import` could not be resolved. Checked at the same four entry points
// as has_circular_import, and for the same reason: the module's symbols are missing,
// so codegen would emit C with holes in it.
bool has_unresolved_import(void);
// mtime of the newest module file LOADED this run, or 0 if none. The `wyn run`
// incremental cache must compare against this as well as the entry file, or editing
// a module and re-running silently executes the previous binary.
time_t get_newest_module_mtime(void);
// Newest mtime among a source's DIRECT imports, resolved but not loaded. Used by the
// `wyn run` cache, which decides before any module is loaded.
time_t scan_import_mtimes(const char* source);
void set_source_directory(const char* source_file);
void check_all_modules(void);

#endif
