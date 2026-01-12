#include "types.h"
#include "ast.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// T3.2.1: Trait Definitions and Implementation
// Basic trait system with trait definitions and implementations

// Forward declarations
Type* check_expr(Expr* expr, SymbolTable* scope);
Symbol* find_symbol(SymbolTable* scope, Token name);
Type* make_type(TypeKind kind);

// Trait registry
typedef struct Trait {
    Token name;
    Token* type_params;
    int type_param_count;
    FnStmt** methods;
    int method_count;
    bool* method_has_default;
    struct Trait* next;
} Trait;

// Trait implementation registry
typedef struct TraitImpl {
    Token trait_name;
    Token type_name;
    FnStmt** methods;
    int method_count;
    struct TraitImpl* next;
} TraitImpl;

static Trait* g_traits = NULL;
static TraitImpl* g_trait_impls = NULL;
static size_t g_trait_count = 0;
static size_t g_trait_impl_count = 0;

// Initialize trait system
void wyn_traits_init(void) {
    g_traits = NULL;
    g_trait_impls = NULL;
    g_trait_count = 0;
    g_trait_impl_count = 0;
    
    // T3.2.3: Register standard traits
    wyn_register_standard_traits();
    
    // T3.2.3: Implement standard traits for basic types
    wyn_implement_standard_traits_for_basic_types();
}

// Register a trait definition
void wyn_register_trait(void* trait_ptr) {
    TraitStmt* trait_stmt = (TraitStmt*)trait_ptr;
    if (!trait_stmt) return;
    
    Trait* trait = malloc(sizeof(Trait));
    if (!trait) return;
    
    trait->name = trait_stmt->name;
    trait->type_params = trait_stmt->type_params;
    trait->type_param_count = trait_stmt->type_param_count;
    trait->methods = trait_stmt->methods;
    trait->method_count = trait_stmt->method_count;
    trait->method_has_default = trait_stmt->method_has_default;
    trait->next = g_traits;
    
    g_traits = trait;
    g_trait_count++;
}

