#include "types.h"
#include "ast.h"
#include "memory.h"
#include "traits.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// T3.1.1: Generic Functions Implementation - Simplified Version
// Basic generic programming support with function templates

// Forward declarations
Type* check_expr(Expr* expr, SymbolTable* scope);
Symbol* find_symbol(SymbolTable* scope, Token name);
Type* make_type(TypeKind kind);
bool wyn_types_equal(Type* a, Type* b);

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

// T3.1.1: Generic instantiation tracking
typedef struct GenericInstantiation {
    Token function_name;
    Type** type_args;
    int type_arg_count;
    char* monomorphic_name;  // Generated name like "identity_int"
    struct GenericInstantiation* next;
} GenericInstantiation;

// T4.1: Generic struct instantiation tracking
typedef struct GenericStructInstantiation {
    Token struct_name;
    Type** type_args;
    int type_arg_count;
    char* monomorphic_name;  // Generated name like "Box_int"
    StructStmt* monomorphic_struct;  // The generated monomorphic struct
    struct GenericStructInstantiation* next;
} GenericStructInstantiation;

// Global registries
static GenericFunction* g_generic_functions = NULL;
static GenericStruct* g_generic_structs = NULL;
static int g_generic_function_count = 0;
static int g_generic_struct_count = 0;

static GenericInstantiation* g_instantiations = NULL;
static GenericStructInstantiation* g_struct_instantiations = NULL;

// Register a generic function instantiation
void wyn_register_generic_instantiation(Token func_name, Type** type_args, int type_arg_count) {
    // Check if this instantiation already exists
    GenericInstantiation* current = g_instantiations;
    while (current) {
        if (current->function_name.length == func_name.length &&
            memcmp(current->function_name.start, func_name.start, func_name.length) == 0 &&
            current->type_arg_count == type_arg_count) {
            
            // Check if type arguments match
            bool types_match = true;
            for (int i = 0; i < type_arg_count; i++) {
                if (!wyn_types_equal(current->type_args[i], type_args[i])) {
                    types_match = false;
                    break;
                }
            }
            
            if (types_match) {
                return; // Already registered
            }
        }
        current = current->next;
    }
    
    // Create new instantiation
    GenericInstantiation* inst = malloc(sizeof(GenericInstantiation));
    inst->function_name = func_name;
    inst->type_arg_count = type_arg_count;
    inst->type_args = malloc(sizeof(Type*) * type_arg_count);
    
    for (int i = 0; i < type_arg_count; i++) {
        inst->type_args[i] = type_args[i];
    }
    
    // Generate monomorphic name
    inst->monomorphic_name = malloc(256);
    wyn_generate_monomorphic_name(func_name, type_args, type_arg_count, 
                                  inst->monomorphic_name, 256);
    
    inst->next = g_instantiations;
    g_instantiations = inst;
}

// T4.1: Register a generic struct instantiation
void wyn_register_generic_struct_instantiation(Token struct_name, Type** type_args, int type_arg_count) {
    // Check if this instantiation already exists
    GenericStructInstantiation* current = g_struct_instantiations;
    while (current) {
        if (current->struct_name.length == struct_name.length &&
            memcmp(current->struct_name.start, struct_name.start, struct_name.length) == 0 &&
            current->type_arg_count == type_arg_count) {
            
            // Check if type arguments match
            bool types_match = true;
            for (int i = 0; i < type_arg_count; i++) {
                if (!wyn_types_equal(current->type_args[i], type_args[i])) {
                    types_match = false;
                    break;
                }
            }
            
            if (types_match) {
                return; // Already registered
            }
        }
        current = current->next;
    }
    
    // Create new struct instantiation
    GenericStructInstantiation* inst = malloc(sizeof(GenericStructInstantiation));
    inst->struct_name = struct_name;
    inst->type_arg_count = type_arg_count;
    inst->type_args = malloc(sizeof(Type*) * type_arg_count);
    
    for (int i = 0; i < type_arg_count; i++) {
        inst->type_args[i] = type_args[i];
    }
    
    // Generate monomorphic name
    inst->monomorphic_name = malloc(256);
    wyn_generate_monomorphic_struct_name(struct_name, type_args, type_arg_count, 
                                        inst->monomorphic_name, 256);
    
    // Generate the monomorphic struct
    GenericStruct* generic_struct = wyn_find_generic_struct(struct_name);
    if (generic_struct) {
        inst->monomorphic_struct = (StructStmt*)wyn_monomorphize_struct(generic_struct, type_args, type_arg_count);
        
        // Update the monomorphic struct name
        if (inst->monomorphic_struct) {
            // Create a new token for the monomorphic name
            Token new_name;
            new_name.start = inst->monomorphic_name;
            new_name.length = strlen(inst->monomorphic_name);
            new_name.line = struct_name.line;
            inst->monomorphic_struct->name = new_name;
        }
    } else {
        inst->monomorphic_struct = NULL;
    }
    
    inst->next = g_struct_instantiations;
    g_struct_instantiations = inst;
}

