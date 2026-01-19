#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "module_registry.h"

ModuleRegistry global_module_registry = {0};

void init_module_registry() {
    global_module_registry.count = 0;
    for (int i = 0; i < 64; i++) {
        global_module_registry.modules[i] = NULL;
    }
}

void register_module(const char* name, Program* ast) {
    if (global_module_registry.count >= 64) return;
    
    ModuleEntry* entry = malloc(sizeof(ModuleEntry));
    entry->name = strdup(name);
    entry->ast = ast;
    
    global_module_registry.modules[global_module_registry.count++] = entry;
}

Program* get_module(const char* name) {
    for (int i = 0; i < global_module_registry.count; i++) {
        if (strcmp(global_module_registry.modules[i]->name, name) == 0) {
            return global_module_registry.modules[i]->ast;
        }
    }
    return NULL;
}

bool is_module_loaded(const char* name) {
    return get_module(name) != NULL;
}

int get_all_modules(ModuleEntry** out_modules, int max_count) {
    int count = global_module_registry.count < max_count ? 
                global_module_registry.count : max_count;
    for (int i = 0; i < count; i++) {
        out_modules[i] = global_module_registry.modules[i];
    }
    return count;
}

// For C compatibility
int get_all_modules_raw(void** out_modules, int max_count) {
    return get_all_modules((ModuleEntry**)out_modules, max_count);
}
