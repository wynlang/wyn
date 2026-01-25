#ifndef WYN_AST_H
#define WYN_AST_H

#include "common.h"
#include "types.h"

// Forward declarations
typedef struct Expr Expr;
typedef struct Stmt Stmt;

typedef enum {
    EXPR_INT,
    EXPR_FLOAT,
    EXPR_STRING,
    EXPR_CHAR,
    EXPR_IDENT,
    EXPR_BINARY,
    EXPR_CALL,
    EXPR_METHOD_CALL,
    EXPR_ARRAY,
    EXPR_INDEX,
    EXPR_ASSIGN,
    EXPR_STRUCT_INIT,
    EXPR_FIELD_ACCESS,
    EXPR_BOOL,
    EXPR_UNARY,
    EXPR_AWAIT,
    EXPR_MATCH,
    EXPR_TERNARY,
    EXPR_SOME,
    EXPR_NONE,
    EXPR_OK,
    EXPR_ERR,
    EXPR_TRY,           // TASK-026: ? operator for error propagation
    EXPR_PIPELINE,
    EXPR_IF_EXPR,
    EXPR_STRING_INTERP,
    EXPR_RANGE,
    EXPR_LAMBDA,
    EXPR_DESTRUCTURE,
    EXPR_SPREAD,
    EXPR_MAP,
    EXPR_HASHMAP_LITERAL,  // v1.2.3: {} for HashMap
    EXPR_HASHSET_LITERAL,  // v1.2.3: () for HashSet
    EXPR_TUPLE,
    EXPR_TUPLE_INDEX,   // Tuple element access (tuple.0, tuple.1, etc.)
    EXPR_INDEX_ASSIGN,
    EXPR_FIELD_ASSIGN,  // Field assignment (obj.field = value)
    EXPR_OPTIONAL_TYPE,  // T2.5.1: Optional Type Implementation
    EXPR_UNION_TYPE,     // T2.5.2: Union Type Support
    EXPR_RESULT_TYPE,    // TASK-026: Result<T,E> Type Implementation
    EXPR_PATTERN,        // T3.3.1: Pattern expressions for destructuring
    EXPR_FN_TYPE,        // Function type: fn(T1, T2) -> R
    EXPR_BLOCK,          // Block expression: { stmt1; stmt2; expr }
} ExprType;

// T3.3.1: Pattern types for destructuring
typedef enum {
    PATTERN_LITERAL,     // Literal values (42, "hello", true)
    PATTERN_IDENT,       // Variable binding (x, name)
    PATTERN_WILDCARD,    // Wildcard pattern (_)
    PATTERN_STRUCT,      // Struct destructuring (Point{x, y})
    PATTERN_ARRAY,       // Array destructuring ([first, second, ...rest])
    PATTERN_TUPLE,       // Tuple destructuring ((x, y, z))
    PATTERN_RANGE,       // Range patterns (1..10, 'a'..'z')
    PATTERN_OPTION,      // Option patterns (Some(x), None)
    PATTERN_GUARD,       // Pattern with guard clause (x if x > 0)
} PatternType;

typedef struct {
    Expr* left;
    Token op;
    Expr* right;
} BinaryExpr;

typedef struct {
    Expr* callee;
    Expr** args;
    int arg_count;
    void* selected_overload;  // T1.5.3: Selected function overload (void* to avoid circular dependency)
} CallExpr;

typedef struct {
    Expr* object;
    Token method;
    Expr** args;
    int arg_count;
} MethodCallExpr;

typedef struct {
    Expr** elements;
    int count;
} ArrayExpr;

typedef struct {
    Expr* array;
    Expr* index;
} IndexExpr;

typedef struct {
    Token name;
    Expr* value;
} AssignExpr;

typedef struct {
    Token type_name;
    Token* field_names;
    Expr** field_values;
    int field_count;
    char* monomorphic_name;  // T4.1: For generic struct instantiations
} StructInitExpr;

typedef struct {
    Expr* object;
    Token field;
    bool is_enum_access;  // True if this is enum member access (EnumName.MEMBER)
} FieldAccessExpr;

typedef struct {
    Token op;
    Expr* operand;
} UnaryExpr;

typedef struct {
    Expr* expr;
} AwaitExpr;

// T3.3.1: Pattern structures for destructuring
typedef struct Pattern Pattern;

