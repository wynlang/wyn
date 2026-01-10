#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "../src/modules.h"

void test_module_creation() {
    printf("Testing module creation...\n");
    
    wyn_init_modules();
    
    WynModule* module = wyn_create_module("math.geometry", "math/geometry.wyn");
    assert(module != NULL);
    assert(strcmp(module->name, "math.geometry") == 0);
    assert(strcmp(module->path, "math/geometry.wyn") == 0);
    assert(module->export_count == 0);
    assert(module->import_count == 0);
    assert(!module->is_loaded);
    
    wyn_register_module(module);
    
    WynModule* found = wyn_find_module("math.geometry");
    assert(found == module);
    
    printf("✓ Module creation test passed\n");
}

void test_module_exports() {
    printf("Testing module exports...\n");
    
    WynModule* module = wyn_create_module("test_module", "test.wyn");
    
    // Add some dummy exports
    int dummy_function = 42;
    int dummy_struct = 100;
    
    wyn_add_export(module, "distance", WYN_EXPORT_FUNCTION, &dummy_function);
    wyn_add_export(module, "Point", WYN_EXPORT_STRUCT, &dummy_struct);
    
    assert(module->export_count == 2);
    
    WynExport* export = wyn_find_export(module, "distance");
    assert(export != NULL);
    assert(strcmp(export->name, "distance") == 0);
    assert(export->type == WYN_EXPORT_FUNCTION);
    assert(export->is_public);
    
    export = wyn_find_export(module, "Point");
    assert(export != NULL);
    assert(strcmp(export->name, "Point") == 0);
    assert(export->type == WYN_EXPORT_STRUCT);
    
    export = wyn_find_export(module, "nonexistent");
    assert(export == NULL);
    
    printf("✓ Module exports test passed\n");
}

void test_module_imports() {
    printf("Testing module imports...\n");
    
    wyn_init_modules();  // Clean state for this test
    
    WynModule* geometry_module = wyn_create_module("math.geometry", "math/geometry.wyn");
    WynModule* main_module = wyn_create_module("main", "main.wyn");
    
    // Add exports to geometry module
    int point_struct = 100;
    int distance_func = 42;
    wyn_add_export(geometry_module, "Point", WYN_EXPORT_STRUCT, &point_struct);
    wyn_add_export(geometry_module, "distance", WYN_EXPORT_FUNCTION, &distance_func);
    
    wyn_register_module(geometry_module);
    wyn_register_module(main_module);
    
    // Add selective imports to main module
    wyn_add_import(main_module, "math.geometry", "Point", NULL);
    wyn_add_import(main_module, "math.geometry", "distance", NULL);
    
    assert(main_module->import_count == 2);
    
    // Test import resolution
    void* resolved = wyn_resolve_import(main_module, "Point");
    assert(resolved == &point_struct);
    
    resolved = wyn_resolve_import(main_module, "distance");
    assert(resolved == &distance_func);
    
    resolved = wyn_resolve_import(main_module, "nonexistent");
    assert(resolved == NULL);
    
    printf("✓ Module imports test passed\n");
}

void test_wildcard_imports() {
    printf("Testing wildcard imports...\n");
    
    wyn_init_modules();  // Clean state for this test
    
    WynModule* utils_module = wyn_create_module("utils", "utils.wyn");
    WynModule* main_module = wyn_create_module("main2", "main2.wyn");
    
    // Add exports to utils module
    int helper1 = 1;
    int helper2 = 2;
    wyn_add_export(utils_module, "helper1", WYN_EXPORT_FUNCTION, &helper1);
    wyn_add_export(utils_module, "helper2", WYN_EXPORT_FUNCTION, &helper2);
    
    wyn_register_module(utils_module);
    wyn_register_module(main_module);
    
    // Add wildcard import
    wyn_add_import(main_module, "utils", NULL, NULL);
    
    assert(main_module->import_count == 1);
    assert(main_module->imports[0].is_wildcard);
    
    // Test wildcard resolution
    void* resolved = wyn_resolve_import(main_module, "helper1");
    assert(resolved == &helper1);
    
    resolved = wyn_resolve_import(main_module, "helper2");
    assert(resolved == &helper2);
    
    printf("✓ Wildcard imports test passed\n");
}

void test_aliased_imports() {
    printf("Testing aliased imports...\n");
    
    wyn_init_modules();  // Clean state for this test
    
    WynModule* math_module = wyn_create_module("math", "math.wyn");
    WynModule* main_module = wyn_create_module("main3", "main3.wyn");
    
    // Add export to math module
    int sqrt_func = 999;
    wyn_add_export(math_module, "sqrt", WYN_EXPORT_FUNCTION, &sqrt_func);
    
    wyn_register_module(math_module);
    wyn_register_module(main_module);
    
    // Add aliased import
    wyn_add_import(main_module, "math", "sqrt", "square_root");
    
    assert(main_module->import_count == 1);
    assert(!main_module->imports[0].is_wildcard);
    assert(strcmp(main_module->imports[0].alias, "square_root") == 0);
    
    // Test alias resolution
    void* resolved = wyn_resolve_import(main_module, "square_root");
    assert(resolved == &sqrt_func);
    
    // Original name should not resolve
    resolved = wyn_resolve_import(main_module, "sqrt");
    assert(resolved == NULL);
    
    printf("✓ Aliased imports test passed\n");
}

void test_module_path_resolution() {
    printf("Testing module path resolution...\n");
    
    char* path = wyn_resolve_module_path("math.geometry");
    assert(strcmp(path, "math/geometry.wyn") == 0);
    free(path);
    
    path = wyn_resolve_module_path("simple");
    assert(strcmp(path, "simple.wyn") == 0);
    free(path);
    
    path = wyn_resolve_module_path("deep.nested.module");
    assert(strcmp(path, "deep/nested/module.wyn") == 0);
    free(path);
    
    printf("✓ Module path resolution test passed\n");
}

int main() {
    printf("Running Module System Tests...\n\n");
    
    test_module_creation();
    test_module_exports();
    test_module_imports();
    test_wildcard_imports();
    test_aliased_imports();
    test_module_path_resolution();
    
    wyn_cleanup_modules();
    
    printf("\n✅ All module system tests passed!\n");
    return 0;
}
