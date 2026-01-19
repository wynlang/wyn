#ifndef WYN_MODULE_H
#define WYN_MODULE_H

#include "ast.h"

// Module resolution
Program* load_module(const char* module_name);
void add_module_path(const char* path);
void preload_imports(const char* source);
char* resolve_module_path(const char* module_name);
bool is_builtin_module(const char* module_name);
void set_source_directory(const char* source_file);

#endif
