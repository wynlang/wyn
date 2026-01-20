#ifndef WYN_TYPES_H
#define WYN_TYPES_H

#include "common.h"

// Forward declarations
typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct Pattern Pattern;  // T3.3.1: Pattern forward declaration
typedef struct Type Type;  // Forward declare Type for method dispatch

// Phase 1 Task 1.2: Method Signature Table
typedef struct {
    const char* receiver_type;  // "string", "int", "float", etc.
    const char* method_name;    // "upper", "lower", "abs", etc.
    const char* return_type;    // "string", "int", "float", etc.
    int param_count;            // Number of parameters (excluding self)
} MethodSignature;

// Lookup method return type given receiver type and method name
const char* lookup_method_return_type(const char* receiver_type, const char* method_name);

// Get receiver type string from Type for method dispatch
const char* get_receiver_type_string(const Type* type);

// Dispatch method call based on receiver type and method name
typedef struct {
    const char* c_function;  // C function to call (e.g., "string_len")
    bool needs_args;         // Whether to emit arguments
    bool pass_by_ref;        // Whether to pass receiver by reference (&obj)
} MethodDispatch;

bool dispatch_method(const char* receiver_type, const char* method_name, int arg_count, MethodDispatch* out);

// T1.5.2: LambdaExpr definition (moved from ast.h to break circular dependency)
typedef struct LambdaExpr {
    Token* params;
    int param_count;
    Expr* body;
    // T3.4.1: Closure capture support
    Token* captured_vars;     // Variables captured from environment
    int captured_count;       // Number of captured variables
    bool* capture_by_move;    // Whether each capture is by move or reference
} LambdaExpr;

typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_VOID,
    TYPE_ARRAY,
    TYPE_STRUCT,
    TYPE_ENUM,
    TYPE_FUNCTION,
    TYPE_MAP,
    TYPE_SET,
    TYPE_OPTIONAL,  // T2.5.1: Optional Type Implementation
    TYPE_UNION,     // T2.5.2: Union Type Support
    TYPE_RESULT,    // TASK-026: Result<T,E> Type Implementation
    TYPE_GENERIC,   // T3.1.2: Generic type parameter
} TypeKind;

// Type already forward declared above

typedef struct {
    Token name;
    Type** field_types;
    Token* field_names;
    int field_count;
} StructType;

typedef struct {
    Type** param_types;
    int param_count;
    Type* return_type;
    bool is_variadic;  // Support for variadic functions (e.g., printf)
} FunctionType;

typedef struct {
    Type* key_type;
    Type* value_type;
} MapType;

typedef struct {
    Type* inner_type;  // The type that is optional (T in T?)
} OptionalType;

typedef struct {
    Type** types;      // Array of types in the union (T, U, V, ...)
    int type_count;    // Number of types in the union
} UnionType;

typedef struct {
    Type* ok_type;     // The success type (T in Result<T,E>)
    Type* err_type;    // The error type (E in Result<T,E>)
} ResultType;

typedef struct {
    Token* variants;   // Array of enum variant names
    int variant_count; // Number of variants
} EnumType;

struct Type {
    TypeKind kind;
    Token name;
    union {
        StructType struct_type;
        FunctionType fn_type;
        MapType map_type;
        OptionalType optional_type;  // T2.5.1: Optional Type Implementation
        UnionType union_type;        // T2.5.2: Union Type Support
        ResultType result_type;      // TASK-026: Result<T,E> Type Implementation
        EnumType enum_type;          // Enum type with variants
    };
};

typedef struct Symbol {
    Token name;
    Type* type;
    bool is_mutable;
    // T1.5.3: Function overloading support
    struct Symbol* next_overload;  // Linked list of overloaded functions
    char* mangled_name;           // Mangled name for code generation
} Symbol;

typedef struct SymbolTable {
    Symbol* symbols;
    int count;
    int capacity;
    struct SymbolTable* parent;
} SymbolTable;

// Symbol table operations
void add_symbol(SymbolTable* scope, Token name, Type* type, bool is_mutable);
Symbol* find_symbol(SymbolTable* scope, Token name);
SymbolTable* get_global_scope(void);

// T2.5.4: Type Inference Improvements
typedef struct {
    size_t variables_inferred;
    size_t functions_inferred;
    size_t generics_inferred;
    size_t total_inferences;
    double inference_success_rate;
} TypeInferenceStats;

