#ifndef WYN_MODULE_REGISTRY_H
#define WYN_MODULE_REGISTRY_H

#include "ast.h"

typedef struct {
    char* name;
    Program* ast;
} ModuleEntry;

typedef struct {
    ModuleEntry* modules[64];
    int count;
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

// Get all loaded modules
int get_all_modules(ModuleEntry** out_modules, int max_count);

#endif
