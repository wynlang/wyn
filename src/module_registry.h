#ifndef WYN_MODULE_REGISTRY_H
#define WYN_MODULE_REGISTRY_H

#include "ast.h"

typedef struct {
    char* name;
    Program* ast;
} ModuleEntry;

typedef struct {
    ModuleEntry** modules;  // growable (realloc-doubling)
    int count;
    int capacity;
} ModuleRegistry;

// Global module registry
extern ModuleRegistry global_module_registry;

// Initialize registry
void init_module_registry();

// Register a module
void register_module(const char* name, Program* ast);

// Get a module
Program* get_module(const char* name);

// Check if module is loaded
bool is_module_loaded(const char* name);

// Get module name by AST pointer
const char* get_module_name_by_ast(Program* ast);

// Get module count and access by index
int get_module_count(void);
Program* get_module_at(int index);

// Get all loaded modules
int get_all_modules(ModuleEntry** out_modules, int max_count);

// Direct access to a module entry (name + ast) by index; NULL if out of range.
// Preferred over get_all_modules_raw: no fixed-size caller buffer needed.
void* get_module_entry_at(int index);

#endif