// Type inference functions
void wyn_type_inference_init(void);
Type* wyn_infer_variable_type(Expr* init_expr, SymbolTable* scope);
Type* wyn_infer_function_return_type(Stmt* function_body, SymbolTable* scope);
Type* wyn_analyze_return_statements(Stmt* stmt, SymbolTable* scope);
Type* wyn_infer_binary_result_type(Expr* binary_expr);
Type* wyn_infer_call_return_type(Expr* call_expr, SymbolTable* scope);
TypeInferenceStats wyn_type_inference_get_stats(void);
void wyn_type_inference_print_stats(void);
void wyn_type_inference_reset(void);
void wyn_type_inference_set_enabled(bool enabled);

// T3.1.1: Generic Functions (Simplified)
typedef struct GenericFunction GenericFunction;
typedef struct GenericStruct GenericStruct;  // T3.1.2: Generic struct forward declaration
typedef struct TypeConstraint TypeConstraint;  // T3.1.3: Type constraint forward declaration
typedef struct {
    size_t generic_functions_registered;
    size_t types_instantiated;  // T3.1.2: Now includes generic structs
    size_t monomorphizations_generated;
    bool generics_enabled;
} GenericStats;

// Generic function system
void wyn_generics_init(void);
void wyn_register_generic_function(void* fn);
GenericFunction* wyn_find_generic_function(Token name);
Type* wyn_instantiate_generic_function(const char* name, Type** type_args, int arg_count);
bool wyn_is_generic_function_call(Token function_name);
Type* wyn_infer_generic_call_type(Token function_name, Expr** args, int arg_count);
void wyn_generate_monomorphic_name(Token base_name, Type** type_args, int type_arg_count, 
                                   char* buffer, size_t buffer_size);
bool wyn_is_type_parameter(GenericFunction* generic_fn, Token type_name);
void wyn_register_generic_instantiation(Token func_name, Type** type_args, int type_arg_count);
void wyn_collect_generic_instantiations_from_program(void* prog_ptr);
void wyn_collect_generic_instantiations_from_stmt(void* stmt_ptr);
void wyn_collect_generic_instantiations_from_expr(void* expr_ptr);
void wyn_generate_monomorphic_instances_for_codegen(void* prog_ptr);
void wyn_emit_monomorphic_function_declaration(void* original_fn, Type** type_args, int type_arg_count, const char* monomorphic_name);
void wyn_emit_monomorphic_function_definition(void* original_fn, Type** type_args, int type_arg_count, const char* monomorphic_name);
void wyn_emit(const char* fmt, ...); // Global emit function for code generation
GenericStats wyn_get_generic_stats(void);
void wyn_print_generic_stats(void);
void wyn_list_generic_functions(void);
void wyn_cleanup_generics(void);

// T3.1.2: Generic struct system
void wyn_register_generic_struct(void* struct_ptr);
GenericStruct* wyn_find_generic_struct(Token name);
bool wyn_is_generic_struct(Token struct_name);
void wyn_generate_monomorphic_struct_name(Token base_name, Type** type_args, int type_arg_count, 
                                          char* buffer, size_t buffer_size);
void* wyn_monomorphize_struct(GenericStruct* generic_struct, Type** type_args, int type_arg_count);
void wyn_list_generic_structs(void);

// T3.1.3: Type constraints and bounds
TypeConstraint* wyn_create_constraint(Token trait_name);
TypeConstraint* wyn_add_constraint(TypeConstraint* constraints, Token trait_name);
bool wyn_check_constraints(Type* concrete_type, TypeConstraint* constraints);
TypeConstraint** wyn_parse_where_constraints(Token* type_params, int type_param_count);
void wyn_free_constraints(TypeConstraint* constraints);
bool wyn_validate_generic_call(GenericFunction* generic_fn, Type** type_args, int type_arg_count);
bool wyn_validate_generic_struct(GenericStruct* generic_struct, Type** type_args, int type_arg_count);

// T3.2.2: Trait bounds and constraints
bool wyn_has_trait_bound(TypeConstraint* constraints, Token trait_name);
void wyn_get_trait_bounds_string(TypeConstraint* constraints, char* buffer, size_t buffer_size);
bool wyn_validate_trait_bounds(GenericFunction* generic_fn, Type** type_args, int type_arg_count);

