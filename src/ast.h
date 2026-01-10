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
    EXPR_MATCH,
    EXPR_TERNARY,
    EXPR_SOME,
    EXPR_NONE,
    EXPR_OK,
    EXPR_ERR,
    EXPR_PIPELINE,
    EXPR_IF_EXPR,
    EXPR_STRING_INTERP,
    EXPR_RANGE,
    EXPR_LAMBDA,
    EXPR_DESTRUCTURE,
    EXPR_SPREAD,
    EXPR_TRY,
    EXPR_MAP,
    EXPR_TUPLE,
    EXPR_INDEX_ASSIGN,
    EXPR_OPTIONAL_TYPE,  // T2.5.1: Optional Type Implementation
    EXPR_UNION_TYPE,     // T2.5.2: Union Type Support
    EXPR_PATTERN,        // T3.3.1: Pattern expressions for destructuring
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
} StructInitExpr;

typedef struct {
    Expr* object;
    Token field;
} FieldAccessExpr;

typedef struct {
    Token op;
    Expr* operand;
} UnaryExpr;

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
    Token pattern;
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
    Expr* object;
    Expr* index;
    Expr* value;
} IndexAssignExpr;

typedef struct {
    Expr* inner_type;  // The type that is optional (T in T?)
} OptionalTypeExpr;

typedef struct {
    Expr** types;      // Array of types in the union (T, U, V, ...)
    int type_count;    // Number of types in the union
} UnionTypeExpr;

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
        IndexAssignExpr index_assign;
        OptionalTypeExpr optional_type;  // T2.5.1: Optional Type Implementation
        UnionTypeExpr union_type;        // T2.5.2: Union Type Support
    };
};

typedef enum {
    STMT_EXPR,
    STMT_VAR,
    STMT_RETURN,
    STMT_BLOCK,
    STMT_FN,
    STMT_STRUCT,
    STMT_IMPL,
    STMT_IF,
    STMT_WHILE,
    STMT_FOR,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_ENUM,
    STMT_TYPE_ALIAS,
    STMT_IMPORT,
    STMT_EXPORT,
    STMT_ASYNC_FN,
    STMT_TRY,
    STMT_THROW,
    STMT_TEST,  // T1.6.2: Testing Framework Agent addition
    STMT_MATCH, // T1.4.3: Control Flow Agent addition
    STMT_TRAIT, // T3.2.1: Trait definition statement
    STMT_MODULE, // T3.5.1: Module declaration statement
} StmtType;

typedef struct Stmt Stmt;

typedef struct {
    Token name;          // Original single variable name (for backward compatibility)
    Pattern* pattern;    // T3.3.2: Pattern for destructuring let bindings
    Expr* type;
    Expr* init;
    bool is_const;
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
} FnStmt;

typedef struct {
    Token name;
    Token* type_params;       // T3.1.2: Generic type parameters
    int type_param_count;     // T3.1.2: Number of type parameters
    Token* fields;
    Expr** field_types;
    bool* field_arc_managed;  // T2.5.3: ARC integration for struct fields
    int field_count;
} StructStmt;

typedef struct {
    Token type_name;
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
} EnumStmt;

typedef struct {
    Token name;
    Token target;
} TypeAliasStmt;

typedef struct {
    Token module;
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
    Token exception_var;  // Variable to bind caught exception
    Stmt* catch_block;
} TryStmt;

typedef struct {
    Expr* value;  // Expression to throw
} ThrowStmt;

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
        ReturnStmt ret;
        BlockStmt block;
        FnStmt fn;
        StructStmt struct_decl;
        ImplStmt impl;
        TraitStmt trait_decl;      // T3.2.1: Trait definition statement
        ModuleStmt module_decl;    // T3.5.1: Module declaration statement
        IfStmt if_stmt;
        WhileStmt while_stmt;
        ForStmt for_stmt;
        EnumStmt enum_decl;
        TypeAliasStmt type_alias;
        ImportStmt import;
        ExportStmt export;
        TryStmt try_stmt;
        ThrowStmt throw_stmt;
        BreakStmt break_stmt;      // T1.4.2: Control Flow Agent addition
        ContinueStmt continue_stmt; // T1.4.2: Control Flow Agent addition
        MatchStmt match_stmt;      // T1.4.3: Control Flow Agent addition
        struct {  // T1.6.2: Test statement structure
            Token name;
            Stmt* body;
            bool is_async;
        } test_stmt;
    };
};

typedef struct {
    Stmt** stmts;
    int count;
} Program;

#endif