// Check if two types are equal (simplified)
bool wyn_types_equal(Type* a, Type* b) {
    if (!a || !b) return a == b;
    return a->kind == b->kind;
}

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

// T3.2.2: Enhanced constraint checking with trait validation
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
    (void)type_params;  // Reserved for future use
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
        
        size_t remaining = buffer_size - strlen(buffer) - 1;
        size_t copy_len = (size_t)current->trait_name.length < remaining ? 
                          (size_t)current->trait_name.length : remaining;
        strncat(buffer, current->trait_name.start, copy_len);
        
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
    (void)type_arg_count;  // Reserved for validation
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
        
        // Substitute type parameters with concrete types
        Expr* original_type = generic_struct->original_struct->field_types[i];
        if (original_type && original_type->type == EXPR_IDENT) {
            // Check if this is a type parameter
            bool is_type_param = false;
            int type_param_index = -1;
            
            for (int j = 0; j < generic_struct->type_param_count; j++) {
                if (generic_struct->type_params[j].length == original_type->token.length &&
                    memcmp(generic_struct->type_params[j].start, original_type->token.start, 
                           original_type->token.length) == 0) {
                    is_type_param = true;
                    type_param_index = j;
                    break;
                }
            }
            
            if (is_type_param && type_param_index < type_arg_count) {
                // Create a new expression with the concrete type
                Expr* concrete_type = malloc(sizeof(Expr));
                concrete_type->type = EXPR_IDENT;
                
                // Create a token for the concrete type name
                Token concrete_token;
                Type* concrete_type_obj = type_args[type_param_index];
                
                switch (concrete_type_obj->kind) {
                    case TYPE_INT:
                        concrete_token.start = "int";
                        concrete_token.length = 3;
                        break;
                    case TYPE_FLOAT:
                        concrete_token.start = "float";
                        concrete_token.length = 5;
                        break;
                    case TYPE_STRING:
                        concrete_token.start = "string";
                        concrete_token.length = 6;
                        break;
                    case TYPE_BOOL:
                        concrete_token.start = "bool";
                        concrete_token.length = 4;
                        break;
                    default:
                        concrete_token.start = "int";  // fallback
                        concrete_token.length = 3;
                        break;
                }
                concrete_token.line = original_type->token.line;
                concrete_type->token = concrete_token;
                
                monomorphic_struct->field_types[i] = concrete_type;
            } else {
                // Not a type parameter, copy as-is
                monomorphic_struct->field_types[i] = original_type;
            }
        } else {
            // Not an identifier, copy as-is
            monomorphic_struct->field_types[i] = original_type;
        }
        
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
    
    // Enhanced type inference from all arguments
    Type** inferred_types = malloc(sizeof(Type*) * arg_count);
    
    for (int i = 0; i < arg_count; i++) {
        Type* arg_type = NULL;
        
        if (args[i]) {
            switch (args[i]->type) {
                case EXPR_INT:
                    arg_type = make_type(TYPE_INT);
                    break;
                case EXPR_FLOAT:
                    arg_type = make_type(TYPE_FLOAT);
                    break;
                case EXPR_BOOL:
                    arg_type = make_type(TYPE_BOOL);
                    break;
                case EXPR_STRING:
                    arg_type = make_type(TYPE_STRING);
                    break;
                case EXPR_ARRAY:
                    arg_type = make_type(TYPE_ARRAY);
                    break;
                default:
                    arg_type = make_type(TYPE_INT); // Default
                    break;
            }
        } else {
            arg_type = make_type(TYPE_INT); // Default
        }
        
        inferred_types[i] = arg_type;
    }
    
    // For now, return the first argument's type as the function return type
    Type* return_type = (arg_count > 0) ? inferred_types[0] : make_type(TYPE_INT);
    
    free(inferred_types);
    return return_type;
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
                case TYPE_ARRAY:
                    strncat(buffer, "array", buffer_size - strlen(buffer) - 1);
                    break;
                case TYPE_STRUCT:
                    // For struct types, use the struct name if available
                    if (type_args[i]->struct_type.name.length > 0) {
                        size_t remaining = buffer_size - strlen(buffer) - 1;
                        size_t copy_len = (size_t)type_args[i]->struct_type.name.length < remaining ? 
                                          (size_t)type_args[i]->struct_type.name.length : remaining;
                        strncat(buffer, type_args[i]->struct_type.name.start, copy_len);
                    } else {
                        strncat(buffer, "struct", buffer_size - strlen(buffer) - 1);
                    }
                    break;
                case TYPE_GENERIC:
                    strncat(buffer, "generic", buffer_size - strlen(buffer) - 1);
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