typedef struct {
    Token value;  // Literal value token
} LiteralPattern;

typedef struct {
    Token name;   // Variable name to bind
} IdentPattern;

typedef struct {
    Token struct_name;     // Name of the struct type
    Token* field_names;    // Field names to destructure
    Pattern** field_patterns; // Patterns for each field
    int field_count;       // Number of fields
} StructPattern;

typedef struct {
    Pattern** elements;    // Array element patterns
    int element_count;     // Number of elements
    bool has_rest;         // Whether there's a ...rest pattern
    Token rest_name;       // Name for rest elements (if has_rest)
} ArrayPattern;

typedef struct {
    Pattern** elements;    // Tuple element patterns
    int element_count;     // Number of elements
} TuplePattern;

typedef struct {
    Expr* start;          // Range start expression
    Expr* end;            // Range end expression
    bool inclusive;       // Whether range is inclusive
} RangePattern;

typedef struct {
    Pattern* inner;       // Pattern inside Some() or None
    bool is_some;         // true for Some(pattern), false for None
    Token variant_name;   // Variant name (Some, None, Ok, Err, etc.)
    Token enum_name;      // Enum type name (Result, Option, etc.)
} OptionPattern;

typedef struct {
    Pattern* pattern;     // Base pattern
    Expr* guard;          // Guard expression
} GuardPattern;

struct Pattern {
    PatternType type;
    union {
        LiteralPattern literal;
        IdentPattern ident;
        StructPattern struct_pat;
        ArrayPattern array;
        TuplePattern tuple;
        RangePattern range;
        OptionPattern option;
        GuardPattern guard;
    };
};

typedef struct {
    Pattern* pattern;
    Expr* result;
} MatchArm;

typedef struct {
    Expr* value;
    MatchArm* arms;
    int arm_count;
} MatchExpr;

typedef struct {
    Expr* value;
} OptionExpr;

typedef struct {
    Expr* condition;
    Expr* then_expr;
    Expr* else_expr;
} TernaryExpr;

typedef struct {
    Expr** stages;
    int stage_count;
} PipelineExpr;

typedef struct {
    Expr* condition;
    Expr* then_expr;
    Expr* else_expr;
} IfExpr;

typedef struct {
    char** parts;
    Expr** expressions;
    int count;
} StringInterpExpr;

typedef struct {
    Expr* start;
    Expr* end;
    bool inclusive;
} RangeExpr;

typedef struct {
    Token* params;
    int param_count;
    Expr* body;
    // T3.4.1: Closure capture support - moved to types.h
} LambdaExpr_MOVED_TO_TYPES_H;

typedef struct {
    Expr** keys;
    Expr** values;
    int count;
} MapExpr;

typedef struct {
    Expr** elements;
    int count;
} TupleExpr;

typedef struct {
    Expr* tuple;
    int index;
} TupleIndexExpr;

typedef struct {
    Expr* object;
    Expr* index;
    Expr* value;
} IndexAssignExpr;

typedef struct {
    Expr* object;
    Token field;
    Expr* value;
} FieldAssignExpr;

typedef struct {
    Expr* inner_type;  // The type that is optional (T in T?)
} OptionalTypeExpr;

typedef struct {
    Expr** types;      // Array of types in the union (T, U, V, ...)
    int type_count;    // Number of types in the union
} UnionTypeExpr;

typedef struct {
    Expr* ok_type;     // The success type (T in Result<T,E>)
    Expr* err_type;    // The error type (E in Result<T,E>)
} ResultTypeExpr;

typedef struct {
    Expr** param_types;  // Parameter types
    int param_count;     // Number of parameters
    Expr* return_type;   // Return type
} FnTypeExpr;

typedef struct {
    Expr* value;       // Expression that might fail (for ? operator)
} TryExpr;

typedef struct {
    Stmt** stmts;      // Statements in the block
    int stmt_count;    // Number of statements
    Expr* result;      // Final expression (result of the block)
} BlockExpr;

