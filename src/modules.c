#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#include "modules.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Module registry
static WynModule** modules = NULL;
static size_t module_count = 0;
static size_t module_capacity = 0;

// Initialize module system
void wyn_init_modules(void) {
    modules = NULL;
    module_count = 0;
    module_capacity = 0;
}

// Create a new module
WynModule* wyn_create_module(const char* name, const char* path) {
    WynModule* module = malloc(sizeof(WynModule));
    module->name = strdup(name);
    module->path = strdup(path);
    module->exports = NULL;
    module->export_count = 0;
    module->export_capacity = 0;
    module->imports = NULL;
    module->import_count = 0;
    module->import_capacity = 0;
    module->is_loaded = false;
    return module;
}

// Register a module
void wyn_register_module(WynModule* module) {
    if (module_count >= module_capacity) {
        module_capacity = module_capacity == 0 ? 8 : module_capacity * 2;
        modules = realloc(modules, sizeof(WynModule*) * module_capacity);
    }
    modules[module_count++] = module;
}

// Find module by name
WynModule* wyn_find_module(const char* name) {
    for (size_t i = 0; i < module_count; i++) {
        if (strcmp(modules[i]->name, name) == 0) {
            return modules[i];
        }
    }
    return NULL;
}

// Add export to module
void wyn_add_export(WynModule* module, const char* name, WynExportType type, void* item) {
    if (module->export_count >= module->export_capacity) {
        module->export_capacity = module->export_capacity == 0 ? 8 : module->export_capacity * 2;
        module->exports = realloc(module->exports, sizeof(WynExport) * module->export_capacity);
    }
    
    WynExport* export = &module->exports[module->export_count++];
    export->name = strdup(name);
    export->type = type;
    export->item = item;
    export->is_public = true;
}

// Add import to module
void wyn_add_import(WynModule* module, const char* module_name, const char* item_name, const char* alias) {
    if (module->import_count >= module->import_capacity) {
        module->import_capacity = module->import_capacity == 0 ? 8 : module->import_capacity * 2;
        module->imports = realloc(module->imports, sizeof(WynImport) * module->import_capacity);
    }
    
    WynImport* import = &module->imports[module->import_count++];
    import->module_name = strdup(module_name);
    import->item_name = item_name ? strdup(item_name) : NULL;
    import->alias = alias ? strdup(alias) : NULL;
    import->is_wildcard = (item_name == NULL);
}

// Resolve import path
char* wyn_resolve_module_path(const char* module_name) {
    // Convert module.submodule to module/submodule.wyn
    size_t len = strlen(module_name);
    char* path = malloc(len + 5); // +4 for ".wyn" +1 for null
    
    strcpy(path, module_name);
    
    // Replace dots with slashes
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '.') {
            path[i] = '/';
        }
    }
    
    strcat(path, ".wyn");
    return path;
}

// Find export in module
WynExport* wyn_find_export(WynModule* module, const char* name) {
    for (size_t i = 0; i < module->export_count; i++) {
        if (strcmp(module->exports[i].name, name) == 0 && module->exports[i].is_public) {
            return &module->exports[i];
        }
    }
    return NULL;
}

// Resolve imported symbol
void* wyn_resolve_import(WynModule* module, const char* name) {
    for (size_t i = 0; i < module->import_count; i++) {
        WynImport* import = &module->imports[i];
        
        if (import->is_wildcard) {
            // Wildcard import - check in the imported module
            WynModule* imported_module = wyn_find_module(import->module_name);
            if (imported_module) {
                WynExport* export = wyn_find_export(imported_module, name);
                if (export) {
                    return export->item;
                }
            }
        } else if (import->alias && strcmp(import->alias, name) == 0) {
            // Aliased import
            WynModule* imported_module = wyn_find_module(import->module_name);
            if (imported_module) {
                WynExport* export = wyn_find_export(imported_module, import->item_name);
                if (export) {
                    return export->item;
                }
            }
        } else if (import->item_name && strcmp(import->item_name, name) == 0 && !import->alias) {
            // Direct import (no alias)
            WynModule* imported_module = wyn_find_module(import->module_name);
            if (imported_module) {
                WynExport* export = wyn_find_export(imported_module, name);
                if (export) {
                    return export->item;
                }
            }
        }
    }
    return NULL;
}

// Load and parse module file
WynModule* resolve_module(const char* module_name) {
    // Check if module is already loaded
    WynModule* existing = wyn_find_module(module_name);
    if (existing && existing->is_loaded) {
        return existing;
    }
    
    // Resolve module path
    char* path = wyn_resolve_module_path(module_name);
    
    // Try to open the file
    FILE* file = fopen(path, "r");
    if (!file) {
        free(path);
        return NULL;
    }
    
    // Read file content
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* content = malloc(size + 1);
    fread(content, 1, size, file);
    content[size] = '\0';
    fclose(file);
    
    // Create module if it doesn't exist
    if (!existing) {
        existing = wyn_create_module(module_name, path);
        wyn_register_module(existing);
    }
    
    // Parse the module content (simplified - just look for function definitions)
    char* pos = content;
    while ((pos = strstr(pos, "fn ")) != NULL) {
        pos += 3; // Skip "fn "
        
        // Extract function name
        char* name_start = pos;
        while (*pos && *pos != '(' && *pos != ' ') pos++;
        
        if (*pos == '(') {
            size_t name_len = pos - name_start;
            char* func_name = malloc(name_len + 1);
            strncpy(func_name, name_start, name_len);
            func_name[name_len] = '\0';
            
            // Add function to module exports (using function name as item)
            wyn_add_export(existing, func_name, WYN_EXPORT_FUNCTION, func_name);
        }
    }
    
    existing->is_loaded = true;
    free(content);
    free(path);
    return existing;
}

// Cleanup module system
void wyn_cleanup_modules(void) {
    for (size_t i = 0; i < module_count; i++) {
        WynModule* module = modules[i];
        
        // Free exports
        for (size_t j = 0; j < module->export_count; j++) {
            free((void*)module->exports[j].name);
        }
        free(module->exports);
        
        // Free imports
        for (size_t j = 0; j < module->import_count; j++) {
            free((void*)module->imports[j].module_name);
            if (module->imports[j].item_name) {
                free((void*)module->imports[j].item_name);
            }
            if (module->imports[j].alias) {
                free((void*)module->imports[j].alias);
            }
        }
        free(module->imports);
        
        free((void*)module->name);
        free((void*)module->path);
        free(module);
    }
    
    free(modules);
    modules = NULL;
    module_count = 0;
    module_capacity = 0;
}
