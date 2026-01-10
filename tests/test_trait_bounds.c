// T3.2.2: Trait Bounds and Constraints Test Program
// Simple test to verify trait bounds functionality

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "ast.h"

// Forward declaration
Type* make_type(TypeKind kind);

// Simple make_type implementation for testing
Type* make_type(TypeKind kind) {
    Type* t = calloc(1, sizeof(Type));
    t->kind = kind;
    return t;
}

// Test trait bounds functionality
int main() {
    printf("=== T3.2.2: Trait Bounds and Constraints Test ===\n");
    
    // Initialize systems
    wyn_generics_init();
    wyn_traits_init();
    
    // Create test tokens
    Token display_trait = {TOKEN_IDENT, "Display", 7, 0};
    Token debug_trait = {TOKEN_IDENT, "Debug", 5, 0};
    Token point_type = {TOKEN_IDENT, "Point", 5, 0};
    
    // Test constraint creation
    printf("1. Testing constraint creation...\n");
    TypeConstraint* constraints = wyn_create_constraint(display_trait);
    constraints = wyn_add_constraint(constraints, debug_trait);
    
    // Test trait bound checking
    printf("2. Testing trait bound checking...\n");
    bool has_display = wyn_has_trait_bound(constraints, display_trait);
    bool has_debug = wyn_has_trait_bound(constraints, debug_trait);
    
    printf("   Has Display bound: %s\n", has_display ? "Yes" : "No");
    printf("   Has Debug bound: %s\n", has_debug ? "Yes" : "No");
    
    // Test bounds string generation
    printf("3. Testing bounds string generation...\n");
    char bounds_str[256];
    wyn_get_trait_bounds_string(constraints, bounds_str, sizeof(bounds_str));
    printf("   Bounds string: %s\n", bounds_str);
    
    // Test constraint validation (will fail since Point doesn't implement traits yet)
    printf("4. Testing constraint validation...\n");
    Type* point_type_obj = make_type(TYPE_STRUCT);
    point_type_obj->name = point_type;
    
    bool satisfies = wyn_check_constraints(point_type_obj, constraints);
    printf("   Point satisfies constraints: %s\n", satisfies ? "Yes" : "No");
    
    // Register a trait implementation to make validation pass
    printf("5. Testing with trait implementation...\n");
    wyn_register_trait_impl(display_trait, point_type, NULL, 0);
    wyn_register_trait_impl(debug_trait, point_type, NULL, 0);
    
    satisfies = wyn_check_constraints(point_type_obj, constraints);
    printf("   Point satisfies constraints after impl: %s\n", satisfies ? "Yes" : "No");
    
    // Print statistics
    printf("6. System statistics:\n");
    wyn_print_trait_stats();
    wyn_print_generic_stats();
    
    // Cleanup
    wyn_free_constraints(constraints);
    free(point_type_obj);
    wyn_cleanup_traits();
    wyn_cleanup_generics();
    
    printf("=== T3.2.2: Trait Bounds Test Complete ===\n");
    return 0;
}