struct Expr {
    ExprType type;
    Token token;
    struct Type* expr_type;  // Type information populated by checker
    union {
        BinaryExpr binary;
        CallExpr call;
        MethodCallExpr method_call;
        ArrayExpr array;
        IndexExpr index;
        AssignExpr assign;
        StructInitExpr struct_init;
        FieldAccessExpr field_access;
        UnaryExpr unary;
        AwaitExpr await;
        MatchExpr match;
        OptionExpr option;
        TernaryExpr ternary;
        PipelineExpr pipeline;
        IfExpr if_expr;
        StringInterpExpr string_interp;
        RangeExpr range;
        LambdaExpr lambda;
        MapExpr map;
        TupleExpr tuple;
        TupleIndexExpr tuple_index;
        IndexAssignExpr index_assign;
        FieldAssignExpr field_assign;
        OptionalTypeExpr optional_type;  // T2.5.1: Optional Type Implementation
        UnionTypeExpr union_type;        // T2.5.2: Union Type Support
        ResultTypeExpr result_type;      // TASK-026: Result<T,E> Type Implementation
        TryExpr try_expr;                // TASK-026: ? operator for error propagation
        FnTypeExpr fn_type;              // Function type: fn(T1, T2) -> R
        BlockExpr block;                 // Block expression: { stmt1; stmt2; expr }
    };
};

typedef enum {
    STMT_EXPR,
    STMT_VAR,
    STMT_CONST,
    STMT_RETURN,
    STMT_BLOCK,
    STMT_UNSAFE,
    STMT_FN,
    STMT_EXTERN,
    STMT_STRUCT,
    STMT_IMPL,
    STMT_IF,
    STMT_WHILE,
    STMT_FOR,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_ENUM,
    STMT_TYPE_ALIAS,
    STMT_MACRO,
    STMT_IMPORT,
    STMT_EXPORT,
    STMT_ASYNC_FN,
    STMT_TRY,
    STMT_CATCH,      // TASK-026: Catch block for error handling
    STMT_THROW,
    STMT_TEST,  // T1.6.2: Testing Framework Agent addition
    STMT_MATCH, // T1.4.3: Control Flow Agent addition
    STMT_TRAIT, // T3.2.1: Trait definition statement
    STMT_MODULE, // T3.5.1: Module declaration statement
    STMT_SPAWN, // Concurrency: spawn statement
} StmtType;

typedef struct Stmt Stmt;

typedef struct {
    Token name;          // Original single variable name (for backward compatibility)
    Pattern* pattern;    // T3.3.2: Pattern for destructuring let bindings
    Expr* type;
    Expr* init;
    bool is_const;
    bool is_mutable;     // Whether variable is declared with 'mut'
    bool uses_pattern;   // T3.3.2: Whether this uses pattern matching
} VarStmt;

typedef struct {
    Expr* value;
} ReturnStmt;

typedef struct {
    Stmt** stmts;
    int count;
} BlockStmt;

typedef struct {
    Token name;
    Token* params;
    Expr** param_types;
    bool* param_mutable;
    Expr** param_defaults;    // T1.5.2: Default parameter values (NULL if no default)
    int param_count;
    Token* type_params;
    int type_param_count;
    Expr* return_type;
    Stmt* body;
    bool is_public;
    bool is_async;
    Token receiver_type;      // For extension methods: fn Type.method()
    bool is_extension;        // True if this is an extension method
} FnStmt;

typedef struct {
    Token name;
    Token* params;
    Expr** param_types;
    int param_count;
    Expr* return_type;
    bool is_variadic;  // For functions like printf(format, ...)
} ExternStmt;

typedef struct {
    Token name;
    Token* type_params;       // T3.1.2: Generic type parameters
    int type_param_count;     // T3.1.2: Number of type parameters
    Token* fields;
    Expr** field_types;
    bool* field_arc_managed;  // T2.5.3: ARC integration for struct fields
    int field_count;
    FnStmt** methods;         // Methods defined inside struct
    int method_count;         // Number of methods
    bool is_public;           // Whether struct is public (for modules)
} StructStmt;

typedef struct {
    Token type_name;
    Token trait_name;         // Optional: for "impl Trait for Type"
    bool is_trait_impl;       // True if this is "impl Trait for Type"
    Token* type_params;       // Generic impl parameters
    int type_param_count;
    Token* trait_bounds;      // Trait bounds for type parameters
    int trait_bound_count;
    FnStmt** methods;
    int method_count;
} ImplStmt;

