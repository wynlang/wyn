#ifndef WYN_MODULES_H
#define WYN_MODULES_H

#include <stdbool.h>
#include <stddef.h>

// Export types
typedef enum {
    WYN_EXPORT_FUNCTION,
    WYN_EXPORT_STRUCT,
    WYN_EXPORT_VARIABLE,
    WYN_EXPORT_TYPE,
    WYN_EXPORT_TRAIT
} WynExportType;

// Module export
typedef struct {
    const char* name;
    WynExportType type;
    void* item;
    bool is_public;
} WynExport;

// Module import
typedef struct {
    const char* module_name;
    const char* item_name;  // NULL for wildcard imports
    const char* alias;      // NULL if no alias
    bool is_wildcard;
} WynImport;

// Module structure
typedef struct {
    const char* name;
    const char* path;
    WynExport* exports;
    size_t export_count;
    size_t export_capacity;
    WynImport* imports;
    size_t import_count;
    size_t import_capacity;
    bool is_loaded;
} WynModule;

// Module system functions
void wyn_init_modules(void);
WynModule* wyn_create_module(const char* name, const char* path);
void wyn_register_module(WynModule* module);
WynModule* wyn_find_module(const char* name);
void wyn_add_export(WynModule* module, const char* name, WynExportType type, void* item);
void wyn_add_import(WynModule* module, const char* module_name, const char* item_name, const char* alias);
char* wyn_resolve_module_path(const char* module_name);
WynExport* wyn_find_export(WynModule* module, const char* name);
void* wyn_resolve_import(WynModule* module, const char* name);
WynModule* resolve_module(const char* module_name);  // Load and parse module file
void wyn_cleanup_modules(void);

#endif // WYN_MODULES_H