// T3.2.1: Trait system
void wyn_traits_init(void);
void wyn_register_trait(void* trait_ptr);
void wyn_register_trait_impl(Token trait_name, Token type_name, void** methods, int method_count);
bool wyn_type_implements_trait(Token type_name, Token trait_name);
void wyn_print_trait_stats(void);
void wyn_list_traits(void);
void wyn_cleanup_traits(void);

// T3.2.3: Standard traits
void wyn_register_standard_traits(void);
void wyn_implement_standard_traits_for_basic_types(void);
bool wyn_is_standard_trait(Token trait_name);
const char* wyn_get_standard_trait_method(Token trait_name);
bool wyn_can_derive_trait(Token type_name, Token trait_name);

// T3.3.1: Pattern matching and destructuring
void wyn_patterns_init(void);
Pattern* wyn_create_literal_pattern(Token value);
Pattern* wyn_create_ident_pattern(Token name);
Pattern* wyn_create_wildcard_pattern(void);
Pattern* wyn_create_struct_pattern(Token struct_name, Token* field_names, Pattern** field_patterns, int field_count);
Pattern* wyn_create_array_pattern(Pattern** elements, int element_count, bool has_rest, Token rest_name);
Pattern* wyn_create_tuple_pattern(Pattern** elements, int element_count);
Pattern* wyn_create_range_pattern(Expr* start, Expr* end, bool inclusive);
Pattern* wyn_create_option_pattern(Pattern* inner, bool is_some);
Pattern* wyn_create_guard_pattern(Pattern* base_pattern, Expr* guard);
bool wyn_pattern_matches(Pattern* pattern, Expr* value, SymbolTable* scope);
void wyn_extract_pattern_bindings(Pattern* pattern, Expr* value, SymbolTable* scope);
bool wyn_is_pattern_exhaustive(Pattern** patterns, int pattern_count, Type* match_type);
void wyn_free_pattern(Pattern* pattern);
void wyn_print_pattern_stats(void);
void wyn_reset_pattern_stats(void);
void wyn_cleanup_patterns(void);

// T3.3.2: Pattern matching in let bindings
bool wyn_is_pattern_irrefutable(Pattern* pattern);
bool wyn_process_let_binding(Pattern* pattern, Expr* init_expr, SymbolTable* scope);
bool wyn_validate_function_parameter_patterns(Pattern** param_patterns, int param_count);
void wyn_check_let_pattern_completeness(Pattern* pattern, Type* value_type);
void wyn_extract_pattern_variables(Pattern* pattern, Token** variables, int* var_count, int max_vars);

// T3.4.1: Closures and lambda functions
void wyn_closures_init(void);
LambdaExpr* wyn_create_lambda(Token* params, int param_count, Expr* body, SymbolTable* scope);
void wyn_analyze_lambda_captures(LambdaExpr* lambda, Expr* body, SymbolTable* scope);
Type* wyn_create_closure_type(LambdaExpr* lambda, SymbolTable* scope);
bool wyn_validate_lambda(LambdaExpr* lambda, SymbolTable* scope);
Type* wyn_apply_closure(LambdaExpr* lambda, Expr** args, int arg_count, SymbolTable* scope);
bool wyn_implements_fn_trait(Type* closure_type, Token trait_name);
void wyn_generate_closure_name(LambdaExpr* lambda, char* buffer, size_t buffer_size);
void wyn_free_lambda(LambdaExpr* lambda);
void wyn_print_closure_stats(void);
void wyn_reset_closure_stats(void);
void wyn_cleanup_closures(void);

// T3.4.2: Closure types and traits
void wyn_determine_closure_traits(LambdaExpr* lambda, bool* implements_fn, bool* implements_fn_mut, bool* implements_fn_once);
void wyn_implement_closure_traits(LambdaExpr* lambda);
bool wyn_is_closure_function_pointer_compatible(LambdaExpr* lambda);
void* wyn_closure_to_function_pointer(LambdaExpr* lambda);
Type* wyn_create_closure_type_with_bounds(LambdaExpr* lambda, Token trait_bound);
Type* wyn_apply_higher_order_function(Token fn_name, LambdaExpr* closure, Expr** args, int arg_count, SymbolTable* scope);
bool wyn_closure_implements_fn_trait(LambdaExpr* lambda, Token trait_name);

#endif
