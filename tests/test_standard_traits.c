// T3.2.3: Standard Traits Test Program
// Test standard trait implementations and functionality

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "ast.h"

// Simple make_type implementation for testing
Type* make_type(TypeKind kind) {
    Type* t = calloc(1, sizeof(Type));
    t->kind = kind;
    return t;
}

// Test standard traits functionality
int main() {
    printf("=== T3.2.3: Standard Traits Test ===\n");
    
    // Initialize systems
    wyn_generics_init();
    wyn_traits_init();
    
    // Test standard trait registration
    printf("1. Testing standard trait registration...\n");
    
    Token eq_trait = {TOKEN_IDENT, "Eq", 2, 0};
    Token ord_trait = {TOKEN_IDENT, "Ord", 3, 0};
    Token clone_trait = {TOKEN_IDENT, "Clone", 5, 0};
    Token copy_trait = {TOKEN_IDENT, "Copy", 4, 0};
    Token display_trait = {TOKEN_IDENT, "Display", 7, 0};
    Token debug_trait = {TOKEN_IDENT, "Debug", 5, 0};
    Token iterator_trait = {TOKEN_IDENT, "Iterator", 8, 0};
    Token custom_trait = {TOKEN_IDENT, "Custom", 6, 0};
    
    printf("   Eq is standard trait: %s\n", wyn_is_standard_trait(eq_trait) ? "Yes" : "No");
    printf("   Ord is standard trait: %s\n", wyn_is_standard_trait(ord_trait) ? "Yes" : "No");
    printf("   Clone is standard trait: %s\n", wyn_is_standard_trait(clone_trait) ? "Yes" : "No");
    printf("   Copy is standard trait: %s\n", wyn_is_standard_trait(copy_trait) ? "Yes" : "No");
    printf("   Display is standard trait: %s\n", wyn_is_standard_trait(display_trait) ? "Yes" : "No");
    printf("   Debug is standard trait: %s\n", wyn_is_standard_trait(debug_trait) ? "Yes" : "No");
    printf("   Iterator is standard trait: %s\n", wyn_is_standard_trait(iterator_trait) ? "Yes" : "No");
    printf("   Custom is standard trait: %s\n", wyn_is_standard_trait(custom_trait) ? "Yes" : "No");
    
    // Test standard trait methods
    printf("2. Testing standard trait methods...\n");
    printf("   Eq method: %s\n", wyn_get_standard_trait_method(eq_trait));
    printf("   Ord method: %s\n", wyn_get_standard_trait_method(ord_trait));
    printf("   Clone method: %s\n", wyn_get_standard_trait_method(clone_trait));
    printf("   Display method: %s\n", wyn_get_standard_trait_method(display_trait));
    printf("   Debug method: %s\n", wyn_get_standard_trait_method(debug_trait));
    printf("   Iterator method: %s\n", wyn_get_standard_trait_method(iterator_trait));
    
    // Test basic type trait implementations
    printf("3. Testing basic type trait implementations...\n");
    
    Token int_type = {TOKEN_IDENT, "int", 3, 0};
    Token float_type = {TOKEN_IDENT, "float", 5, 0};
    Token string_type = {TOKEN_IDENT, "string", 6, 0};
    Token bool_type = {TOKEN_IDENT, "bool", 4, 0};
    
    printf("   int implements Eq: %s\n", wyn_type_implements_trait(int_type, eq_trait) ? "Yes" : "No");
    printf("   int implements Ord: %s\n", wyn_type_implements_trait(int_type, ord_trait) ? "Yes" : "No");
    printf("   int implements Clone: %s\n", wyn_type_implements_trait(int_type, clone_trait) ? "Yes" : "No");
    printf("   int implements Copy: %s\n", wyn_type_implements_trait(int_type, copy_trait) ? "Yes" : "No");
    printf("   int implements Display: %s\n", wyn_type_implements_trait(int_type, display_trait) ? "Yes" : "No");
    
    printf("   string implements Eq: %s\n", wyn_type_implements_trait(string_type, eq_trait) ? "Yes" : "No");
    printf("   string implements Clone: %s\n", wyn_type_implements_trait(string_type, clone_trait) ? "Yes" : "No");
    printf("   string implements Copy: %s\n", wyn_type_implements_trait(string_type, copy_trait) ? "Yes" : "No");
    printf("   string implements Display: %s\n", wyn_type_implements_trait(string_type, display_trait) ? "Yes" : "No");
    
    // Test trait derivation capabilities
    printf("4. Testing trait derivation capabilities...\n");
    printf("   int can derive Eq: %s\n", wyn_can_derive_trait(int_type, eq_trait) ? "Yes" : "No");
    printf("   int can derive Clone: %s\n", wyn_can_derive_trait(int_type, clone_trait) ? "Yes" : "No");
    printf("   string can derive Copy: %s\n", wyn_can_derive_trait(string_type, copy_trait) ? "Yes" : "No");
    printf("   string can derive Display: %s\n", wyn_can_derive_trait(string_type, display_trait) ? "Yes" : "No");
    
    // Test with constraint validation
    printf("5. Testing standard traits with constraints...\n");
    
    // Create constraints using standard traits
    TypeConstraint* constraints = wyn_create_constraint(eq_trait);
    constraints = wyn_add_constraint(constraints, clone_trait);
    
    Type* int_type_obj = make_type(TYPE_INT);
    int_type_obj->name = int_type;
    
    Type* string_type_obj = make_type(TYPE_STRING);
    string_type_obj->name = string_type;
    
    bool int_satisfies = wyn_check_constraints(int_type_obj, constraints);
    bool string_satisfies = wyn_check_constraints(string_type_obj, constraints);
    
    printf("   int satisfies Eq + Clone: %s\n", int_satisfies ? "Yes" : "No");
    printf("   string satisfies Eq + Clone: %s\n", string_satisfies ? "Yes" : "No");
    
    // Print system statistics
    printf("6. System statistics:\n");
    wyn_print_trait_stats();
    
    // Cleanup
    wyn_free_constraints(constraints);
    free(int_type_obj);
    free(string_type_obj);
    wyn_cleanup_traits();
    wyn_cleanup_generics();
    
    printf("=== T3.2.3: Standard Traits Test Complete ===\n");
    return 0;
}
