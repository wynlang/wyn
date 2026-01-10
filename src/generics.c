#include "types.h"
#include "ast.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// T3.1.1: Generic Functions Implementation - Simplified Version
// Basic generic programming support with function templates

// Forward declarations
Type* check_expr(Expr* expr, SymbolTable* scope);
Symbol* find_symbol(SymbolTable* scope, Token name);
Type* make_type(TypeKind kind);

// T3.1.3: Type constraint structure
typedef struct TypeConstraint {
    Token trait_name;           // Name of the trait constraint
    struct TypeConstraint* next; // For multiple constraints (T: Display + Debug)
} TypeConstraint;

// Generic function registry
typedef struct GenericFunction {
    Token name;
    Token* type_params;
    int type_param_count;
    TypeConstraint** constraints;  // T3.1.3: Constraints for each type parameter
    FnStmt* original_fn;
    struct GenericFunction* next;
} GenericFunction;

// T3.1.2: Generic struct registry
typedef struct GenericStruct {
    Token name;
    Token* type_params;
    int type_param_count;
    TypeConstraint** constraints;  // T3.1.3: Constraints for each type parameter
    StructStmt* original_struct;
    struct GenericStruct* next;
} GenericStruct;

static GenericFunction* g_generic_functions = NULL;
static GenericStruct* g_generic_structs = NULL;  // T3.1.2: Generic struct registry
static size_t g_generic_function_count = 0;
static size_t g_generic_struct_count = 0;        // T3.1.2: Generic struct count

// Initialize generic system
void wyn_generics_init(void) {
    g_generic_functions = NULL;
    g_generic_structs = NULL;
    g_generic_function_count = 0;
    g_generic_struct_count = 0;
}

// T3.1.3: Create a type constraint
TypeConstraint* wyn_create_constraint(Token trait_name) {
    TypeConstraint* constraint = malloc(sizeof(TypeConstraint));
    if (!constraint) return NULL;
    
    constraint->trait_name = trait_name;
    constraint->next = NULL;
    
    return constraint;
}

// T3.1.3: Add a constraint to a constraint list
TypeConstraint* wyn_add_constraint(TypeConstraint* constraints, Token trait_name) {
    TypeConstraint* new_constraint = wyn_create_constraint(trait_name);
    if (!new_constraint) return constraints;
    
    if (!constraints) {
        return new_constraint;
    }
    
    // Add to the end of the constraint list
    TypeConstraint* current = constraints;
    while (current->next) {
        current = current->next;
    }
    current->next = new_constraint;
    
    return constraints;
}

// T3.2.2: Trait Bounds and Constraints - Enhanced constraint checking with trait validation
bool wyn_check_constraints(Type* concrete_type, TypeConstraint* constraints) {
    if (!concrete_type) return false;
    if (!constraints) return true; // No constraints to check
    
    // Check each constraint in the list
    TypeConstraint* current = constraints;
    while (current) {
        // Check if the concrete type implements the required trait
        Token type_name = concrete_type->name;
        Token trait_name = current->trait_name;
        
        if (!wyn_type_implements_trait(type_name, trait_name)) {
            // Type does not implement required trait
            printf("Error: Type %.*s does not implement trait %.*s\n",
                   (int)type_name.length, type_name.start,
                   (int)trait_name.length, trait_name.start);
            return false;
        }
        
        current = current->next;
    }
    
    return true;
}

// T3.2.2: Enhanced where clause constraint parsing with trait bounds
TypeConstraint** wyn_parse_where_constraints(Token* type_params, int type_param_count) {
    // Enhanced implementation for parsing "where T: Display + Debug" syntax
    TypeConstraint** constraints = malloc(sizeof(TypeConstraint*) * type_param_count);
    
    for (int i = 0; i < type_param_count; i++) {
        constraints[i] = NULL; // No constraints by default
        
        // For demonstration, add some common trait constraints
        // In a full parser implementation, this would parse actual where clause syntax
        
        // Example: Add Display constraint to first type parameter
        if (i == 0) {
            Token display_trait = {TOKEN_IDENT, "Display", 7, 0};
            constraints[i] = wyn_create_constraint(display_trait);
            
            // Example: Add Debug constraint as well (T: Display + Debug)
            Token debug_trait = {TOKEN_IDENT, "Debug", 5, 0};
            constraints[i] = wyn_add_constraint(constraints[i], debug_trait);
        }
    }
    
    return constraints;
}