// Emit a monomorphic function instance (placeholder - will be implemented in codegen integration)
void wyn_emit_monomorphic_function(GenericFunction* generic_fn, Type** type_args, int type_arg_count, const char* monomorphic_name) {
    (void)generic_fn;
    (void)type_args;
    (void)type_arg_count;
    (void)monomorphic_name;
    // This will be implemented when integrating with codegen
}

// Emit a monomorphic function declaration for codegen
void wyn_emit_monomorphic_function_declaration(void* original_fn_ptr, Type** type_args, int type_arg_count, const char* monomorphic_name) {
    FnStmt* original_fn = (FnStmt*)original_fn_ptr;
    // Forward declaration for wyn_emit function
    extern void wyn_emit(const char* fmt, ...);
    
    if (!original_fn || !type_args || !monomorphic_name) {
        return;
    }
    
    // Determine return type from original function's return type annotation
    const char* return_type = "int"; // Default
    
    if (original_fn->return_type && original_fn->return_type->type == EXPR_IDENT) {
        Token type_name = original_fn->return_type->token;
        if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
            return_type = "int";
        } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
            return_type = "char*";
        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
            return_type = "double";
        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
            return_type = "bool";
        }
    }
    
    // Generate function declaration
    wyn_emit("%s %s(", return_type, monomorphic_name);
    
    // Generate parameters with concrete types
    for (int i = 0; i < original_fn->param_count; i++) {
        if (i > 0) wyn_emit(", ");
        
        // Use type argument for parameter type
        const char* param_type = "int"; // Default
        char param_type_buf[256] = {0};
        
        if (i < type_arg_count && type_args[i]) {
            switch (type_args[i]->kind) {
                case TYPE_INT:
                    param_type = "int";
                    break;
                case TYPE_FLOAT:
                    param_type = "double";
                    break;
                case TYPE_STRING:
                    param_type = "const char*";
                    break;
                case TYPE_BOOL:
                    param_type = "bool";
                    break;
                case TYPE_STRUCT:
                    snprintf(param_type_buf, sizeof(param_type_buf), "%.*s",
                            type_args[i]->struct_type.name.length,
                            type_args[i]->struct_type.name.start);
                    param_type = param_type_buf;
                    break;
                default:
                    param_type = "int";
                    break;
            }
        }
        
        wyn_emit("%s %.*s", param_type, 
             (int)original_fn->params[i].length, original_fn->params[i].start);
    }
    
    wyn_emit(");\n");
}