// Find a trait by name
Trait* wyn_find_trait(Token name) {
    Trait* current = g_traits;
    while (current) {
        if (current->name.length == name.length &&
            memcmp(current->name.start, name.start, name.length) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Register a trait implementation
void wyn_register_trait_impl(Token trait_name, Token type_name, void** methods, int method_count) {
    TraitImpl* impl = malloc(sizeof(TraitImpl));
    if (!impl) return;
    
    impl->trait_name = trait_name;
    impl->type_name = type_name;
    impl->methods = (FnStmt**)methods;
    impl->method_count = method_count;
    impl->next = g_trait_impls;
    
    g_trait_impls = impl;
    g_trait_impl_count++;
}

// Find trait implementation for a type
TraitImpl* wyn_find_trait_impl(Token trait_name, Token type_name) {
    TraitImpl* current = g_trait_impls;
    while (current) {
        if (current->trait_name.length == trait_name.length &&
            memcmp(current->trait_name.start, trait_name.start, trait_name.length) == 0 &&
            current->type_name.length == type_name.length &&
            memcmp(current->type_name.start, type_name.start, type_name.length) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Check if a type implements a trait
bool wyn_type_implements_trait(Token type_name, Token trait_name) {
    return wyn_find_trait_impl(trait_name, type_name) != NULL;
}

// Validate trait implementation completeness
bool wyn_validate_trait_impl(Trait* trait, TraitImpl* impl) {
    if (!trait || !impl) return false;
    
    // Check that all required methods are implemented
    for (int i = 0; i < trait->method_count; i++) {
        if (!trait->method_has_default[i]) {
            // This method is required - check if it's implemented
            bool found = false;
            for (int j = 0; j < impl->method_count; j++) {
                if (trait->methods[i]->name.length == impl->methods[j]->name.length &&
                    memcmp(trait->methods[i]->name.start, impl->methods[j]->name.start, 
                           trait->methods[i]->name.length) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false; // Required method not implemented
            }
        }
    }
    
    return true;
}

// Get trait system statistics
typedef struct {
    size_t traits_registered;
    size_t trait_impls_registered;
    size_t methods_defined;
    size_t default_methods;
} TraitStats;

TraitStats wyn_get_trait_stats(void) {
    TraitStats stats = {0};
    stats.traits_registered = g_trait_count;
    stats.trait_impls_registered = g_trait_impl_count;
    
    // Count methods and defaults
    Trait* current = g_traits;
    while (current) {
        stats.methods_defined += current->method_count;
        for (int i = 0; i < current->method_count; i++) {
            if (current->method_has_default[i]) {
                stats.default_methods++;
            }
        }
        current = current->next;
    }
    
    return stats;
}

// Print trait system statistics
void wyn_print_trait_stats(void) {
    TraitStats stats = wyn_get_trait_stats();
    
    printf("=== Trait System Statistics ===\n");
    printf("Traits registered: %zu\n", stats.traits_registered);
    printf("Trait implementations: %zu\n", stats.trait_impls_registered);
    printf("Methods defined: %zu\n", stats.methods_defined);
    printf("Default methods: %zu\n", stats.default_methods);
    printf("==============================\n");
}

// List all registered traits
void wyn_list_traits(void) {
    printf("=== Registered Traits ===\n");
    
    Trait* current = g_traits;
    int count = 0;
    
    while (current) {
        printf("%d. %.*s", ++count, (int)current->name.length, current->name.start);
        
        if (current->type_param_count > 0) {
            printf("<");
            for (int i = 0; i < current->type_param_count; i++) {
                printf("%.*s", (int)current->type_params[i].length, current->type_params[i].start);
                if (i < current->type_param_count - 1) {
                    printf(", ");
                }
            }
            printf(">");
        }
        
        printf(" (%d methods", current->method_count);
        
        // Count default methods
        int defaults = 0;
        for (int i = 0; i < current->method_count; i++) {
            if (current->method_has_default[i]) {
                defaults++;
            }
        }
        if (defaults > 0) {
            printf(", %d defaults", defaults);
        }
        
        printf(")\n");
        current = current->next;
    }
    
    if (count == 0) {
        printf("No traits registered.\n");
    }
    
    printf("========================\n");
}

// Cleanup trait system
void wyn_cleanup_traits(void) {
    // Cleanup traits
    Trait* current_trait = g_traits;
    while (current_trait) {
        Trait* next = current_trait->next;
        free(current_trait);
        current_trait = next;
    }
    g_traits = NULL;
    g_trait_count = 0;
    
    // Cleanup trait implementations
    TraitImpl* current_impl = g_trait_impls;
    while (current_impl) {
        TraitImpl* next = current_impl->next;
        free(current_impl);
        current_impl = next;
    }
    g_trait_impls = NULL;
    g_trait_impl_count = 0;
}

// T3.2.3: Check if a trait is a standard trait
bool wyn_is_standard_trait(Token trait_name) {
    const char* standard_traits[] = {"Eq", "Ord", "Clone", "Copy", "Display", "Debug", "Iterator"};
    const int num_standard_traits = 7;
    
    for (int i = 0; i < num_standard_traits; i++) {
        size_t len = strlen(standard_traits[i]);
        if ((size_t)trait_name.length == len && 
            memcmp(trait_name.start, standard_traits[i], len) == 0) {
            return true;
        }
    }
    return false;
}

// T3.2.3: Automatically implement standard traits for basic types
void wyn_implement_standard_traits_for_basic_types(void) {
    // Define basic type tokens
    Token int_type = {TOKEN_IDENT, "int", 3, 0};
    Token float_type = {TOKEN_IDENT, "float", 5, 0};
    Token string_type = {TOKEN_IDENT, "string", 6, 0};
    Token bool_type = {TOKEN_IDENT, "bool", 4, 0};
    
    // Define trait tokens
    Token eq_trait = {TOKEN_IDENT, "Eq", 2, 0};
    Token ord_trait = {TOKEN_IDENT, "Ord", 3, 0};
    Token clone_trait = {TOKEN_IDENT, "Clone", 5, 0};
    Token copy_trait = {TOKEN_IDENT, "Copy", 4, 0};
    Token display_trait = {TOKEN_IDENT, "Display", 7, 0};
    Token debug_trait = {TOKEN_IDENT, "Debug", 5, 0};
    
    // Implement traits for int
    wyn_register_trait_impl(eq_trait, int_type, NULL, 0);
    wyn_register_trait_impl(ord_trait, int_type, NULL, 0);
    wyn_register_trait_impl(clone_trait, int_type, NULL, 0);
    wyn_register_trait_impl(copy_trait, int_type, NULL, 0);
    wyn_register_trait_impl(display_trait, int_type, NULL, 0);
    wyn_register_trait_impl(debug_trait, int_type, NULL, 0);
    
    // Implement traits for float
    wyn_register_trait_impl(eq_trait, float_type, NULL, 0);
    wyn_register_trait_impl(ord_trait, float_type, NULL, 0);
    wyn_register_trait_impl(clone_trait, float_type, NULL, 0);
    wyn_register_trait_impl(copy_trait, float_type, NULL, 0);
    wyn_register_trait_impl(display_trait, float_type, NULL, 0);
    wyn_register_trait_impl(debug_trait, float_type, NULL, 0);
    
    // Implement traits for string
    wyn_register_trait_impl(eq_trait, string_type, NULL, 0);
    wyn_register_trait_impl(ord_trait, string_type, NULL, 0);
    wyn_register_trait_impl(clone_trait, string_type, NULL, 0);
    wyn_register_trait_impl(display_trait, string_type, NULL, 0);
    wyn_register_trait_impl(debug_trait, string_type, NULL, 0);
    // Note: string does not implement Copy (it's not a simple copy type)
    
    // Implement traits for bool
    wyn_register_trait_impl(eq_trait, bool_type, NULL, 0);
    wyn_register_trait_impl(ord_trait, bool_type, NULL, 0);
    wyn_register_trait_impl(clone_trait, bool_type, NULL, 0);
    wyn_register_trait_impl(copy_trait, bool_type, NULL, 0);
    wyn_register_trait_impl(display_trait, bool_type, NULL, 0);
    wyn_register_trait_impl(debug_trait, bool_type, NULL, 0);
}

// T3.2.3: Get standard trait method name for a trait
const char* wyn_get_standard_trait_method(Token trait_name) {
    if (trait_name.length == 2 && memcmp(trait_name.start, "Eq", 2) == 0) {
        return "eq";
    } else if (trait_name.length == 3 && memcmp(trait_name.start, "Ord", 3) == 0) {
        return "cmp";
    } else if (trait_name.length == 5 && memcmp(trait_name.start, "Clone", 5) == 0) {
        return "clone";
    } else if (trait_name.length == 7 && memcmp(trait_name.start, "Display", 7) == 0) {
        return "to_string";
    } else if (trait_name.length == 5 && memcmp(trait_name.start, "Debug", 5) == 0) {
        return "debug_string";
    } else if (trait_name.length == 8 && memcmp(trait_name.start, "Iterator", 8) == 0) {
        return "next";
    }
    return NULL;
}

// T3.2.3: Check if a type can automatically derive a trait
bool wyn_can_derive_trait(Token type_name, Token trait_name) {
    // For now, allow derivation for basic types and standard traits
    if (wyn_is_standard_trait(trait_name)) {
        // Basic types can derive most standard traits
        if ((type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) ||
            (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) ||
            (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0)) {
            return true;
        }
        
        // String can derive most traits except Copy
        if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
            return !(trait_name.length == 4 && memcmp(trait_name.start, "Copy", 4) == 0);
        }
    }
    
    return false;
}

// T3.2.3: Standard Traits - Create standard trait definitions
void wyn_register_standard_traits(void) {
    // Create standard trait tokens
    Token eq_trait = {TOKEN_IDENT, "Eq", 2, 0};
    Token ord_trait = {TOKEN_IDENT, "Ord", 3, 0};
    Token clone_trait = {TOKEN_IDENT, "Clone", 5, 0};
    Token copy_trait = {TOKEN_IDENT, "Copy", 4, 0};
    Token display_trait = {TOKEN_IDENT, "Display", 7, 0};
    Token debug_trait = {TOKEN_IDENT, "Debug", 5, 0};
    Token iterator_trait = {TOKEN_IDENT, "Iterator", 8, 0};
    
    // Register Eq trait
    TraitStmt eq_stmt = {0};
    eq_stmt.name = eq_trait;
    eq_stmt.type_param_count = 0;
    eq_stmt.type_params = NULL;
    eq_stmt.method_count = 1;
    eq_stmt.methods = malloc(sizeof(FnStmt*));
    eq_stmt.method_has_default = malloc(sizeof(bool));
    eq_stmt.methods[0] = malloc(sizeof(FnStmt));
    eq_stmt.methods[0]->name = (Token){TOKEN_IDENT, "eq", 2, 0};
    eq_stmt.method_has_default[0] = false;
    wyn_register_trait(&eq_stmt);
    
    // Register Ord trait
    TraitStmt ord_stmt = {0};
    ord_stmt.name = ord_trait;
    ord_stmt.type_param_count = 0;
    ord_stmt.type_params = NULL;
    ord_stmt.method_count = 1;
    ord_stmt.methods = malloc(sizeof(FnStmt*));
    ord_stmt.method_has_default = malloc(sizeof(bool));
    ord_stmt.methods[0] = malloc(sizeof(FnStmt));
    ord_stmt.methods[0]->name = (Token){TOKEN_IDENT, "cmp", 3, 0};
    ord_stmt.method_has_default[0] = false;
    wyn_register_trait(&ord_stmt);
    
    // Register Clone trait
    TraitStmt clone_stmt = {0};
    clone_stmt.name = clone_trait;
    clone_stmt.type_param_count = 0;
    clone_stmt.type_params = NULL;
    clone_stmt.method_count = 1;
    clone_stmt.methods = malloc(sizeof(FnStmt*));
    clone_stmt.method_has_default = malloc(sizeof(bool));
    clone_stmt.methods[0] = malloc(sizeof(FnStmt));
    clone_stmt.methods[0]->name = (Token){TOKEN_IDENT, "clone", 5, 0};
    clone_stmt.method_has_default[0] = false;
    wyn_register_trait(&clone_stmt);
    
    // Register Copy trait (marker trait - no methods)
    TraitStmt copy_stmt = {0};
    copy_stmt.name = copy_trait;
    copy_stmt.type_param_count = 0;
    copy_stmt.type_params = NULL;
    copy_stmt.method_count = 0;
    copy_stmt.methods = NULL;
    copy_stmt.method_has_default = NULL;
    wyn_register_trait(&copy_stmt);
    
    // Register Display trait
    TraitStmt display_stmt = {0};
    display_stmt.name = display_trait;
    display_stmt.type_param_count = 0;
    display_stmt.type_params = NULL;
    display_stmt.method_count = 1;
    display_stmt.methods = malloc(sizeof(FnStmt*));
    display_stmt.method_has_default = malloc(sizeof(bool));
    display_stmt.methods[0] = malloc(sizeof(FnStmt));
    display_stmt.methods[0]->name = (Token){TOKEN_IDENT, "to_string", 9, 0};
    display_stmt.method_has_default[0] = false;
    wyn_register_trait(&display_stmt);
    
    // Register Debug trait with default implementation
    TraitStmt debug_stmt = {0};
    debug_stmt.name = debug_trait;
    debug_stmt.type_param_count = 0;
    debug_stmt.type_params = NULL;
    debug_stmt.method_count = 1;
    debug_stmt.methods = malloc(sizeof(FnStmt*));
    debug_stmt.method_has_default = malloc(sizeof(bool));
    debug_stmt.methods[0] = malloc(sizeof(FnStmt));
    debug_stmt.methods[0]->name = (Token){TOKEN_IDENT, "debug_string", 12, 0};
    debug_stmt.method_has_default[0] = true; // Has default implementation
    wyn_register_trait(&debug_stmt);
    
    // Register Iterator trait
    TraitStmt iterator_stmt = {0};
    iterator_stmt.name = iterator_trait;
    iterator_stmt.type_param_count = 1;
    iterator_stmt.type_params = malloc(sizeof(Token));
    iterator_stmt.type_params[0] = (Token){TOKEN_IDENT, "Item", 4, 0};
    iterator_stmt.method_count = 1;
    iterator_stmt.methods = malloc(sizeof(FnStmt*));
    iterator_stmt.method_has_default = malloc(sizeof(bool));
    iterator_stmt.methods[0] = malloc(sizeof(FnStmt));
    iterator_stmt.methods[0]->name = (Token){TOKEN_IDENT, "next", 4, 0};
    iterator_stmt.method_has_default[0] = false;
    wyn_register_trait(&iterator_stmt);
    
    // T3.4.2: Register closure traits (Fn, FnMut, FnOnce)
    Token fn_trait = {TOKEN_IDENT, "Fn", 2, 0};
    Token fn_mut_trait = {TOKEN_IDENT, "FnMut", 5, 0};
    Token fn_once_trait = {TOKEN_IDENT, "FnOnce", 6, 0};
    
    // Register Fn trait
    TraitStmt fn_stmt = {0};
    fn_stmt.name = fn_trait;
    fn_stmt.type_param_count = 1;
    fn_stmt.type_params = malloc(sizeof(Token));
    fn_stmt.type_params[0] = (Token){TOKEN_IDENT, "Args", 4, 0};
    fn_stmt.method_count = 1;
    fn_stmt.methods = malloc(sizeof(FnStmt*));
    fn_stmt.method_has_default = malloc(sizeof(bool));
    fn_stmt.methods[0] = malloc(sizeof(FnStmt));
    fn_stmt.methods[0]->name = (Token){TOKEN_IDENT, "call", 4, 0};
    fn_stmt.method_has_default[0] = false;
    wyn_register_trait(&fn_stmt);
    
    // Register FnMut trait
    TraitStmt fn_mut_stmt = {0};
    fn_mut_stmt.name = fn_mut_trait;
    fn_mut_stmt.type_param_count = 1;
    fn_mut_stmt.type_params = malloc(sizeof(Token));
    fn_mut_stmt.type_params[0] = (Token){TOKEN_IDENT, "Args", 4, 0};
    fn_mut_stmt.method_count = 1;
    fn_mut_stmt.methods = malloc(sizeof(FnStmt*));
    fn_mut_stmt.method_has_default = malloc(sizeof(bool));
    fn_mut_stmt.methods[0] = malloc(sizeof(FnStmt));
    fn_mut_stmt.methods[0]->name = (Token){TOKEN_IDENT, "call_mut", 8, 0};
    fn_mut_stmt.method_has_default[0] = false;
    wyn_register_trait(&fn_mut_stmt);
    
    // Register FnOnce trait
    TraitStmt fn_once_stmt = {0};
    fn_once_stmt.name = fn_once_trait;
    fn_once_stmt.type_param_count = 1;
    fn_once_stmt.type_params = malloc(sizeof(Token));
    fn_once_stmt.type_params[0] = (Token){TOKEN_IDENT, "Args", 4, 0};
    fn_once_stmt.method_count = 1;
    fn_once_stmt.methods = malloc(sizeof(FnStmt*));
    fn_once_stmt.method_has_default = malloc(sizeof(bool));
    fn_once_stmt.methods[0] = malloc(sizeof(FnStmt));
    fn_once_stmt.methods[0]->name = (Token){TOKEN_IDENT, "call_once", 9, 0};
    fn_once_stmt.method_has_default[0] = false;
    wyn_register_trait(&fn_once_stmt);
}
