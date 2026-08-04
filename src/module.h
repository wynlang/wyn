#ifndef WYN_MODULE_H
#define WYN_MODULE_H

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
void set_source_directory(const char* source_file);
void check_all_modules(void);

#endif