// Emit a monomorphic function definition for codegen
void wyn_emit_monomorphic_function_definition(void* original_fn_ptr, Type** type_args, int type_arg_count, const char* monomorphic_name) {
    FnStmt* original_fn = (FnStmt*)original_fn_ptr;
    // Forward declaration for wyn_emit function
    extern void wyn_emit(const char* fmt, ...);
    extern void codegen_stmt(Stmt* stmt);
    
    if (!original_fn || !type_args || !monomorphic_name) {
        return;
    }
    
    // Determine return type from original function's return type annotation
    const char* return_type = "int"; // Default
    
    if (original_fn->return_type && original_fn->return_type->type == EXPR_IDENT) {
        Token type_name = original_fn->return_type->token;
        if (type_name.length == 3 && memcmp(type_name.start, "int", 3) == 0) {
            return_type = "int";
        } else if (type_name.length == 6 && memcmp(type_name.start, "string", 6) == 0) {
            return_type = "char*";
        } else if (type_name.length == 5 && memcmp(type_name.start, "float", 5) == 0) {
            return_type = "double";
        } else if (type_name.length == 4 && memcmp(type_name.start, "bool", 4) == 0) {
            return_type = "bool";
        }
    }
    
    // Generate function signature
    wyn_emit("// Monomorphic instance of %.*s\n", 
         (int)original_fn->name.length, original_fn->name.start);
    wyn_emit("%s %s(", return_type, monomorphic_name);
    
    // Generate parameters with concrete types
    for (int i = 0; i < original_fn->param_count; i++) {
        if (i > 0) wyn_emit(", ");
        
        // Use type argument for parameter type
        const char* param_type = "int"; // Default
        char param_type_buf[256] = {0};
        
        if (i < type_arg_count && type_args[i]) {
            switch (type_args[i]->kind) {
                case TYPE_INT:
                    param_type = "int";
                    break;
                case TYPE_FLOAT:
                    param_type = "double";
                    break;
                case TYPE_STRING:
                    param_type = "const char*";
                    break;
                case TYPE_BOOL:
                    param_type = "bool";
                    break;
                case TYPE_STRUCT:
                    snprintf(param_type_buf, sizeof(param_type_buf), "%.*s",
                            type_args[i]->struct_type.name.length,
                            type_args[i]->struct_type.name.start);
                    param_type = param_type_buf;
                    break;
                default:
                    param_type = "int";
                    break;
            }
        }
        
        wyn_emit("%s %.*s", param_type, 
             (int)original_fn->params[i].length, original_fn->params[i].start);
    }
    
    wyn_emit(") {\n");
    
    // Generate function body
    if (original_fn->body) {
        codegen_stmt(original_fn->body);
    } else {
        wyn_emit("    return 0;\n");
    }
    
    wyn_emit("}\n\n");
}

// Generate all monomorphic instances for code generation
void wyn_generate_monomorphic_instances(void) {
    GenericInstantiation* current = g_instantiations;
    
    while (current) {
        // Find the original generic function
        GenericFunction* generic_fn = wyn_find_generic_function(current->function_name);
        if (generic_fn) {
            // Generate monomorphic version
            wyn_emit_monomorphic_function(generic_fn, current->type_args, current->type_arg_count, current->monomorphic_name);
        }
        current = current->next;
    }
}

// Generate monomorphic instances for codegen integration
void wyn_generate_monomorphic_instances_for_codegen(void* prog_ptr) {
    Program* prog = (Program*)prog_ptr;
    GenericInstantiation* current = g_instantiations;
    
    // First, generate monomorphized struct definitions
    GenericStructInstantiation* struct_current = g_struct_instantiations;
    while (struct_current) {
        if (struct_current->monomorphic_struct) {
            // Generate the struct definition using codegen_stmt
            extern void codegen_stmt(Stmt* stmt);
            Stmt wrapper_stmt;
            wrapper_stmt.type = STMT_STRUCT;
            wrapper_stmt.struct_decl = *struct_current->monomorphic_struct;
            codegen_stmt(&wrapper_stmt);
        }
        struct_current = struct_current->next;
    }
    
    // Then generate function declarations and definitions as before
    // First pass: Generate forward declarations
    while (current) {
        GenericFunction* generic_fn = wyn_find_generic_function(current->function_name);
        if (generic_fn && generic_fn->original_fn) {
            // Generate forward declaration
            wyn_emit_monomorphic_function_declaration(generic_fn->original_fn, 
                                                      current->type_args, 
                                                      current->type_arg_count, 
                                                      current->monomorphic_name);
        }
        current = current->next;
    }
    
    // Second pass: Generate function definitions
    current = g_instantiations;
    while (current) {
        GenericFunction* generic_fn = wyn_find_generic_function(current->function_name);
        if (generic_fn && generic_fn->original_fn) {
            // Generate monomorphic function definition
            wyn_emit_monomorphic_function_definition(generic_fn->original_fn, 
                                                     current->type_args, 
                                                     current->type_arg_count, 
                                                     current->monomorphic_name);
        }
        current = current->next;
    }
}

// Collect generic instantiations from the entire program
void wyn_collect_generic_instantiations_from_program(void* prog_ptr) {
    Program* prog = (Program*)prog_ptr;
    
    // Walk through all statements and expressions to find generic function calls
    for (int i = 0; i < prog->count; i++) {
        wyn_collect_generic_instantiations_from_stmt(prog->stmts[i]);
    }
}