// T3.2.2: Free constraint list
void wyn_free_constraints(TypeConstraint* constraints) {
    while (constraints) {
        TypeConstraint* next = constraints->next;
        free(constraints);
        constraints = next;
    }
}

// T3.2.2: Check if a type parameter has specific trait bound
bool wyn_has_trait_bound(TypeConstraint* constraints, Token trait_name) {
    TypeConstraint* current = constraints;
    while (current) {
        if (current->trait_name.length == trait_name.length &&
            memcmp(current->trait_name.start, trait_name.start, trait_name.length) == 0) {
            return true;
        }
        current = current->next;
    }
    return false;
}

// T3.2.2: Get all trait bounds for a type parameter as a string (for debugging)
void wyn_get_trait_bounds_string(TypeConstraint* constraints, char* buffer, size_t buffer_size) {
    if (!constraints) {
        strncpy(buffer, "no bounds", buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return;
    }
    
    buffer[0] = '\0';
    TypeConstraint* current = constraints;
    bool first = true;
    
    while (current && strlen(buffer) < buffer_size - 20) {
        if (!first) {
            strncat(buffer, " + ", buffer_size - strlen(buffer) - 1);
        }
        
        strncat(buffer, current->trait_name.start, 
                (current->trait_name.length < buffer_size - strlen(buffer) - 1) ? 
                current->trait_name.length : buffer_size - strlen(buffer) - 1);
        
        first = false;
        current = current->next;
    }
}

// T3.2.2: Validate that a generic function call satisfies all trait bounds
bool wyn_validate_trait_bounds(GenericFunction* generic_fn, Type** type_args, int type_arg_count) {
    if (!generic_fn || !type_args) return false;
    
    // Check that we have the right number of type arguments
    if (type_arg_count != generic_fn->type_param_count) {
        printf("Error: Generic function %.*s expects %d type arguments, got %d\n",
               (int)generic_fn->name.length, generic_fn->name.start,
               generic_fn->type_param_count, type_arg_count);
        return false;
    }
    
    // Check trait bounds for each type argument
    for (int i = 0; i < type_arg_count; i++) {
        if (generic_fn->constraints && generic_fn->constraints[i]) {
            if (!wyn_check_constraints(type_args[i], generic_fn->constraints[i])) {
                char bounds_str[256];
                wyn_get_trait_bounds_string(generic_fn->constraints[i], bounds_str, sizeof(bounds_str));
                printf("Error: Type argument %d for %.*s does not satisfy bounds: %s\n",
                       i + 1, (int)generic_fn->name.length, generic_fn->name.start, bounds_str);
                return false;
            }
        }
    }
    
    return true;
}

// Register a generic function
void wyn_register_generic_function(void* fn_ptr) {
    FnStmt* fn = (FnStmt*)fn_ptr;
    if (!fn || fn->type_param_count == 0) {
        return; // Not a generic function
    }
    
    GenericFunction* generic_fn = malloc(sizeof(GenericFunction));
    if (!generic_fn) return;
    
    generic_fn->name = fn->name;
    generic_fn->type_params = fn->type_params;
    generic_fn->type_param_count = fn->type_param_count;
    generic_fn->original_fn = fn;
    generic_fn->next = g_generic_functions;
    
    // T3.1.3: Initialize constraints (placeholder for now)
    generic_fn->constraints = wyn_parse_where_constraints(fn->type_params, fn->type_param_count);
    
    g_generic_functions = generic_fn;
    g_generic_function_count++;
}

// Find a generic function by name
GenericFunction* wyn_find_generic_function(Token name) {
    GenericFunction* current = g_generic_functions;
    while (current) {
        if (current->name.length == name.length &&
            memcmp(current->name.start, name.start, name.length) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// T3.1.1: Instantiate a generic function with concrete types
Type* wyn_instantiate_generic_function(const char* name, Type** type_args, int arg_count) {
    if (!name || !type_args || arg_count <= 0) {
        return NULL;
    }
    
    // Create a token for the function name
    Token name_token;
    name_token.start = name;
    name_token.length = strlen(name);
    // Note: lexeme field doesn't exist in Token, using start instead
    
    // Find the generic function
    GenericFunction* generic_fn = wyn_find_generic_function(name_token);
    if (!generic_fn) {
        return NULL;
    }
    
    // Check if argument count matches type parameter count
    if (arg_count != generic_fn->type_param_count) {
        return NULL;
    }
    
    // For now, return a simple function type to indicate successful instantiation
    // Full monomorphization will be implemented later
    Type* fn_type = make_type(TYPE_FUNCTION);
    if (fn_type) {
        fn_type->name = generic_fn->name;
    }
    
    return fn_type;
}

// T3.1.2: Register a generic struct
void wyn_register_generic_struct(void* struct_ptr) {
    StructStmt* struct_stmt = (StructStmt*)struct_ptr;
    if (!struct_stmt || struct_stmt->type_param_count == 0) {
        return; // Not a generic struct
    }
    
    GenericStruct* generic_struct = malloc(sizeof(GenericStruct));
    if (!generic_struct) return;
    
    generic_struct->name = struct_stmt->name;
    generic_struct->type_params = struct_stmt->type_params;
    generic_struct->type_param_count = struct_stmt->type_param_count;
    generic_struct->original_struct = struct_stmt;
    generic_struct->next = g_generic_structs;
    
    // T3.1.3: Initialize constraints (placeholder for now)
    generic_struct->constraints = wyn_parse_where_constraints(struct_stmt->type_params, struct_stmt->type_param_count);
    
    g_generic_structs = generic_struct;
    g_generic_struct_count++;
}

// T3.1.2: Find a generic struct by name
GenericStruct* wyn_find_generic_struct(Token name) {
    GenericStruct* current = g_generic_structs;
    while (current) {
        if (current->name.length == name.length &&
            memcmp(current->name.start, name.start, name.length) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// T3.1.2: Check if a struct is generic
bool wyn_is_generic_struct(Token struct_name) {
    return wyn_find_generic_struct(struct_name) != NULL;
}

// T3.1.2: Generate a monomorphic struct name
void wyn_generate_monomorphic_struct_name(Token base_name, Type** type_args, int type_arg_count, 
                                          char* buffer, size_t buffer_size) {
    // Create a unique name for the monomorphic struct instance
    snprintf(buffer, buffer_size, "%.*s_", (int)base_name.length, base_name.start);
    
    for (int i = 0; i < type_arg_count; i++) {
        if (type_args[i]) {
            switch (type_args[i]->kind) {
                case TYPE_INT:
                    strncat(buffer, "int", buffer_size - strlen(buffer) - 1);
                    break;
                case TYPE_FLOAT:
                    strncat(buffer, "float", buffer_size - strlen(buffer) - 1);
                    break;
                case TYPE_STRING:
                    strncat(buffer, "string", buffer_size - strlen(buffer) - 1);
                    break;
                case TYPE_BOOL:
                    strncat(buffer, "bool", buffer_size - strlen(buffer) - 1);
                    break;
                default:
                    strncat(buffer, "unknown", buffer_size - strlen(buffer) - 1);
                    break;
            }
            if (i < type_arg_count - 1) {
                strncat(buffer, "_", buffer_size - strlen(buffer) - 1);
            }
        }
    }
}

// T3.2.2: Enhanced generic function call validation with trait bounds
bool wyn_validate_generic_call(GenericFunction* generic_fn, Type** type_args, int type_arg_count) {
    return wyn_validate_trait_bounds(generic_fn, type_args, type_arg_count);
}

// T3.2.2: Enhanced generic struct instantiation validation with trait bounds
bool wyn_validate_generic_struct(GenericStruct* generic_struct, Type** type_args, int type_arg_count) {
    if (!generic_struct || !type_args) return false;
    
    // Check that we have the right number of type arguments
    if (type_arg_count != generic_struct->type_param_count) {
        printf("Error: Generic struct %.*s expects %d type arguments, got %d\n",
               (int)generic_struct->name.length, generic_struct->name.start,
               generic_struct->type_param_count, type_arg_count);
        return false;
    }
    
    // Check trait bounds for each type argument
    for (int i = 0; i < type_arg_count; i++) {
        if (generic_struct->constraints && generic_struct->constraints[i]) {
            if (!wyn_check_constraints(type_args[i], generic_struct->constraints[i])) {
                char bounds_str[256];
                wyn_get_trait_bounds_string(generic_struct->constraints[i], bounds_str, sizeof(bounds_str));
                printf("Error: Type argument %d for struct %.*s does not satisfy bounds: %s\n",
                       i + 1, (int)generic_struct->name.length, generic_struct->name.start, bounds_str);
                return false;
            }
        }
    }
    
    return true;
}
void* wyn_monomorphize_struct(GenericStruct* generic_struct, Type** type_args, int type_arg_count) {
    if (!generic_struct || !type_args) {
        return NULL;
    }
    
    // Create a new struct instance with concrete types
    StructStmt* monomorphic_struct = malloc(sizeof(StructStmt));
    if (!monomorphic_struct) return NULL;
    
    // Copy basic struct structure
    *monomorphic_struct = *generic_struct->original_struct;
    
    // Clear generic parameters in monomorphic version
    monomorphic_struct->type_params = NULL;
    monomorphic_struct->type_param_count = 0;
    
    // Copy field information (simplified for now)
    monomorphic_struct->fields = malloc(sizeof(Token) * generic_struct->original_struct->field_count);
    monomorphic_struct->field_types = malloc(sizeof(Expr*) * generic_struct->original_struct->field_count);
    monomorphic_struct->field_arc_managed = malloc(sizeof(bool) * generic_struct->original_struct->field_count);
    
    for (int i = 0; i < generic_struct->original_struct->field_count; i++) {
        monomorphic_struct->fields[i] = generic_struct->original_struct->fields[i];
        monomorphic_struct->field_types[i] = generic_struct->original_struct->field_types[i];
        monomorphic_struct->field_arc_managed[i] = generic_struct->original_struct->field_arc_managed[i];
    }
    
    return (void*)monomorphic_struct;
}

// Check if a function call is calling a generic function
bool wyn_is_generic_function_call(Token function_name) {
    return wyn_find_generic_function(function_name) != NULL;
}

// Simple type inference for generic function calls
Type* wyn_infer_generic_call_type(Token function_name, Expr** args, int arg_count) {
    GenericFunction* generic_fn = wyn_find_generic_function(function_name);
    if (!generic_fn) {
        return NULL;
    }
    
    // For now, return a simple type based on the first argument
    // This is a placeholder for proper type inference
    if (arg_count > 0 && args[0]) {
        // Simple heuristic: return the type of the first argument
        return make_type(TYPE_INT); // Placeholder
    }
    
    return make_type(TYPE_INT); // Default
}

// Generate a monomorphic function name
void wyn_generate_monomorphic_name(Token base_name, Type** type_args, int type_arg_count, 
                                   char* buffer, size_t buffer_size) {
    // Create a unique name for the monomorphic instance
    snprintf(buffer, buffer_size, "%.*s_", (int)base_name.length, base_name.start);
    
    for (int i = 0; i < type_arg_count; i++) {
        if (type_args[i]) {
            switch (type_args[i]->kind) {
                case TYPE_INT:
                    strncat(buffer, "int", buffer_size - strlen(buffer) - 1);
                    break;
                case TYPE_FLOAT:
                    strncat(buffer, "float", buffer_size - strlen(buffer) - 1);
                    break;
                case TYPE_STRING:
                    strncat(buffer, "string", buffer_size - strlen(buffer) - 1);
                    break;
                case TYPE_BOOL:
                    strncat(buffer, "bool", buffer_size - strlen(buffer) - 1);
                    break;
                default:
                    strncat(buffer, "unknown", buffer_size - strlen(buffer) - 1);
                    break;
            }
            if (i < type_arg_count - 1) {
                strncat(buffer, "_", buffer_size - strlen(buffer) - 1);
            }
        }
    }
}

// Check if a type parameter name matches a given token
bool wyn_is_type_parameter(GenericFunction* generic_fn, Token type_name) {
    for (int i = 0; i < generic_fn->type_param_count; i++) {
        if (generic_fn->type_params[i].length == type_name.length &&
            memcmp(generic_fn->type_params[i].start, type_name.start, type_name.length) == 0) {
            return true;
        }
    }
    return false;
}

// Get generic system statistics
GenericStats wyn_get_generic_stats(void) {
    GenericStats stats = {
        .generic_functions_registered = g_generic_function_count,
        .types_instantiated = g_generic_struct_count,  // T3.1.2: Include generic structs
        .monomorphizations_generated = 0, // Placeholder
        .generics_enabled = true
    };
    return stats;
}

// Print generic system statistics
void wyn_print_generic_stats(void) {
    GenericStats stats = wyn_get_generic_stats();
    
    printf("=== Generic System Statistics ===\n");
    printf("Generic functions registered: %zu\n", stats.generic_functions_registered);
    printf("Generic structs registered: %zu\n", stats.types_instantiated);  // T3.1.2
    printf("Monomorphizations generated: %zu\n", stats.monomorphizations_generated);
    printf("Generics enabled: %s\n", stats.generics_enabled ? "Yes" : "No");
    printf("=================================\n");
}

// List all registered generic functions
void wyn_list_generic_functions(void) {
    printf("=== Registered Generic Functions ===\n");
    
    GenericFunction* current = g_generic_functions;
    int count = 0;
    
    while (current) {
        printf("%d. %.*s<", ++count, (int)current->name.length, current->name.start);
        
        for (int i = 0; i < current->type_param_count; i++) {
            printf("%.*s", (int)current->type_params[i].length, current->type_params[i].start);
            
            // T3.1.3: Show constraints if any
            if (current->constraints && current->constraints[i]) {
                printf(": ");
                TypeConstraint* constraint = current->constraints[i];
                while (constraint) {
                    printf("%.*s", (int)constraint->trait_name.length, constraint->trait_name.start);
                    if (constraint->next) {
                        printf(" + ");
                    }
                    constraint = constraint->next;
                }
            }
            
            if (i < current->type_param_count - 1) {
                printf(", ");
            }
        }
        
        printf(">\n");
        current = current->next;
    }
    
    if (count == 0) {
        printf("No generic functions registered.\n");
    }
    
    printf("===================================\n");
}

// T3.1.2: List all registered generic structs
void wyn_list_generic_structs(void) {
    printf("=== Registered Generic Structs ===\n");
    
    GenericStruct* current = g_generic_structs;
    int count = 0;
    
    while (current) {
        printf("%d. %.*s<", ++count, (int)current->name.length, current->name.start);
        
        for (int i = 0; i < current->type_param_count; i++) {
            printf("%.*s", (int)current->type_params[i].length, current->type_params[i].start);
            
            // T3.1.3: Show constraints if any
            if (current->constraints && current->constraints[i]) {
                printf(": ");
                TypeConstraint* constraint = current->constraints[i];
                while (constraint) {
                    printf("%.*s", (int)constraint->trait_name.length, constraint->trait_name.start);
                    if (constraint->next) {
                        printf(" + ");
                    }
                    constraint = constraint->next;
                }
            }
            
            if (i < current->type_param_count - 1) {
                printf(", ");
            }
        }
        
        printf(">\n");
        current = current->next;
    }
    
    if (count == 0) {
        printf("No generic structs registered.\n");
    }
    
    printf("==================================\n");
}

// Cleanup generic system
void wyn_cleanup_generics(void) {
    // Cleanup generic functions
    GenericFunction* current_fn = g_generic_functions;
    while (current_fn) {
        GenericFunction* next = current_fn->next;
        
        // T3.1.3: Free constraints
        if (current_fn->constraints) {
            for (int i = 0; i < current_fn->type_param_count; i++) {
                wyn_free_constraints(current_fn->constraints[i]);
            }
            free(current_fn->constraints);
        }
        
        free(current_fn);
        current_fn = next;
    }
    g_generic_functions = NULL;
    g_generic_function_count = 0;
    
    // T3.1.2: Cleanup generic structs
    GenericStruct* current_struct = g_generic_structs;
    while (current_struct) {
        GenericStruct* next = current_struct->next;
        
        // T3.1.3: Free constraints
        if (current_struct->constraints) {
            for (int i = 0; i < current_struct->type_param_count; i++) {
                wyn_free_constraints(current_struct->constraints[i]);
            }
            free(current_struct->constraints);
        }
        
        free(current_struct);
        current_struct = next;
    }
    g_generic_structs = NULL;
    g_generic_struct_count = 0;
}
