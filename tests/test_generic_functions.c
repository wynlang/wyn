#include "../src/test.h"
#include "../src/generics.h"
#include "../src/memory.h"
#include "../src/types.h"
#include <string.h>
#include <stdio.h>

// Test macro
#define RUN_TEST(name) do { \
    printf("Running test: %s... ", #name); \
    if (name()) { \
        printf("✅ PASSED\n"); \
    } else { \
        printf("❌ FAILED\n"); \
        all_passed = false; \
    } \
} while(0)

// Forward declaration
Type* make_type(TypeKind kind);

// T3.1.1: Generic Functions Implementation Tests
// Tests registration and basic functionality of generic functions

// Helper function to create a type
Type* make_int_type() {
    Type* type = make_type(TYPE_INT);
    return type;
}

static bool test_generic_function_registration() {
    // Test registering generic functions in the system
    wyn_generics_init();
    
    // Create a mock generic function
    FnStmt mock_fn = {0};
    mock_fn.name.start = "identity";
    mock_fn.name.length = 8;
    mock_fn.type_param_count = 1;
    
    Token type_param = {0};
    type_param.start = "T";
    type_param.length = 1;
    mock_fn.type_params = &type_param;
    
    // Register the generic function
    wyn_register_generic_function(&mock_fn);
    
    // Check if function can be found (just check it exists)
    GenericFunction* found = wyn_find_generic_function(mock_fn.name);
    
    return found != NULL;
}

static bool test_generic_function_lookup() {
    // Test finding registered generic functions
    wyn_generics_init();
    
    // Create and register a mock function
    FnStmt mock_fn = {0};
    mock_fn.name.start = "max";
    mock_fn.name.length = 3;
    mock_fn.type_param_count = 1;
    
    Token type_param = {0};
    type_param.start = "T";
    type_param.length = 1;
    mock_fn.type_params = &type_param;
    
    wyn_register_generic_function(&mock_fn);
    
    // Test lookup
    Token search_name = {0};
    search_name.start = "max";
    search_name.length = 3;
    
    GenericFunction* found = wyn_find_generic_function(search_name);
    
    return found != NULL;
}

static bool test_generic_function_instantiation() {
    // Test basic instantiation of generic functions
    wyn_generics_init();
    
    // Create and register a mock function
    FnStmt mock_fn = {0};
    mock_fn.name.start = "swap";
    mock_fn.name.length = 4;
    mock_fn.type_param_count = 1;
    
    Token type_param = {0};
    type_param.start = "T";
    type_param.length = 1;
    mock_fn.type_params = &type_param;
    
    wyn_register_generic_function(&mock_fn);
    
    // Test instantiation with int type
    Type* int_type = make_int_type();
    Type* instantiated = wyn_instantiate_generic_function("swap", &int_type, 1);
    
    return instantiated != NULL && instantiated->kind == TYPE_FUNCTION;
}

static bool test_multiple_generic_functions() {
    // Test registering multiple generic functions
    wyn_generics_init();
    
    // Register first function
    FnStmt fn1 = {0};
    fn1.name.start = "identity";
    fn1.name.length = 8;
    fn1.type_param_count = 1;
    
    Token type_param1 = {0};
    type_param1.start = "T";
    type_param1.length = 1;
    fn1.type_params = &type_param1;
    
    wyn_register_generic_function(&fn1);
    
    // Register second function
    FnStmt fn2 = {0};
    fn2.name.start = "compare";
    fn2.name.length = 7;
    fn2.type_param_count = 1;
    
    Token type_param2 = {0};
    type_param2.start = "U";
    type_param2.length = 1;
    fn2.type_params = &type_param2;
    
    wyn_register_generic_function(&fn2);
    
    // Check both can be found
    Token name1 = {0};
    name1.start = "identity";
    name1.length = 8;
    
    Token name2 = {0};
    name2.start = "compare";
    name2.length = 7;
    
    GenericFunction* found1 = wyn_find_generic_function(name1);
    GenericFunction* found2 = wyn_find_generic_function(name2);
    
    return found1 != NULL && found2 != NULL && found1 != found2;
}

static bool test_generic_function_type_parameters() {
    // Test functions with multiple type parameters
    wyn_generics_init();
    
    // Create function with 2 type parameters
    FnStmt mock_fn = {0};
    mock_fn.name.start = "map";
    mock_fn.name.length = 3;
    mock_fn.type_param_count = 2;
    
    Token type_params[2] = {0};
    type_params[0].start = "T";
    type_params[0].length = 1;
    type_params[1].start = "U";
    type_params[1].length = 1;
    mock_fn.type_params = type_params;
    
    wyn_register_generic_function(&mock_fn);
    
    // Check registration (just check it exists)
    GenericFunction* found = wyn_find_generic_function(mock_fn.name);
    
    return found != NULL;
}

int main() {
    printf("🔥 Testing T3.1.1: Generic Functions Implementation\n");
    printf("==================================================\n\n");
    
    bool all_passed = true;
    
    RUN_TEST(test_generic_function_registration);
    RUN_TEST(test_generic_function_lookup);
    RUN_TEST(test_generic_function_instantiation);
    RUN_TEST(test_multiple_generic_functions);
    RUN_TEST(test_generic_function_type_parameters);
    
    printf("\n==================================================\n");
    if (all_passed) {
        printf("✅ All T3.1.1 generic functions tests PASSED!\n");
        printf("T3.1.1: Generic Functions Implementation - COMPLETED ✅\n");
        return 0;
    } else {
        printf("❌ Some T3.1.1 tests FAILED!\n");
        return 1;
    }
}
