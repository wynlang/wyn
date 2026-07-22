#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "module_registry.h"
#include "growable.h"

ModuleRegistry global_module_registry = {0};

void init_module_registry() {
    global_module_registry.count = 0;
}

void register_module(const char* name, Program* ast) {
    WYN_ENSURE_CAP(global_module_registry.modules, global_module_registry.count,
                   global_module_registry.capacity);

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
    
    // Try converting _ to / and search again (network_http -> network/http)
    if (strchr(name, '_')) {
        char alt_name[256];
        strncpy(alt_name, name, sizeof(alt_name) - 1);
        alt_name[sizeof(alt_name) - 1] = '\0';
        for (char* p = alt_name; *p; p++) {
            if (*p == '_') *p = '/';
        }
        for (int i = 0; i < global_module_registry.count; i++) {
            if (strcmp(global_module_registry.modules[i]->name, alt_name) == 0) {
                return global_module_registry.modules[i]->ast;
            }
        }
    }
    
    return NULL;
}

const char* get_module_name_by_ast(Program* ast) {
    for (int i = 0; i < global_module_registry.count; i++) {
        if (global_module_registry.modules[i]->ast == ast) {
            return global_module_registry.modules[i]->name;
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

int get_module_count(void) {
    return global_module_registry.count;
}

Program* get_module_at(int index) {
    if (index < 0 || index >= global_module_registry.count) {
        return NULL;
    }
    return global_module_registry.modules[index]->ast;
}

void* get_module_entry_at(int index) {
    if (index < 0 || index >= global_module_registry.count) {
        return NULL;
    }
    return global_module_registry.modules[index];
}