// T3.2.1: Trait definition statement
typedef struct {
    Token name;
    Token* type_params;       // Generic trait parameters
    int type_param_count;
    FnStmt** methods;         // Trait method signatures
    int method_count;
    bool* method_has_default; // Which methods have default implementations
} TraitStmt;

typedef struct {
    Expr* condition;
    Stmt* then_branch;
    Stmt* else_branch;
} IfStmt;

typedef struct {
    Expr* condition;
    Stmt* body;
} WhileStmt;

typedef struct {
    Stmt* init;
    Expr* condition;
    Expr* increment;
    Stmt* body;
    // For array iteration: for item in array
    Expr* array_expr;  // The array being iterated
    Token loop_var;    // The loop variable name
} ForStmt;

typedef struct {
    Token name;
    Token* variants;
    int variant_count;
    bool is_public;
    // For enum variants with data
    Expr*** variant_types;      // Array of arrays of type expressions
    int* variant_type_counts;   // Count of types for each variant
    // For generic enums: enum Result<T, E>
    Token* type_params;
    int type_param_count;
} EnumStmt;

typedef struct {
    Token name;
    Token target;
} TypeAliasStmt;

typedef struct {
    Token name;
    Token* params;
    int param_count;
    Token body;  // Store as token for simple text substitution
} MacroStmt;

typedef struct {
    Token module;
    Token path;      // Optional path like "wyn:math"
    Token alias;     // Optional alias for "import math as m"
    Token* items;
    int item_count;
} ImportStmt;

typedef struct {
    Stmt* stmt;  // The statement being exported (fn, var, struct, etc.)
} ExportStmt;

typedef struct {
    Token name;
    Stmt** body;
    int body_count;
} ModuleStmt;

typedef struct {
    Stmt* try_block;
    Token* exception_types;  // Types of exceptions to catch
    Token* exception_vars;   // Variables to bind caught exceptions
    Stmt** catch_blocks;     // Multiple catch blocks
    int catch_count;         // Number of catch blocks
    Stmt* finally_block;     // Optional finally block
} TryStmt;

typedef struct {
    Expr* value;  // Expression to throw
} ThrowStmt;

typedef struct {
    Token exception_type;  // Type of exception to catch
    Token exception_var;   // Variable to bind caught exception
    Stmt* body;           // Catch block body
} CatchStmt;

// T1.4.2: Break/Continue Implementation - Control Flow Agent addition
typedef struct {
    // Break statements don't need additional data
} BreakStmt;

typedef struct {
    // Continue statements don't need additional data
} ContinueStmt;

// T3.3.1: Enhanced Match Statement with destructuring patterns
typedef struct {
    Pattern* pattern;   // Destructuring pattern (replaces simple token)
    Expr* guard;        // Optional guard clause
    Stmt* body;         // Statement to execute for this pattern
} MatchCase;

typedef struct {
    Expr* value;        // Expression to match against
    MatchCase* cases;   // Array of match cases
    int case_count;     // Number of cases
} MatchStmt;

struct Stmt {
    StmtType type;
    union {
        Expr* expr;
        VarStmt var;
        VarStmt const_stmt;  // Reuse VarStmt structure for constants
        ReturnStmt ret;
        BlockStmt block;
        FnStmt fn;
        ExternStmt extern_fn;
        StructStmt struct_decl;
        ImplStmt impl;
        TraitStmt trait_decl;      // T3.2.1: Trait definition statement
        ModuleStmt module_decl;    // T3.5.1: Module declaration statement
        IfStmt if_stmt;
        WhileStmt while_stmt;
        ForStmt for_stmt;
        EnumStmt enum_decl;
        TypeAliasStmt type_alias;
        MacroStmt macro;
        ImportStmt import;
        ExportStmt export;
        TryStmt try_stmt;
        ThrowStmt throw_stmt;
        CatchStmt catch_stmt;        // TASK-026: Catch statement
        BreakStmt break_stmt;      // T1.4.2: Control Flow Agent addition
        ContinueStmt continue_stmt; // T1.4.2: Control Flow Agent addition
        MatchStmt match_stmt;      // T1.4.3: Control Flow Agent addition
        struct {  // T1.6.2: Test statement structure
            Token name;
            Stmt* body;
            bool is_async;
        } test_stmt;
        struct {  // Spawn statement
            Expr* call;
        } spawn;
    };
};

typedef struct {
    Stmt** stmts;
    int count;
} Program;

#endif