// Collect generic instantiations from a statement
void wyn_collect_generic_instantiations_from_stmt(void* stmt_ptr) {
    Stmt* stmt = (Stmt*)stmt_ptr;
    if (!stmt) return;
    
    switch (stmt->type) {
        case STMT_EXPR:
            wyn_collect_generic_instantiations_from_expr(stmt->expr);
            break;
        case STMT_VAR:
            if (stmt->var.init) {
                wyn_collect_generic_instantiations_from_expr(stmt->var.init);
            }
            break;
        case STMT_RETURN:
            if (stmt->ret.value) {
                wyn_collect_generic_instantiations_from_expr(stmt->ret.value);
            }
            break;
        case STMT_BLOCK:
            for (int i = 0; i < stmt->block.count; i++) {
                wyn_collect_generic_instantiations_from_stmt(stmt->block.stmts[i]);
            }
            break;
        case STMT_FN:
            if (stmt->fn.body) {
                wyn_collect_generic_instantiations_from_stmt(stmt->fn.body);
            }
            break;
        case STMT_MATCH:
            // Collect from match value expression
            if (stmt->match_stmt.value) {
                wyn_collect_generic_instantiations_from_expr(stmt->match_stmt.value);
            }
            // Collect from each match case body
            for (int i = 0; i < stmt->match_stmt.case_count; i++) {
                if (stmt->match_stmt.cases[i].body) {
                    wyn_collect_generic_instantiations_from_stmt(stmt->match_stmt.cases[i].body);
                }
            }
            break;
        default:
            break;
    }
}

// Collect generic instantiations from an expression
void wyn_collect_generic_instantiations_from_expr(void* expr_ptr) {
    Expr* expr = (Expr*)expr_ptr;
    if (!expr) return;
    
    switch (expr->type) {
        case EXPR_CALL:
            // Check if this is a generic function call
            if (expr->call.callee->type == EXPR_IDENT) {
                Token func_name = expr->call.callee->token;
                
                if (wyn_is_generic_function_call(func_name)) {
                    // Collect argument types for generic instantiation
                    Type** arg_types = malloc(sizeof(Type*) * expr->call.arg_count);
                    for (int i = 0; i < expr->call.arg_count; i++) {
                        // Use expr_type if available (set by checker), otherwise infer from expression
                        if (expr->call.args[i]->expr_type) {
                            arg_types[i] = expr->call.args[i]->expr_type;
                        } else {
                            // Fallback: infer type from expression
                            switch (expr->call.args[i]->type) {
                                case EXPR_INT:
                                    arg_types[i] = make_type(TYPE_INT);
                                    break;
                                case EXPR_FLOAT:
                                    arg_types[i] = make_type(TYPE_FLOAT);
                                    break;
                                case EXPR_STRING:
                                    arg_types[i] = make_type(TYPE_STRING);
                                    break;
                                case EXPR_BOOL:
                                    arg_types[i] = make_type(TYPE_BOOL);
                                    break;
                                default:
                                    arg_types[i] = make_type(TYPE_INT);
                                    break;
                            }
                        }
                    }
                    
                    // Register this instantiation
                    wyn_register_generic_instantiation(func_name, arg_types, expr->call.arg_count);
                    free(arg_types);
                }
            }
            
            // Recursively process arguments
            for (int i = 0; i < expr->call.arg_count; i++) {
                wyn_collect_generic_instantiations_from_expr(expr->call.args[i]);
            }
            break;
        case EXPR_STRUCT_INIT:
            // Process field values
            for (int i = 0; i < expr->struct_init.field_count; i++) {
                wyn_collect_generic_instantiations_from_expr(expr->struct_init.field_values[i]);
            }
            break;
        case EXPR_BINARY:
            wyn_collect_generic_instantiations_from_expr(expr->binary.left);
            wyn_collect_generic_instantiations_from_expr(expr->binary.right);
            break;
        case EXPR_UNARY:
            wyn_collect_generic_instantiations_from_expr(expr->unary.operand);
            break;
        case EXPR_MATCH:
            // Collect from match value expression
            if (expr->match.value) {
                wyn_collect_generic_instantiations_from_expr(expr->match.value);
            }
            // Collect from each match arm result
            for (int i = 0; i < expr->match.arm_count; i++) {
                if (expr->match.arms[i].result) {
                    wyn_collect_generic_instantiations_from_expr(expr->match.arms[i].result);
                }
            }
            break;
        default:
            break;
    }
}

// Get all registered instantiations (for codegen)
GenericInstantiation* wyn_get_instantiations(void) {
    return g_instantiations;
}

// Get generic system statistics
GenericStats wyn_get_generic_stats(void) {
    GenericStats stats;
    stats.generic_functions_registered = g_generic_function_count;
    stats.types_instantiated = g_generic_struct_count;  // T3.1.2: Include generic structs
    stats.monomorphizations_generated = 0; // Placeholder
    stats.generics_enabled = true;
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
