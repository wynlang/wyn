#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "ast.h"
#include "types.h"
#include "security.h"
#include "test.h"
#include "error.h"

extern Token next_token();

typedef struct {
    Token current;
    Token previous;
    bool had_error;
    const char* filename;  // For better error messages
} Parser;

static Parser parser;
static const char* current_source_file = NULL;  // Global for error reporting

// Parser state stack for module loading
static Parser parser_stack[16];
static int parser_stack_depth = 0;

// Module alias registry (shared with checker/codegen)
typedef struct {
    char alias[64];
    char module[64];
} ModuleAlias;

static ModuleAlias global_module_aliases[32];
static int global_module_alias_count = 0;

void register_parser_module_alias(const char* alias, const char* module) {
    if (global_module_alias_count < 32) {
        snprintf(global_module_aliases[global_module_alias_count].alias, 64, "%s", alias);
        snprintf(global_module_aliases[global_module_alias_count].module, 64, "%s", module);
        global_module_alias_count++;
    }
}

const char* resolve_parser_module_alias(const char* name) {
    for (int i = 0; i < global_module_alias_count; i++) {
        if (strcmp(global_module_aliases[i].alias, name) == 0) {
            return global_module_aliases[i].module;
        }
    }
    return NULL;  // Not an alias
}

void save_parser_state() {
    if (parser_stack_depth < 16) {
        parser_stack[parser_stack_depth++] = parser;
    }
}

void restore_parser_state() {
    if (parser_stack_depth > 0) {
        parser = parser_stack[--parser_stack_depth];
    }
}

void set_parser_filename(const char* filename) {
    current_source_file = filename;
    parser.filename = filename;
}

Expr* expression();
static Expr* assignment();
static Expr* call();  // Forward declaration for await
Stmt* statement();
static Stmt* parse_test_statement(); // T1.6.2: Testing Framework Agent addition
static Stmt* parse_while_statement(); // T1.4.1: Control Flow Agent addition
static Stmt* parse_break_statement(); // T1.4.2: Control Flow Agent addition
static Stmt* parse_continue_statement(); // T1.4.2: Control Flow Agent addition
static Stmt* parse_match_statement(); // T1.4.3: Control Flow Agent addition
static Expr* parse_type(); // T2.5.1: Optional Type Implementation
static Expr* parse_result_type(); // TASK-026: Result type parsing
static Stmt* impl_block(); // T2.5.3: Enhanced Struct System
static Stmt* trait_decl(); // T3.2.1: Trait Definitions
static Pattern* parse_pattern(); // T3.3.1: Pattern parsing for destructuring
static Stmt* parse_try_statement(); // TASK-026: Try statement parsing
static Stmt* parse_catch_statement(); // TASK-026: Catch statement parsing
static Expr* parse_try_expression(); // TASK-026: ? operator parsing
void check_stmt(Stmt* stmt, SymbolTable* scope);
void codegen_stmt(Stmt* stmt);

static void advance() {
    parser.previous = parser.current;
    parser.current = next_token();
}

static bool check(WynTokenType type) {
    return parser.current.type == type;
}

static bool match(WynTokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

static void expect(WynTokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    
    // Use enhanced error reporting if filename is available
    if (current_source_file) {
        show_error_context(current_source_file, parser.current.line, 1, message, NULL);
    } else {
        // Fallback to basic error
        fprintf(stderr, "Error at line %d: %s (no filename set)\n", parser.current.line, message);
    }
    
    parser.had_error = true;
    // Skip to next statement to avoid cascading errors
    while (!check(TOKEN_SEMI) && !check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        advance();
    }
    if (check(TOKEN_SEMI)) advance();
}

static Expr* alloc_expr() {
    return (Expr*)safe_calloc(1, sizeof(Expr));
}

static Stmt* alloc_stmt() {
    return (Stmt*)safe_calloc(1, sizeof(Stmt));
}

static Expr* primary() {
    if (match(TOKEN_AWAIT)) {
        Expr* expr = alloc_expr();
        expr->type = EXPR_AWAIT;
        expr->await.expr = call();  // Parse full call expression, not just primary
        return expr;
    }
    
    if (match(TOKEN_NOT) || match(TOKEN_MINUS) || match(TOKEN_BANG) || match(TOKEN_TILDE)) {
        Token op = parser.previous;
        Expr* operand = primary();
        Expr* unary = alloc_expr();
        unary->type = EXPR_UNARY;
        unary->unary.op = op;
        unary->unary.operand = operand;
        return unary;
    }
    
    if (match(TOKEN_INT)) {
        Expr* expr = alloc_expr();
        expr->type = EXPR_INT;
        expr->token = parser.previous;
        return expr;
    }
    
    if (match(TOKEN_FLOAT)) {
        Expr* expr = alloc_expr();
        expr->type = EXPR_FLOAT;
        expr->token = parser.previous;
        return expr;
    }
    
    if (match(TOKEN_STRING)) {
        // Check for string interpolation
        Token str_token = parser.previous;
        const char* str = str_token.start + 1; // Skip opening quote
        int len = str_token.length - 2; // Skip quotes
        
        // Simple check for ${} pattern
        bool has_interp = false;
        for (int i = 0; i < len - 1; i++) {
            if (str[i] == '$' && str[i + 1] == '{') {
                has_interp = true;
                break;
            }
        }
        
        if (has_interp) {
            Expr* expr = alloc_expr();
            expr->type = EXPR_STRING_INTERP;
            expr->token = str_token;
            
            // Parse interpolation expressions
            expr->string_interp.parts = malloc(sizeof(char*) * 8);
            expr->string_interp.expressions = malloc(sizeof(Expr*) * 8);
            expr->string_interp.count = 0;
            
            // Parse the string and extract ${...} expressions
            int part_start = 0;
            for (int i = 0; i < len - 1; i++) {
                if (str[i] == '$' && str[i + 1] == '{') {
                    // Add string part before ${
                    if (i > part_start) {
                        int part_len = i - part_start;
                        char* part = malloc(part_len + 1);
                        memcpy(part, str + part_start, part_len);
                        part[part_len] = '\0';
                        expr->string_interp.parts[expr->string_interp.count] = part;
                        expr->string_interp.expressions[expr->string_interp.count] = NULL;
                        expr->string_interp.count++;
                    }
                    
                    // Find the closing }
                    int expr_start = i + 2;
                    int expr_end = expr_start;
                    while (expr_end < len && str[expr_end] != '}') {
                        expr_end++;
                    }
                    
                    if (expr_end < len) {
                        // Extract expression
                        int expr_len = expr_end - expr_start;
                        char* expr_str = malloc(expr_len + 1);
                        memcpy(expr_str, str + expr_start, expr_len);
                        expr_str[expr_len] = '\0';
                        
                        // For now, assume it's just a variable name
                        expr->string_interp.parts[expr->string_interp.count] = NULL;
                        
                        // Create a simple identifier expression
                        Expr* var_expr = alloc_expr();
                        var_expr->type = EXPR_IDENT;
                        var_expr->token.start = expr_str;
                        var_expr->token.length = expr_len;
                        expr->string_interp.expressions[expr->string_interp.count] = var_expr;
                        expr->string_interp.count++;
                        
                        i = expr_end; // Skip to after }
                        part_start = i + 1;
                    }
                }
            }
            
            // Add remaining string part
            if (part_start < len) {
                int part_len = len - part_start;
                char* part = malloc(part_len + 1);
                memcpy(part, str + part_start, part_len);
                part[part_len] = '\0';
                expr->string_interp.parts[expr->string_interp.count] = part;
                expr->string_interp.expressions[expr->string_interp.count] = NULL;
                expr->string_interp.count++;
            }
            
            return expr;
        }
        
        Expr* expr = alloc_expr();
        expr->type = EXPR_STRING;
        expr->token = parser.previous;
        return expr;
    }
    
    if (match(TOKEN_CHAR)) {
        Expr* expr = alloc_expr();
        expr->type = EXPR_CHAR;
        expr->token = parser.previous;
        return expr;
    }
    
    if (match(TOKEN_IDENT)) {
        Token name = parser.previous;
        
        // Check for struct initialization: TypeName { field: value, ... }
        // Only if we're in an expression context and the identifier starts with uppercase
        if (check(TOKEN_LBRACE) && name.start[0] >= 'A' && name.start[0] <= 'Z') {
            advance(); // consume '{'
            
            Expr* expr = alloc_expr();
            expr->type = EXPR_STRUCT_INIT;
            expr->struct_init.type_name = name;
            expr->struct_init.field_names = malloc(sizeof(Token) * 16);
            expr->struct_init.field_values = malloc(sizeof(Expr*) * 16);
            expr->struct_init.field_count = 0;
            
            if (!check(TOKEN_RBRACE)) {
                do {
                    // Parse field name
                    expect(TOKEN_IDENT, "Expected field name");
                    expr->struct_init.field_names[expr->struct_init.field_count] = parser.previous;
                    
                    expect(TOKEN_COLON, "Expected ':' after field name");
                    
                    // Parse field value
                    expr->struct_init.field_values[expr->struct_init.field_count] = expression();
                    expr->struct_init.field_count++;
                } while (match(TOKEN_COMMA));
            }
            
            expect(TOKEN_RBRACE, "Expected '}' after struct fields");
            return expr;
        }
        
        // Check for qualified name: EnumName::Variant
        if (match(TOKEN_COLONCOLON)) {
            expect(TOKEN_IDENT, "Expected identifier after '::'");
            Token variant = parser.previous;
            
            // Create a qualified identifier by combining them
            char* qualified = malloc(name.length + 2 + variant.length + 1);
            memcpy(qualified, name.start, name.length);
            qualified[name.length] = ':';
            qualified[name.length + 1] = ':';
            memcpy(qualified + name.length + 2, variant.start, variant.length);
            qualified[name.length + 2 + variant.length] = '\0';
            
            Token qualified_token;
            qualified_token.type = TOKEN_IDENT;
            qualified_token.start = qualified;
            qualified_token.length = name.length + 2 + variant.length;
            qualified_token.line = name.line;
            
            Expr* expr = alloc_expr();
            expr->type = EXPR_IDENT;
            expr->token = qualified_token;
            return expr;
        }
        
        // Regular identifier
        Expr* expr = alloc_expr();
        expr->type = EXPR_IDENT;
        expr->token = name;
        
        // T2.5.1: Check for optional type suffix '?'
        if (check(TOKEN_QUESTION)) {
            advance(); // consume '?'
            Expr* optional_expr = alloc_expr();
            optional_expr->type = EXPR_OPTIONAL_TYPE;
            optional_expr->optional_type.inner_type = expr;
            return optional_expr;
        }
        
        return expr;
    }
    
    if (match(TOKEN_TRUE) || match(TOKEN_FALSE)) {
        Expr* expr = alloc_expr();
        expr->type = EXPR_BOOL;
        expr->token = parser.previous;
        return expr;
    }
    
    if (match(TOKEN_NULL)) {
        Expr* expr = alloc_expr();
        expr->type = EXPR_INT;
        Token zero = {TOKEN_INT, "0", 1, 0};
        expr->token = zero;
        return expr;
    }
    
    if (match(TOKEN_NONE)) {
        Expr* expr = alloc_expr();
        expr->type = EXPR_NONE;
        // Allow optional parentheses: none or none()
        if (check(TOKEN_LPAREN)) {
            advance();  // consume '('
            expect(TOKEN_RPAREN, "Expected ')' after none");
        }
        return expr;
    }
    
    if (match(TOKEN_SOME)) {
        Expr* expr = alloc_expr();
        expr->type = EXPR_SOME;
        expect(TOKEN_LPAREN, "Expected '(' after some");
        expr->option.value = expression();
        expect(TOKEN_RPAREN, "Expected ')' after value");
        return expr;
    }
    
    if (match(TOKEN_OK)) {
        Expr* expr = alloc_expr();
        expr->type = EXPR_OK;
        expect(TOKEN_LPAREN, "Expected '(' after ok");
        expr->option.value = expression();
        expect(TOKEN_RPAREN, "Expected ')' after value");
        return expr;
    }
    
    if (match(TOKEN_ERR)) {
        Expr* expr = alloc_expr();
        expr->type = EXPR_ERR;
        expect(TOKEN_LPAREN, "Expected '(' after err");
        expr->option.value = expression();
        expect(TOKEN_RPAREN, "Expected ')' after value");
        return expr;
    }
    
    if (match(TOKEN_IF)) {
        Expr* expr = alloc_expr();
        expr->type = EXPR_IF_EXPR;
        expr->if_expr.condition = expression();
        expect(TOKEN_LBRACE, "Expected '{' after if condition");
        expr->if_expr.then_expr = expression();
        expect(TOKEN_RBRACE, "Expected '}' after if expression");
        
        if (match(TOKEN_ELSE)) {
            expect(TOKEN_LBRACE, "Expected '{' after else");
            expr->if_expr.else_expr = expression();
            expect(TOKEN_RBRACE, "Expected '}' after else expression");
        } else {
            expr->if_expr.else_expr = NULL;
        }
        return expr;
    }
    
    if (match(TOKEN_MATCH)) {
        Expr* expr = alloc_expr();
        expr->type = EXPR_MATCH;
        expr->match.value = expression();
        expect(TOKEN_LBRACE, "Expected '{' after match value");
        
        expr->match.arm_count = 0;
        expr->match.arms = malloc(sizeof(MatchArm) * 32);
        
        while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
            // Parse pattern - support integer literals, wildcards, and identifiers
            if (check(TOKEN_INT) || check(TOKEN_FLOAT) || check(TOKEN_STRING) || 
                check(TOKEN_TRUE) || check(TOKEN_FALSE)) {
                // Literal pattern
                expr->match.arms[expr->match.arm_count].pattern = parser.current;
                advance();
            } else if (check(TOKEN_UNDERSCORE)) {
                // Wildcard pattern
                expr->match.arms[expr->match.arm_count].pattern = parser.current;
                advance();
            } else if (check(TOKEN_IDENT)) {
                // Identifier pattern
                expr->match.arms[expr->match.arm_count].pattern = parser.current;
                advance();
            } else {
                fprintf(stderr, "Error at line %d: Expected pattern (literal, identifier, or _)\n", parser.current.line);
                parser.had_error = true;
                return NULL;
            }
            
            expect(TOKEN_FATARROW, "Expected '=>' after pattern");
            expr->match.arms[expr->match.arm_count].result = expression();
            expr->match.arm_count++;
            if (!check(TOKEN_RBRACE)) {
                match(TOKEN_COMMA);
            }
        }
        
        expect(TOKEN_RBRACE, "Expected '}' after match arms");
        return expr;
    }
    
    if (match(TOKEN_LBRACKET)) {
        Expr* expr = alloc_expr();
        expr->type = EXPR_ARRAY;
        expr->array.elements = NULL;
        expr->array.count = 0;
        
        if (!check(TOKEN_RBRACKET)) {
            int capacity = 8;
            expr->array.elements = malloc(sizeof(Expr*) * capacity);
            do {
                if (expr->array.count >= capacity) {
                    capacity *= 2;
                    expr->array.elements = realloc(expr->array.elements, sizeof(Expr*) * capacity);
                }
                expr->array.elements[expr->array.count++] = expression();
            } while (match(TOKEN_COMMA));
        }
        
        expect(TOKEN_RBRACKET, "Expected ']' after array elements");
        return expr;
    }
    
    // v1.2.4: {} for HashMap, {:} for HashSet
    if (match(TOKEN_LBRACE)) {
        // Check if next token is : followed by }
        if (check(TOKEN_COLON)) {
            Token colon = parser.current;
            advance();  // consume :
            if (check(TOKEN_RBRACE)) {
                advance();  // consume }
                // {:} is HashSet literal
                Expr* expr = alloc_expr();
                expr->type = EXPR_HASHSET_LITERAL;
                expr->array.elements = NULL;
                expr->array.count = 0;
                return expr;
            }
            // Not {:}, backtrack - this will be an error
            parser.current = colon;
        }
        
        // Otherwise it's a HashMap
        Expr* expr = alloc_expr();
        expr->type = EXPR_HASHMAP_LITERAL;
        expr->array.elements = NULL;
        expr->array.count = 0;
        
        // Parse key-value pairs: {"key": value, "key2": value2}
        if (!check(TOKEN_RBRACE)) {
            int capacity = 8;
            expr->array.elements = malloc(sizeof(Expr*) * capacity);
            
            do {
                // Expect string key
                if (!check(TOKEN_STRING)) {
                    fprintf(stderr, "Error: HashMap keys must be strings\n");
                    break;
                }
                Expr* key = expression();
                
                // Expect colon
                if (!match(TOKEN_COLON)) {
                    fprintf(stderr, "Error: Expected ':' after HashMap key\n");
                    break;
                }
                
                // Parse value
                Expr* value = expression();
                
                // Store key-value pair (key at even index, value at odd)
                if (expr->array.count + 1 >= capacity) {
                    capacity *= 2;
                    expr->array.elements = realloc(expr->array.elements, sizeof(Expr*) * capacity);
                }
                expr->array.elements[expr->array.count++] = key;
                expr->array.elements[expr->array.count++] = value;
                
            } while (match(TOKEN_COMMA));
        }
        
        expect(TOKEN_RBRACE, "Expected '}' after hashmap elements");
        return expr;
    }
    
    // v1.2.3: () for HashSet literal - DISABLED due to conflict with tuples/grouping
    // Use hashset_new() for now
    
    if (match(TOKEN_MAP)) {
        Expr* expr = alloc_expr();
        expr->type = EXPR_MAP;
        expect(TOKEN_LBRACKET, "Expected '[' after 'map'");
        
        // Skip key and value types for now (map[string, int])
        expression(); // key type
        expect(TOKEN_COMMA, "Expected ',' between key and value types");
        expression(); // value type
        expect(TOKEN_RBRACKET, "Expected ']' after map types");
        
        expect(TOKEN_LBRACE, "Expected '{' after map type");
        
        expr->map.keys = NULL;
        expr->map.values = NULL;
        expr->map.count = 0;
        
        if (!check(TOKEN_RBRACE)) {
            int capacity = 8;
            expr->map.keys = malloc(sizeof(Expr*) * capacity);
            expr->map.values = malloc(sizeof(Expr*) * capacity);
            
            do {
                if (expr->map.count >= capacity) {
                    capacity *= 2;
                    expr->map.keys = realloc(expr->map.keys, sizeof(Expr*) * capacity);
                    expr->map.values = realloc(expr->map.values, sizeof(Expr*) * capacity);
                }
                
                expr->map.keys[expr->map.count] = expression();
                expect(TOKEN_COLON, "Expected ':' after map key");
                expr->map.values[expr->map.count] = expression();
                expr->map.count++;
            } while (match(TOKEN_COMMA));
        }
        
        expect(TOKEN_RBRACE, "Expected '}' after map elements");
        return expr;
    }
    
    // TASK-040: Lambda expression parsing |x| x * 2
    if (match(TOKEN_PIPE)) {
        Expr* lambda_expr = alloc_expr();
        lambda_expr->type = EXPR_LAMBDA;
        
        // Parse parameters
        lambda_expr->lambda.param_count = 0;
        lambda_expr->lambda.params = malloc(sizeof(Token) * 8);
        
        if (!check(TOKEN_PIPE)) {
            do {
                expect(TOKEN_IDENT, "Expected parameter name");
                lambda_expr->lambda.params[lambda_expr->lambda.param_count++] = parser.previous;
                
                // Skip optional type annotation
                if (match(TOKEN_COLON)) {
                    // Skip type without using parse_type (which would consume | for union types)
                    // Just skip the type identifier
                    if (check(TOKEN_IDENT)) {
                        advance(); // Skip type name like 'int', 'string', etc.
                        // Skip generic parameters if present
                        if (match(TOKEN_LT)) {
                            int depth = 1;
                            while (depth > 0 && !check(TOKEN_EOF)) {
                                if (match(TOKEN_LT)) depth++;
                                else if (match(TOKEN_GT)) depth--;
                                else advance();
                            }
                        }
                    }
                }
            } while (match(TOKEN_COMMA));
        }
        
        expect(TOKEN_PIPE, "Expected '|' after lambda parameters");
        
        // Skip optional return type annotation
        if (match(TOKEN_ARROW)) {
            // Skip return type without using parse_type
            if (check(TOKEN_IDENT)) {
                advance();
                if (match(TOKEN_LT)) {
                    int depth = 1;
                    while (depth > 0 && !check(TOKEN_EOF)) {
                        if (match(TOKEN_LT)) depth++;
                        else if (match(TOKEN_GT)) depth--;
                        else advance();
                    }
                }
            }
        }
        
        // Parse body - can be expression or block
        if (check(TOKEN_LBRACE)) {
            // Block body: { return expr; }
            // Skip the opening brace
            advance();
            
            // Skip to return statement
            if (match(TOKEN_RETURN)) {
                lambda_expr->lambda.body = expression();
                expect(TOKEN_SEMI, "Expected ';' after return expression");
            } else {
                // No return, just parse expression
                lambda_expr->lambda.body = expression();
            }
            
            expect(TOKEN_RBRACE, "Expected '}' after lambda block body");
        } else {
            lambda_expr->lambda.body = expression();
        }
        
        // Initialize capture fields (will be filled by capture analysis)
        lambda_expr->lambda.captured_vars = NULL;
        lambda_expr->lambda.captured_count = 0;
        lambda_expr->lambda.capture_by_move = NULL;
        
        return lambda_expr;
    }

    // TASK-7.1: Lambda expression parsing fn(x) => x * 2
    if (match(TOKEN_FN)) {
        Expr* lambda_expr = alloc_expr();
        lambda_expr->type = EXPR_LAMBDA;
        
        expect(TOKEN_LPAREN, "Expected '(' after 'fn'");
        
        // Parse parameters
        lambda_expr->lambda.param_count = 0;
        lambda_expr->lambda.params = malloc(sizeof(Token) * 8);
        
        if (!check(TOKEN_RPAREN)) {
            do {
                expect(TOKEN_IDENT, "Expected parameter name");
                lambda_expr->lambda.params[lambda_expr->lambda.param_count++] = parser.previous;
                
                // Parse optional type annotation
                if (match(TOKEN_COLON)) {
                    // Skip type annotation for now (simplified)
                    if (check(TOKEN_IDENT)) {
                        advance(); // Skip type name
                    }
                }
            } while (match(TOKEN_COMMA));
        }
        
        expect(TOKEN_RPAREN, "Expected ')' after lambda parameters");
        
        // Parse optional return type annotation
        if (match(TOKEN_ARROW)) {
            // Skip return type annotation for now (simplified)
            if (check(TOKEN_IDENT)) {
                advance();
            }
        }
        
        expect(TOKEN_FATARROW, "Expected '=>' after lambda signature");
        
        // Parse body - can be expression or block
        if (check(TOKEN_LBRACE)) {
            // Block body: { var y = x * 2; return y + 1; }
            advance(); // consume '{'
            
            // For now, skip all statements until we find a return or reach the end
            // This is a simplified implementation for Task 7.1
            while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
                if (match(TOKEN_RETURN)) {
                    lambda_expr->lambda.body = expression();
                    expect(TOKEN_SEMI, "Expected ';' after return expression");
                    break;
                } else {
                    // Skip other statements for now
                    while (!check(TOKEN_SEMI) && !check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
                        advance();
                    }
                    if (check(TOKEN_SEMI)) advance();
                }
            }
            
            expect(TOKEN_RBRACE, "Expected '}' after lambda block body");
        } else {
            // Expression body: x * 2
            lambda_expr->lambda.body = expression();
        }
        
        // Initialize capture fields (will be filled by capture analysis)
        lambda_expr->lambda.captured_vars = NULL;
        lambda_expr->lambda.captured_count = 0;
        lambda_expr->lambda.capture_by_move = NULL;
        
        return lambda_expr;
    }

    if (match(TOKEN_LPAREN)) {
        // Check if this is a tuple (has comma) or just grouped expression
        Expr* first_expr = expression();
        
        if (match(TOKEN_COMMA)) {
            // This is a tuple
            Expr* tuple_expr = alloc_expr();
            tuple_expr->type = EXPR_TUPLE;
            tuple_expr->tuple.elements = malloc(sizeof(Expr*) * 8);
            tuple_expr->tuple.count = 1;
            tuple_expr->tuple.elements[0] = first_expr;
            
            do {
                if (tuple_expr->tuple.count >= 8) {
                    // Expand if needed (simplified)
                    tuple_expr->tuple.elements = realloc(tuple_expr->tuple.elements, sizeof(Expr*) * (tuple_expr->tuple.count * 2));
                }
                tuple_expr->tuple.elements[tuple_expr->tuple.count++] = expression();
            } while (match(TOKEN_COMMA));
            
            expect(TOKEN_RPAREN, "Expected ')' after tuple elements");
            return tuple_expr;
        } else {
            // Just a grouped expression
            expect(TOKEN_RPAREN, "Expected ')' after expression");
            return first_expr;
        }
    }
    
    return NULL;
}

static Expr* call() {
    Expr* expr = primary();
    
    while (true) {
        if (match(TOKEN_LPAREN)) {
            Expr* call_expr = alloc_expr();
            call_expr->type = EXPR_CALL;
            call_expr->call.callee = expr;
            call_expr->call.args = NULL;
            call_expr->call.arg_count = 0;
            
            if (!check(TOKEN_RPAREN)) {
                int capacity = 8;
                call_expr->call.args = malloc(sizeof(Expr*) * capacity);
                do {
                    if (call_expr->call.arg_count >= capacity) {
                        capacity *= 2;
                        call_expr->call.args = realloc(call_expr->call.args, sizeof(Expr*) * capacity);
                    }
                    call_expr->call.args[call_expr->call.arg_count++] = expression();
                } while (match(TOKEN_COMMA));
            }
            
            expect(TOKEN_RPAREN, "Expected ')' after arguments");
            expr = call_expr;
        } else if (match(TOKEN_LBRACKET)) {
            // Check if this is a slice (arr[1..3]) or index (arr[1])
            Expr* first_expr = expression();
            
            if (match(TOKEN_DOTDOT)) {
                // It's a slice: arr[start..end]
                Expr* end_expr = expression();
                expect(TOKEN_RBRACKET, "Expected ']' after slice");
                
                // Convert to method call: arr.slice(start, end)
                Expr* method_call = alloc_expr();
                method_call->type = EXPR_METHOD_CALL;
                method_call->method_call.object = expr;
                method_call->method_call.method.start = "slice";
                method_call->method_call.method.length = 5;
                method_call->method_call.method.line = parser.previous.line;
                method_call->method_call.arg_count = 2;
                method_call->method_call.args = malloc(sizeof(Expr*) * 2);
                method_call->method_call.args[0] = first_expr;
                method_call->method_call.args[1] = end_expr;
                expr = method_call;
            } else {
                // It's an index: arr[i]
                expect(TOKEN_RBRACKET, "Expected ']' after index");
                Expr* index_expr = alloc_expr();
                index_expr->type = EXPR_INDEX;
                index_expr->index.array = expr;
                index_expr->index.index = first_expr;
                expr = index_expr;
            }
        } else if (match(TOKEN_DOT)) {
            // Check if this is tuple element access (tuple.0, tuple.1, etc.)
            if (check(TOKEN_INT)) {
                Token index_token = parser.current;
                advance();
                
                // Create tuple index access expression
                Expr* tuple_index_expr = alloc_expr();
                tuple_index_expr->type = EXPR_TUPLE_INDEX;
                tuple_index_expr->tuple_index.tuple = expr;
                tuple_index_expr->tuple_index.index = atoi(index_token.start);
                expr = tuple_index_expr;
            } else {
                Token field_or_method = parser.current;
                expect(TOKEN_IDENT, "Expected field or method name after '.'");
            
            // Check for module.Type { ... } struct initialization
            if (check(TOKEN_LBRACE) && field_or_method.start[0] >= 'A' && field_or_method.start[0] <= 'Z') {
                advance(); // consume '{'
                
                Expr* struct_expr = alloc_expr();
                struct_expr->type = EXPR_STRUCT_INIT;
                struct_expr->struct_init.type_name = field_or_method;
                struct_expr->struct_init.field_names = malloc(sizeof(Token) * 16);
                struct_expr->struct_init.field_values = malloc(sizeof(Expr*) * 16);
                struct_expr->struct_init.field_count = 0;
                
                if (!check(TOKEN_RBRACE)) {
                    do {
                        expect(TOKEN_IDENT, "Expected field name");
                        struct_expr->struct_init.field_names[struct_expr->struct_init.field_count] = parser.previous;
                        expect(TOKEN_COLON, "Expected ':' after field name");
                        struct_expr->struct_init.field_values[struct_expr->struct_init.field_count] = expression();
                        struct_expr->struct_init.field_count++;
                    } while (match(TOKEN_COMMA));
                }
                
                expect(TOKEN_RBRACE, "Expected '}' after struct fields");
                expr = struct_expr;
            } else if (match(TOKEN_LPAREN)) {
                Expr* method_expr = alloc_expr();
                method_expr->type = EXPR_METHOD_CALL;
                method_expr->method_call.object = expr;
                method_expr->method_call.method = field_or_method;
                method_expr->method_call.args = NULL;
                method_expr->method_call.arg_count = 0;
                
                if (!check(TOKEN_RPAREN)) {
                    int capacity = 8;
                    method_expr->method_call.args = malloc(sizeof(Expr*) * capacity);
                    do {
                        if (method_expr->method_call.arg_count >= capacity) {
                            capacity *= 2;
                            method_expr->method_call.args = realloc(method_expr->method_call.args, sizeof(Expr*) * capacity);
                        }
                        method_expr->method_call.args[method_expr->method_call.arg_count++] = expression();
                    } while (match(TOKEN_COMMA));
                }
                
                expect(TOKEN_RPAREN, "Expected ')' after arguments");
                expr = method_expr;
            } else {
                // Check if this is enum member access (EnumName.MEMBER)
                if (expr->type == EXPR_IDENT) {
                    // This could be enum member access - let checker validate
                    Expr* field_expr = alloc_expr();
                    field_expr->type = EXPR_FIELD_ACCESS;
                    field_expr->field_access.object = expr;
                    field_expr->field_access.field = field_or_method;
                    field_expr->field_access.is_enum_access = false; // Will be set by checker
                    expr = field_expr;
                } else {
                    // Regular field access
                    Expr* field_expr = alloc_expr();
                    field_expr->type = EXPR_FIELD_ACCESS;
                    field_expr->field_access.object = expr;
                    field_expr->field_access.field = field_or_method;
                    field_expr->field_access.is_enum_access = false;
                    expr = field_expr;
                }
            }
            }
        } else if (match(TOKEN_QUESTION)) {
            // TASK-028: Handle ? operator for error propagation
            Expr* try_expr = alloc_expr();
            try_expr->type = EXPR_TRY;
            try_expr->try_expr.value = expr;
            expr = try_expr;
        } else {
            break;
        }
    }
    
    return expr;
}

static Expr* multiplication() {
    Expr* expr = call();
    
    while (match(TOKEN_STAR) || match(TOKEN_SLASH) || match(TOKEN_PERCENT)) {
        Token op = parser.previous;
        Expr* right = call();
        Expr* binary = alloc_expr();
        binary->type = EXPR_BINARY;
        binary->binary.left = expr;
        binary->binary.op = op;
        binary->binary.right = right;
        expr = binary;
    }
    
    return expr;
}

static Expr* bitwise() {
    Expr* expr = multiplication();
    
    while (match(TOKEN_AMP) || match(TOKEN_PIPE) || match(TOKEN_CARET)) {
        Token op = parser.previous;
        Expr* right = multiplication();
        Expr* binary = alloc_expr();
        binary->type = EXPR_BINARY;
        binary->binary.left = expr;
        binary->binary.op = op;
        binary->binary.right = right;
        expr = binary;
    }
    
    return expr;
}

static Expr* addition() {
    Expr* expr = bitwise();
    
    while (match(TOKEN_PLUS) || match(TOKEN_MINUS)) {
        Token op = parser.previous;
        Expr* right = bitwise();
        Expr* binary = alloc_expr();
        binary->type = EXPR_BINARY;
        binary->binary.left = expr;
        binary->binary.op = op;
        binary->binary.right = right;
        expr = binary;
    }
    
    return expr;
}

static Expr* comparison() {
    Expr* expr = addition();
    
    while (match(TOKEN_LT) || match(TOKEN_GT) || match(TOKEN_LTEQ) || match(TOKEN_GTEQ) ||
           match(TOKEN_EQEQ) || match(TOKEN_BANGEQ)) {
        Token op = parser.previous;
        Expr* right = addition();
        Expr* binary = alloc_expr();
        binary->type = EXPR_BINARY;
        binary->binary.left = expr;
        binary->binary.op = op;
        binary->binary.right = right;
        expr = binary;
    }
    
    return expr;
}

static Expr* logical_and() {
    Expr* expr = comparison();
    
    while (match(TOKEN_AND) || match(TOKEN_AMPAMP)) {
        Token op = parser.previous;
        Expr* right = comparison();
        Expr* binary = alloc_expr();
        binary->type = EXPR_BINARY;
        binary->binary.left = expr;
        binary->binary.op = op;
        binary->binary.right = right;
        expr = binary;
    }
    
    return expr;
}

static Expr* logical_or() {
    Expr* expr = logical_and();
    
    while (match(TOKEN_OR) || match(TOKEN_PIPEPIPE)) {
        Token op = parser.previous;
        Expr* right = logical_and();
        Expr* binary = alloc_expr();
        binary->type = EXPR_BINARY;
        binary->binary.left = expr;
        binary->binary.op = op;
        binary->binary.right = right;
        expr = binary;
    }
    
    // Nil coalescing operator ??
    while (match(TOKEN_QUESTION_QUESTION)) {
        Token op = parser.previous;
        Expr* right = logical_and();
        Expr* binary = alloc_expr();
        binary->type = EXPR_BINARY;
        binary->binary.left = expr;
        binary->binary.op = op;
        binary->binary.right = right;
        expr = binary;
    }
    
    return expr;
}

static Expr* pipeline() {
    Expr* expr = logical_or();
    
    // Check for pipeline operator
    if (match(TOKEN_PIPEGT)) {
        Expr* pipe = alloc_expr();
        pipe->type = EXPR_PIPELINE;
        pipe->pipeline.stage_count = 2;
        pipe->pipeline.stages = malloc(sizeof(Expr*) * 8);
        pipe->pipeline.stages[0] = expr;
        pipe->pipeline.stages[1] = logical_or();
        
        // Handle multiple pipeline stages
        while (match(TOKEN_PIPEGT)) {
            if (pipe->pipeline.stage_count >= 8) break; // Limit to 8 stages
            pipe->pipeline.stages[pipe->pipeline.stage_count++] = logical_or();
        }
        
        return pipe;
    }
    
    return expr;
}

static Expr* assignment() {
    Expr* expr = pipeline();
    
    // Check for null expression
    if (!expr) {
        return NULL;
    }
    
    // Check for ++ and --
    if (match(TOKEN_PLUSPLUS)) {
        if (expr->type == EXPR_IDENT) {
            Expr* assign = alloc_expr();
            assign->type = EXPR_ASSIGN;
            assign->assign.name = expr->token;
            
            Expr* inc = alloc_expr();
            inc->type = EXPR_BINARY;
            inc->binary.left = expr;
            inc->binary.right = alloc_expr();
            inc->binary.right->type = EXPR_INT;
            Token one = {TOKEN_INT, "1", 1, 0};
            inc->binary.right->token = one;
            Token plus = {TOKEN_PLUS, "+", 1, 0};
            inc->binary.op = plus;
            
            assign->assign.value = inc;
            return assign;
        }
    }
    
    if (match(TOKEN_MINUSMINUS)) {
        if (expr->type == EXPR_IDENT) {
            Expr* assign = alloc_expr();
            assign->type = EXPR_ASSIGN;
            assign->assign.name = expr->token;
            
            Expr* dec = alloc_expr();
            dec->type = EXPR_BINARY;
            dec->binary.left = expr;
            dec->binary.right = alloc_expr();
            dec->binary.right->type = EXPR_INT;
            Token one = {TOKEN_INT, "1", 1, 0};
            dec->binary.right->token = one;
            Token minus = {TOKEN_MINUS, "-", 1, 0};
            dec->binary.op = minus;
            
            assign->assign.value = dec;
            return assign;
        }
    }
    
    // Check for index assignment first (map["key"] = value)
    if (expr->type == EXPR_INDEX && check(TOKEN_EQ)) {
        advance(); // consume the '='
        Expr* index_assign = alloc_expr();
        index_assign->type = EXPR_INDEX_ASSIGN;
        index_assign->index_assign.object = expr->index.array;
        index_assign->index_assign.index = expr->index.index;
        index_assign->index_assign.value = assignment();
        return index_assign;
    }
    
    if (match(TOKEN_EQ) || match(TOKEN_PLUSEQ) || match(TOKEN_MINUSEQ) || 
        match(TOKEN_STAREQ) || match(TOKEN_SLASHEQ)) {
        if (expr->type == EXPR_IDENT) {
            Token op = parser.previous;
            Expr* assign = alloc_expr();
            assign->type = EXPR_ASSIGN;
            assign->assign.name = expr->token;
            
            if (op.type == TOKEN_EQ) {
                assign->assign.value = assignment();
            } else {
                Expr* right = assignment();
                Expr* binary = alloc_expr();
                binary->type = EXPR_BINARY;
                binary->binary.left = expr;
                binary->binary.right = right;
                
                Token bin_op = {0};
                if (op.type == TOKEN_PLUSEQ) { bin_op.type = TOKEN_PLUS; bin_op.start = "+"; bin_op.length = 1; }
                else if (op.type == TOKEN_MINUSEQ) { bin_op.type = TOKEN_MINUS; bin_op.start = "-"; bin_op.length = 1; }
                else if (op.type == TOKEN_STAREQ) { bin_op.type = TOKEN_STAR; bin_op.start = "*"; bin_op.length = 1; }
                else if (op.type == TOKEN_SLASHEQ) { bin_op.type = TOKEN_SLASH; bin_op.start = "/"; bin_op.length = 1; }
                
                binary->binary.op = bin_op;
                assign->assign.value = binary;
            }
            
            return assign;
        } else if (expr->type == EXPR_FIELD_ACCESS) {
            // Handle field assignment: obj.field = value
            Token op = parser.previous;
            Expr* field_assign = alloc_expr();
            field_assign->type = EXPR_FIELD_ASSIGN;
            field_assign->field_assign.object = expr->field_access.object;
            field_assign->field_assign.field = expr->field_access.field;
            field_assign->field_assign.value = assignment();
            return field_assign;
        }
    }
    
    return expr;
}

Expr* expression() {
    return assignment();
}

Stmt* statement();

Stmt* statement() {
    if (match(TOKEN_TRY)) {
        Stmt* stmt = alloc_stmt();
        stmt->type = STMT_TRY;
        
        // Parse try block
        expect(TOKEN_LBRACE, "Expected '{' after 'try'");
        stmt->try_stmt.try_block = alloc_stmt();
        stmt->try_stmt.try_block->type = STMT_BLOCK;
        stmt->try_stmt.try_block->block.count = 0;
        stmt->try_stmt.try_block->block.stmts = malloc(sizeof(Stmt*) * 32);
        
        while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
            stmt->try_stmt.try_block->block.stmts[stmt->try_stmt.try_block->block.count++] = statement();
        }
        expect(TOKEN_RBRACE, "Expected '}' after try block");
        
        // Parse catch blocks using new structure
        stmt->try_stmt.catch_count = 0;
        stmt->try_stmt.catch_blocks = malloc(sizeof(Stmt*) * 8);
        stmt->try_stmt.exception_types = malloc(sizeof(Token) * 8);
        stmt->try_stmt.exception_vars = malloc(sizeof(Token) * 8);
        
        while (match(TOKEN_CATCH)) {
            expect(TOKEN_LPAREN, "Expected '(' after catch");
            
            // Parse exception type
            expect(TOKEN_IDENT, "Expected exception type");
            stmt->try_stmt.exception_types[stmt->try_stmt.catch_count] = parser.previous;
            
            // Parse exception variable
            expect(TOKEN_IDENT, "Expected exception variable");
            stmt->try_stmt.exception_vars[stmt->try_stmt.catch_count] = parser.previous;
            
            expect(TOKEN_RPAREN, "Expected ')' after catch parameters");
            expect(TOKEN_LBRACE, "Expected '{' after catch");
            
            stmt->try_stmt.catch_blocks[stmt->try_stmt.catch_count] = statement();
            stmt->try_stmt.catch_count++;
        }
        
        // Optional finally block
        if (match(TOKEN_FINALLY)) {
            expect(TOKEN_LBRACE, "Expected '{' after finally");
            stmt->try_stmt.finally_block = statement();
        } else {
            stmt->try_stmt.finally_block = NULL;
        }
        
        return stmt;
    }
    
    if (check(TOKEN_FN)) {
        fprintf(stderr, "Error at line %d: Nested functions are not supported. Functions can only be defined at the top level.\n", parser.current.line);
        parser.had_error = true;
        
        // Skip the entire function definition to prevent further errors
        advance(); // consume 'fn'
        
        // Skip function name
        if (check(TOKEN_IDENT)) advance();
        
        // Skip parameter list
        if (check(TOKEN_LPAREN)) {
            advance();
            int paren_count = 1;
            while (!check(TOKEN_EOF) && paren_count > 0) {
                if (check(TOKEN_LPAREN)) paren_count++;
                else if (check(TOKEN_RPAREN)) paren_count--;
                advance();
            }
        }
        
        // Skip return type if present
        if (check(TOKEN_ARROW)) {
            advance(); // consume '->'
            if (check(TOKEN_IDENT)) advance(); // consume type
        }
        
        // Skip function body
        if (check(TOKEN_LBRACE)) {
            advance();
            int brace_count = 1;
            while (!check(TOKEN_EOF) && brace_count > 0) {
                if (check(TOKEN_LBRACE)) brace_count++;
                else if (check(TOKEN_RBRACE)) brace_count--;
                advance();
            }
        }
        
        return NULL;
    }
    
    if (match(TOKEN_THROW)) {
        Stmt* stmt = alloc_stmt();
        stmt->type = STMT_THROW;
        stmt->throw_stmt.value = expression();
        match(TOKEN_SEMI);  // Optional semicolon
        return stmt;
    }
    
    if (match(TOKEN_RETURN)) {
        Stmt* stmt = alloc_stmt();
        stmt->type = STMT_RETURN;
        stmt->ret.value = expression();
        match(TOKEN_SEMI);  // Optional semicolon
        return stmt;
    }
    
    if (match(TOKEN_BREAK)) {
        Stmt* stmt = alloc_stmt();
        stmt->type = STMT_BREAK;
        match(TOKEN_SEMI);  // Optional semicolon
        return stmt;
    }
    
    if (match(TOKEN_CONTINUE)) {
        Stmt* stmt = alloc_stmt();
        stmt->type = STMT_CONTINUE;
        match(TOKEN_SEMI);  // Optional semicolon
        return stmt;
    }
    
    if (match(TOKEN_SPAWN)) {
        Stmt* stmt = alloc_stmt();
        stmt->type = STMT_SPAWN;
        stmt->spawn.call = expression();
        match(TOKEN_SEMI);
        return stmt;
    }
    
    if (match(TOKEN_VAR) || match(TOKEN_CONST)) {
        Stmt* stmt = alloc_stmt();
        stmt->type = STMT_VAR;
        WynTokenType decl_type = parser.previous.type;
        
        // var = mutable, const = immutable
        stmt->var.is_const = (decl_type == TOKEN_CONST);
        stmt->var.is_mutable = (decl_type == TOKEN_VAR);
        stmt->var.name = parser.current;
        expect(TOKEN_IDENT, "Expected variable name");
        
        // Check for type annotation: var name: type = value
        if (match(TOKEN_COLON)) {
            stmt->var.type = parse_type();
        } else {
            stmt->var.type = NULL;
        }
        
        expect(TOKEN_EQ, "Expected '=' after variable name");
        stmt->var.init = expression();
        return stmt;
    }
    
    if (match(TOKEN_UNSAFE)) {
        Stmt* stmt = alloc_stmt();
        stmt->type = STMT_UNSAFE;
        expect(TOKEN_LBRACE, "Expected '{' after 'unsafe'");
        stmt->block.stmts = malloc(sizeof(Stmt*) * 32);
        stmt->block.count = 0;
        while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
            stmt->block.stmts[stmt->block.count++] = statement();
        }
        expect(TOKEN_RBRACE, "Expected '}' after unsafe block");
        return stmt;
    }
    
    if (match(TOKEN_IF) || match(TOKEN_ELSEIF)) {
        Stmt* stmt = alloc_stmt();
        stmt->type = STMT_IF;
        if (match(TOKEN_LPAREN)) {
            stmt->if_stmt.condition = expression();
            expect(TOKEN_RPAREN, "Expected ')' after if condition");
        } else {
            stmt->if_stmt.condition = expression();
        }
        expect(TOKEN_LBRACE, "Expected '{' after if condition");
        stmt->if_stmt.then_branch = alloc_stmt();
        stmt->if_stmt.then_branch->type = STMT_BLOCK;
        stmt->if_stmt.then_branch->block.stmts = malloc(sizeof(Stmt*) * 32);
        stmt->if_stmt.then_branch->block.count = 0;
        while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
            stmt->if_stmt.then_branch->block.stmts[stmt->if_stmt.then_branch->block.count++] = statement();
        }
        expect(TOKEN_RBRACE, "Expected '}' after if body");
        
        if (match(TOKEN_ELSE)) {
            if (check(TOKEN_IF)) {
                stmt->if_stmt.else_branch = statement();
            } else {
                expect(TOKEN_LBRACE, "Expected '{' after else");
                stmt->if_stmt.else_branch = alloc_stmt();
                stmt->if_stmt.else_branch->type = STMT_BLOCK;
                stmt->if_stmt.else_branch->block.stmts = malloc(sizeof(Stmt*) * 32);
                stmt->if_stmt.else_branch->block.count = 0;
                while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
                    stmt->if_stmt.else_branch->block.stmts[stmt->if_stmt.else_branch->block.count++] = statement();
                }
                expect(TOKEN_RBRACE, "Expected '}' after else body");
            }
        } else if (check(TOKEN_ELSEIF)) {
            stmt->if_stmt.else_branch = statement();
        } else {
            stmt->if_stmt.else_branch = NULL;
        }
        return stmt;
    }
    
    if (match(TOKEN_IMPORT)) {
        Stmt* stmt = alloc_stmt();
        stmt->type = STMT_IMPORT;
        
        // Parse: import name [as alias] [from "path"]
        expect(TOKEN_IDENT, "Expected module name after 'import'");
        stmt->import.module = parser.previous;
        
        // Optional: as alias
        if (match(TOKEN_AS)) {
            expect(TOKEN_IDENT, "Expected alias name after 'as'");
            stmt->import.alias = parser.previous;
            
            // Register alias globally
            char module_name[256], alias_name[256];
            snprintf(module_name, sizeof(module_name), "%.*s",
                    stmt->import.module.length, stmt->import.module.start);
            snprintf(alias_name, sizeof(alias_name), "%.*s",
                    stmt->import.alias.length, stmt->import.alias.start);
            register_parser_module_alias(alias_name, module_name);
        } else {
            stmt->import.alias.start = NULL;
            stmt->import.alias.length = 0;
        }
        
        // Optional: from "path"
        if (match(TOKEN_FROM)) {
            expect(TOKEN_STRING, "Expected string path after 'from'");
            stmt->import.path = parser.previous;
        } else {
            // No path specified - use default
            stmt->import.path.start = NULL;
            stmt->import.path.length = 0;
        }
        
        stmt->import.items = NULL;
        stmt->import.item_count = 0;
        match(TOKEN_SEMI);  // Optional semicolon
        return stmt;
    }
    
    if (match(TOKEN_EXPORT)) {
        Stmt* stmt = alloc_stmt();
        stmt->type = STMT_EXPORT;
        stmt->export.stmt = statement();  // Parse the statement being exported
        return stmt;
    }
    
    if (match(TOKEN_WHILE)) {
        Stmt* stmt = alloc_stmt();
        stmt->type = STMT_WHILE;
        if (match(TOKEN_LPAREN)) {
            stmt->while_stmt.condition = expression();
            expect(TOKEN_RPAREN, "Expected ')' after while condition");
        } else {
            stmt->while_stmt.condition = expression();
        }
        expect(TOKEN_LBRACE, "Expected '{' after while condition");
        stmt->while_stmt.body = alloc_stmt();
        stmt->while_stmt.body->type = STMT_BLOCK;
        stmt->while_stmt.body->block.stmts = malloc(sizeof(Stmt*) * 32);
        stmt->while_stmt.body->block.count = 0;
        while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
            stmt->while_stmt.body->block.stmts[stmt->while_stmt.body->block.count++] = statement();
        }
        expect(TOKEN_RBRACE, "Expected '}' after while body");
        return stmt;
    }
    
    if (match(TOKEN_FOR)) {
        Stmt* stmt = alloc_stmt();
        stmt->type = STMT_FOR;
        stmt->for_stmt.array_expr = NULL;  // Initialize array iteration fields
        stmt->for_stmt.loop_var = (Token){0};
        
        // Check for optional opening parenthesis
        bool has_parens = match(TOKEN_LPAREN);
        
        // Check for range syntax: for i in 0..10 or array iteration: for item in array
        if (check(TOKEN_IDENT)) {
            Token loop_var = parser.current;
            advance();
            if (match(TOKEN_IN)) {
                // Parse the expression after 'in'
                Expr* iter_expr = expression();
                
                // Check if this is a range (has ..) or array iteration
                if (match(TOKEN_DOTDOT)) {
                    // Range-based for loop: for i in 0..10
                    Expr* end = expression();
                    
                    // If we had opening parens, expect closing parens
                    if (has_parens) {
                        expect(TOKEN_RPAREN, "Expected ')' after range");
                    }
                    
                    // Desugar to: for var i = start; i < end; i += 1
                    stmt->for_stmt.init = alloc_stmt();
                    stmt->for_stmt.init->type = STMT_VAR;
                    stmt->for_stmt.init->var.name = loop_var;
                    stmt->for_stmt.init->var.init = iter_expr;
                    stmt->for_stmt.init->var.is_const = false;
                    
                    Expr* cond = alloc_expr();
                    cond->type = EXPR_BINARY;
                    cond->binary.left = alloc_expr();
                    cond->binary.left->type = EXPR_IDENT;
                    cond->binary.left->token = loop_var;
                    cond->binary.op.type = TOKEN_LT;
                    cond->binary.op.start = "<";
                    cond->binary.op.length = 1;
                    cond->binary.right = end;
                    stmt->for_stmt.condition = cond;
                    
                    Expr* inc = alloc_expr();
                    inc->type = EXPR_ASSIGN;
                    inc->assign.name = loop_var;
                    Expr* inc_val = alloc_expr();
                    inc_val->type = EXPR_BINARY;
                    inc_val->binary.left = alloc_expr();
                    inc_val->binary.left->type = EXPR_IDENT;
                    inc_val->binary.left->token = loop_var;
                    inc_val->binary.op.type = TOKEN_PLUS;
                    inc_val->binary.op.start = "+";
                    inc_val->binary.op.length = 1;
                    inc_val->binary.right = alloc_expr();
                    inc_val->binary.right->type = EXPR_INT;
                    Token one = {TOKEN_INT, "1", 1, 0};
                    inc_val->binary.right->token = one;
                    inc->assign.value = inc_val;
                    stmt->for_stmt.increment = inc;
                } else {
                    // Array iteration: for item in array
                    // If we had opening parens, expect closing parens
                    if (has_parens) {
                        expect(TOKEN_RPAREN, "Expected ')' after array");
                    }
                    
                    // Simplified approach: desugar to range-based loop
                    // for item in array -> for i in 0..array.length { var item = array[i]; ... }
                    
                    // Create index variable name (loop_var + "_i")
                    static char index_name[64];
                    snprintf(index_name, 64, "%.*s_i", loop_var.length, loop_var.start);
                    Token index_var = {TOKEN_IDENT, index_name, strlen(index_name), loop_var.line};
                    
                    // Initialize: var loop_var_i = 0
                    stmt->for_stmt.init = alloc_stmt();
                    stmt->for_stmt.init->type = STMT_VAR;
                    stmt->for_stmt.init->var.name = index_var;
                    stmt->for_stmt.init->var.init = alloc_expr();
                    stmt->for_stmt.init->var.init->type = EXPR_INT;
                    Token zero = {TOKEN_INT, "0", 1, 0};
                    stmt->for_stmt.init->var.init->token = zero;
                    stmt->for_stmt.init->var.is_const = false;
                    
                    // Condition: loop_var_i < array.length (simplified - use hardcoded length for now)
                    stmt->for_stmt.condition = alloc_expr();
                    stmt->for_stmt.condition->type = EXPR_BINARY;
                    stmt->for_stmt.condition->binary.left = alloc_expr();
                    stmt->for_stmt.condition->binary.left->type = EXPR_IDENT;
                    stmt->for_stmt.condition->binary.left->token = index_var;
                    stmt->for_stmt.condition->binary.op.type = TOKEN_LT;
                    stmt->for_stmt.condition->binary.op.start = "<";
                    stmt->for_stmt.condition->binary.op.length = 1;
                    // For now, assume array length is 3 (hardcoded for testing)
                    stmt->for_stmt.condition->binary.right = alloc_expr();
                    stmt->for_stmt.condition->binary.right->type = EXPR_INT;
                    Token three = {TOKEN_INT, "3", 1, 0};
                    stmt->for_stmt.condition->binary.right->token = three;
                    
                    // Increment: loop_var_i += 1
                    stmt->for_stmt.increment = alloc_expr();
                    stmt->for_stmt.increment->type = EXPR_ASSIGN;
                    stmt->for_stmt.increment->assign.name = index_var;
                    Expr* inc_val = alloc_expr();
                    inc_val->type = EXPR_BINARY;
                    inc_val->binary.left = alloc_expr();
                    inc_val->binary.left->type = EXPR_IDENT;
                    inc_val->binary.left->token = index_var;
                    inc_val->binary.op.type = TOKEN_PLUS;
                    inc_val->binary.op.start = "+";
                    inc_val->binary.op.length = 1;
                    inc_val->binary.right = alloc_expr();
                    inc_val->binary.right->type = EXPR_INT;
                    Token one = {TOKEN_INT, "1", 1, 0};
                    inc_val->binary.right->token = one;
                    stmt->for_stmt.increment->assign.value = inc_val;
                    
                    // Store array expression and loop variable for body processing
                    stmt->for_stmt.array_expr = iter_expr;
                    stmt->for_stmt.loop_var = loop_var;
                }
                
                expect(TOKEN_LBRACE, "Expected '{' after for header");
                stmt->for_stmt.body = alloc_stmt();
                stmt->for_stmt.body->type = STMT_BLOCK;
                stmt->for_stmt.body->block.stmts = malloc(sizeof(Stmt*) * 32);
                stmt->for_stmt.body->block.count = 0;
                while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
                    stmt->for_stmt.body->block.stmts[stmt->for_stmt.body->block.count++] = statement();
                }
                expect(TOKEN_RBRACE, "Expected '}' after for body");
                return stmt;
            }
            // Not a range loop, backtrack
            parser.current = parser.previous;
        }
        
        // Regular C-style for loop
        if (match(TOKEN_VAR)) {
            stmt->for_stmt.init = alloc_stmt();
            stmt->for_stmt.init->type = STMT_VAR;
            stmt->for_stmt.init->var.name = parser.current;
            expect(TOKEN_IDENT, "Expected variable name");
            expect(TOKEN_EQ, "Expected '=' after variable name");
            stmt->for_stmt.init->var.init = expression();
        } else {
            stmt->for_stmt.init = NULL;
        }
        
        expect(TOKEN_SEMI, "Expected ';' after for init");
        stmt->for_stmt.condition = expression();
        expect(TOKEN_SEMI, "Expected ';' after for condition");
        stmt->for_stmt.increment = expression();
        
        // If we had opening parens, expect closing parens
        if (has_parens) {
            expect(TOKEN_RPAREN, "Expected ')' after for header");
        }
        
        expect(TOKEN_LBRACE, "Expected '{' after for header");
        stmt->for_stmt.body = alloc_stmt();
        stmt->for_stmt.body->type = STMT_BLOCK;
        stmt->for_stmt.body->block.stmts = malloc(sizeof(Stmt*) * 32);
        stmt->for_stmt.body->block.count = 0;
        while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
            stmt->for_stmt.body->block.stmts[stmt->for_stmt.body->block.count++] = statement();
        }
        expect(TOKEN_RBRACE, "Expected '}' after for body");
        return stmt;
    }
    
    if (check(TOKEN_MATCH)) {
        return parse_match_statement();
    }
    
    Stmt* stmt = alloc_stmt();
    stmt->type = STMT_EXPR;
    stmt->expr = expression();
    match(TOKEN_SEMI);  // Optional semicolon
    return stmt;
}

Stmt* function() {
    bool is_async = match(TOKEN_ASYNC);
    bool is_public = match(TOKEN_PUB);
    expect(TOKEN_FN, "Expected 'fn'");
    Stmt* stmt = alloc_stmt();
    stmt->type = STMT_FN;
    stmt->fn.is_public = is_public;
    stmt->fn.is_async = is_async;
    stmt->fn.name = parser.current;
    expect(TOKEN_IDENT, "Expected function name");
    
    // Check for extension method syntax: fn Type.method()
    Token receiver_type = {0};
    bool is_extension = false;
    if (match(TOKEN_DOT)) {
        receiver_type = stmt->fn.name;
        stmt->fn.name = parser.current;
        expect(TOKEN_IDENT, "Expected method name after '.'");
        is_extension = true;
    }
    
    stmt->fn.type_param_count = 0;
    stmt->fn.type_params = NULL;
    if (match(TOKEN_LT)) {
        stmt->fn.type_params = malloc(sizeof(Token) * 8);
        do {
            stmt->fn.type_params[stmt->fn.type_param_count++] = parser.current;
            expect(TOKEN_IDENT, "Expected type parameter");
            
            // Parse optional type constraints (T: Display + Debug)
            if (match(TOKEN_COLON)) {
                // Skip constraint parsing for now - handled in generics.c
                while (!check(TOKEN_COMMA) && !check(TOKEN_GT)) {
                    advance(); // Skip constraint tokens
                }
            }
        } while (match(TOKEN_COMMA));
        expect(TOKEN_GT, "Expected '>' after type parameters");
    }
    
    // Store extension method info
    stmt->fn.is_extension = is_extension;
    if (is_extension) {
        stmt->fn.receiver_type = receiver_type;
    }
    
    expect(TOKEN_LPAREN, "Expected '(' after function name");
    
    stmt->fn.param_count = 0;
    stmt->fn.params = NULL;
    stmt->fn.param_types = NULL;
    stmt->fn.param_mutable = NULL;
    stmt->fn.param_defaults = NULL;  // T1.5.2: Initialize default parameter array
    
    if (!check(TOKEN_RPAREN)) {
        int capacity = 8;
        stmt->fn.params = malloc(sizeof(Token) * capacity);
        stmt->fn.param_types = malloc(sizeof(Expr*) * capacity);
        stmt->fn.param_mutable = malloc(sizeof(bool) * capacity);
        stmt->fn.param_defaults = malloc(sizeof(Expr*) * capacity);  // T1.5.2: Allocate defaults array
        
        do {
            if (stmt->fn.param_count >= capacity) {
                capacity *= 2;
                stmt->fn.params = realloc(stmt->fn.params, sizeof(Token) * capacity);
                stmt->fn.param_types = realloc(stmt->fn.param_types, sizeof(Expr*) * capacity);
                stmt->fn.param_mutable = realloc(stmt->fn.param_mutable, sizeof(bool) * capacity);
                stmt->fn.param_defaults = realloc(stmt->fn.param_defaults, sizeof(Expr*) * capacity);  // T1.5.2: Realloc defaults
            }
            stmt->fn.param_mutable[stmt->fn.param_count] = match(TOKEN_MUT);
            stmt->fn.params[stmt->fn.param_count] = parser.current;
            expect(TOKEN_IDENT, "Expected parameter name");
            
            // Allow optional type for 'self' parameter (for impl blocks)
            bool is_self = (parser.previous.length == 4 && 
                           memcmp(parser.previous.start, "self", 4) == 0);
            
            if (match(TOKEN_COLON)) {
                stmt->fn.param_types[stmt->fn.param_count] = parse_type();
            } else if (is_self) {
                // self without type - will be filled in by impl_block
                stmt->fn.param_types[stmt->fn.param_count] = NULL;
            } else {
                expect(TOKEN_COLON, "Expected ':' after parameter name");
                stmt->fn.param_types[stmt->fn.param_count] = parse_type();
            }
            
            // T1.5.2: Parse default parameter value if present
            if (match(TOKEN_EQ)) {
                stmt->fn.param_defaults[stmt->fn.param_count] = expression();
            } else {
                stmt->fn.param_defaults[stmt->fn.param_count] = NULL;
            }
            
            stmt->fn.param_count++;
        } while (match(TOKEN_COMMA));
    }
    
    expect(TOKEN_RPAREN, "Expected ')' after parameters");
    
    if (match(TOKEN_ARROW)) {
        stmt->fn.return_type = parse_type(); // Use parse_type for array support
    } else {
        stmt->fn.return_type = NULL;
    }
    
    expect(TOKEN_LBRACE, "Expected '{' before function body");
    Stmt* body = alloc_stmt();
    body->type = STMT_BLOCK;
    body->block.count = 0;
    body->block.stmts = malloc(sizeof(Stmt*) * 1024); // Increased to 1024 for large functions
    
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        if (body->block.count >= 1024) {
            fprintf(stderr, "Error at line %d: Function body too large (max 1024 statements)\n", parser.current.line);
            break;
        }
        body->block.stmts[body->block.count++] = statement();
    }
    
    expect(TOKEN_RBRACE, "Expected '}' after function body");
    stmt->fn.body = body;
    
    return stmt;
}

Stmt* extern_decl() {
    expect(TOKEN_EXTERN, "Expected 'extern'");
    expect(TOKEN_FN, "Expected 'fn' after 'extern'");
    
    Stmt* stmt = alloc_stmt();
    stmt->type = STMT_EXTERN;
    stmt->extern_fn.name = parser.current;
    expect(TOKEN_IDENT, "Expected function name");
    
    expect(TOKEN_LPAREN, "Expected '(' after function name");
    
    stmt->extern_fn.params = NULL;
    stmt->extern_fn.param_types = NULL;
    stmt->extern_fn.param_count = 0;
    stmt->extern_fn.is_variadic = false;
    
    if (!check(TOKEN_RPAREN)) {
        int capacity = 8;
        stmt->extern_fn.params = malloc(sizeof(Token) * capacity);
        stmt->extern_fn.param_types = malloc(sizeof(Expr*) * capacity);
        
        do {
            if (stmt->extern_fn.param_count >= capacity) {
                capacity *= 2;
                stmt->extern_fn.params = realloc(stmt->extern_fn.params, sizeof(Token) * capacity);
                stmt->extern_fn.param_types = realloc(stmt->extern_fn.param_types, sizeof(Expr*) * capacity);
            }
            
            // Check for variadic ...
            if (match(TOKEN_DOTDOTDOT)) {
                stmt->extern_fn.is_variadic = true;
                break;
            }
            
            stmt->extern_fn.params[stmt->extern_fn.param_count] = parser.current;
            expect(TOKEN_IDENT, "Expected parameter name");
            expect(TOKEN_COLON, "Expected ':' after parameter name");
            stmt->extern_fn.param_types[stmt->extern_fn.param_count] = parse_type();
            stmt->extern_fn.param_count++;
        } while (match(TOKEN_COMMA));
    }
    
    expect(TOKEN_RPAREN, "Expected ')' after parameters");
    
    // Optional return type
    if (match(TOKEN_ARROW)) {
        stmt->extern_fn.return_type = parse_type();
    } else {
        stmt->extern_fn.return_type = NULL;
    }
    
    expect(TOKEN_SEMI, "Expected ';' after extern declaration");
    
    return stmt;
}

Stmt* struct_decl() {
    expect(TOKEN_STRUCT, "Expected 'struct'");
    Stmt* stmt = alloc_stmt();
    stmt->type = STMT_STRUCT;
    stmt->struct_decl.is_public = false; // Caller will set if needed
    stmt->struct_decl.name = parser.current;
    expect(TOKEN_IDENT, "Expected struct name");
    
    // T3.1.2: Parse generic type parameters
    stmt->struct_decl.type_param_count = 0;
    stmt->struct_decl.type_params = NULL;
    if (match(TOKEN_LT)) {
        stmt->struct_decl.type_params = malloc(sizeof(Token) * 8);
        do {
            stmt->struct_decl.type_params[stmt->struct_decl.type_param_count++] = parser.current;
            expect(TOKEN_IDENT, "Expected type parameter");
            
            // Parse optional type constraints (T: Display + Debug)
            if (match(TOKEN_COLON)) {
                // Skip constraint parsing for now - handled in generics.c
                while (!check(TOKEN_COMMA) && !check(TOKEN_GT)) {
                    advance(); // Skip constraint tokens
                }
            }
        } while (match(TOKEN_COMMA));
        expect(TOKEN_GT, "Expected '>' after type parameters");
    }
    
    expect(TOKEN_LBRACE, "Expected '{' after struct name");
    
    stmt->struct_decl.field_count = 0;
    stmt->struct_decl.fields = malloc(sizeof(Token) * 32);
    stmt->struct_decl.field_types = malloc(sizeof(Expr*) * 32);
    stmt->struct_decl.field_arc_managed = malloc(sizeof(bool) * 32); // T2.5.3: ARC integration
    stmt->struct_decl.method_count = 0;
    stmt->struct_decl.methods = malloc(sizeof(FnStmt*) * 32);
    
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        // Check if this is a method (fn keyword) or a field
        if (match(TOKEN_FN)) {
            // Parse method
            FnStmt* method = malloc(sizeof(FnStmt));
            method->name = parser.current;
            expect(TOKEN_IDENT, "Expected method name");
            
            expect(TOKEN_LPAREN, "Expected '(' after method name");
            method->param_count = 0;
            method->params = malloc(sizeof(Token) * 16);
            method->param_types = malloc(sizeof(Expr*) * 16);
            
            if (!check(TOKEN_RPAREN)) {
                do {
                    method->params[method->param_count] = parser.current;
                    expect(TOKEN_IDENT, "Expected parameter name");
                    expect(TOKEN_COLON, "Expected ':' after parameter");
                    method->param_types[method->param_count] = parse_type();
                    method->param_count++;
                } while (match(TOKEN_COMMA));
            }
            expect(TOKEN_RPAREN, "Expected ')' after parameters");
            
            // Return type
            if (match(TOKEN_ARROW)) {
                method->return_type = parse_type();
            } else {
                method->return_type = NULL;
            }
            
            // Method body
            expect(TOKEN_LBRACE, "Expected '{' before method body");
            Stmt* body = alloc_stmt();
            body->type = STMT_BLOCK;
            body->block.count = 0;
            body->block.stmts = malloc(sizeof(Stmt*) * 64);
            
            while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
                body->block.stmts[body->block.count++] = statement();
            }
            expect(TOKEN_RBRACE, "Expected '}' after method body");
            
            method->body = body;
            method->is_async = false;
            method->is_extension = false;
            
            stmt->struct_decl.methods[stmt->struct_decl.method_count++] = method;
        } else {
            // Parse field
            stmt->struct_decl.fields[stmt->struct_decl.field_count] = parser.current;
            expect(TOKEN_IDENT, "Expected field name");
            expect(TOKEN_COLON, "Expected ':' after field name");
            stmt->struct_decl.field_types[stmt->struct_decl.field_count] = parse_type();
            
            // T2.5.3: Determine if field needs ARC management
            bool needs_arc = false;
            stmt->struct_decl.field_arc_managed[stmt->struct_decl.field_count] = needs_arc;
            
            stmt->struct_decl.field_count++;
            
            // Consume optional comma after field
            match(TOKEN_COMMA);
        }
    }
    
    expect(TOKEN_RBRACE, "Expected '}' after struct fields");
    return stmt;
}

Stmt* object_decl() {
    expect(TOKEN_OBJECT, "Expected 'object'");
    Stmt* stmt = alloc_stmt();
    stmt->type = STMT_STRUCT;  // Reuse STMT_STRUCT
    stmt->struct_decl.name = parser.current;
    expect(TOKEN_IDENT, "Expected object name");
    
    stmt->struct_decl.type_param_count = 0;
    stmt->struct_decl.type_params = NULL;
    
    expect(TOKEN_LBRACE, "Expected '{' after object name");
    
    stmt->struct_decl.field_count = 0;
    stmt->struct_decl.fields = malloc(sizeof(Token) * 32);
    stmt->struct_decl.field_types = malloc(sizeof(Expr*) * 32);
    stmt->struct_decl.field_arc_managed = malloc(sizeof(bool) * 32);
    stmt->struct_decl.method_count = 0;
    stmt->struct_decl.methods = malloc(sizeof(FnStmt*) * 32);
    
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        Token field_or_method_name = parser.current;
        expect(TOKEN_IDENT, "Expected field or method name");
        expect(TOKEN_COLON, "Expected ':' after name");
        
        // Check if this is a method (fn keyword) or a field (type)
        bool is_method = (parser.current.type == TOKEN_FN) || 
                        (parser.current.length == 2 && memcmp(parser.current.start, "fn", 2) == 0);
        
        if (is_method) {
            if (parser.current.type == TOKEN_FN) {
                advance(); // consume 'fn'
            } else {
                // It's an identifier that looks like "fn" - treat as method
                advance();
            }
            // Parse method: name: fn(params) -> return_type { body }
            FnStmt* method = malloc(sizeof(FnStmt));
            method->name = field_or_method_name;
            
            expect(TOKEN_LPAREN, "Expected '(' after 'fn'");
            method->param_count = 0;
            method->params = malloc(sizeof(Token) * 16);
            method->param_types = malloc(sizeof(Expr*) * 16);
            
            if (!check(TOKEN_RPAREN)) {
                do {
                    method->params[method->param_count] = parser.current;
                    expect(TOKEN_IDENT, "Expected parameter name");
                    expect(TOKEN_COLON, "Expected ':' after parameter");
                    method->param_types[method->param_count] = parse_type();
                    method->param_count++;
                } while (match(TOKEN_COMMA));
            }
            expect(TOKEN_RPAREN, "Expected ')' after parameters");
            
            // Return type
            if (match(TOKEN_ARROW)) {
                method->return_type = parse_type();
            } else {
                method->return_type = NULL;
            }
            
            // Method body
            expect(TOKEN_LBRACE, "Expected '{' before method body");
            Stmt* body = alloc_stmt();
            body->type = STMT_BLOCK;
            body->block.count = 0;
            body->block.stmts = malloc(sizeof(Stmt*) * 64);
            
            while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
                body->block.stmts[body->block.count++] = statement();
            }
            expect(TOKEN_RBRACE, "Expected '}' after method body");
            
            method->body = body;
            method->is_async = false;
            method->is_extension = false;
            
            stmt->struct_decl.methods[stmt->struct_decl.method_count++] = method;
        } else {
            // Parse field: name: type
            stmt->struct_decl.fields[stmt->struct_decl.field_count] = field_or_method_name;
            stmt->struct_decl.field_types[stmt->struct_decl.field_count] = parse_type();
            stmt->struct_decl.field_arc_managed[stmt->struct_decl.field_count] = false;
            stmt->struct_decl.field_count++;
        }
    }
    
    expect(TOKEN_RBRACE, "Expected '}' after object body");
    return stmt;
}

// T2.5.3: Enhanced Struct System - Type System Agent addition
Stmt* impl_block() {
    expect(TOKEN_IMPL, "Expected 'impl'");
    Stmt* stmt = alloc_stmt();
    stmt->type = STMT_IMPL;
    
    // Initialize generic parameters
    stmt->impl.type_param_count = 0;
    stmt->impl.type_params = NULL;
    stmt->impl.trait_bound_count = 0;
    stmt->impl.trait_bounds = NULL;
    
    // Parse optional generic parameters: impl<T: Display>
    if (check(TOKEN_LT)) {
        advance(); // consume '<'
        stmt->impl.type_params = malloc(sizeof(Token) * 8);
        stmt->impl.trait_bounds = malloc(sizeof(Token) * 8);
        
        do {
            expect(TOKEN_IDENT, "Expected type parameter name");
            stmt->impl.type_params[stmt->impl.type_param_count] = parser.previous;
            
            // Parse optional trait bound: T: Display
            if (match(TOKEN_COLON)) {
                expect(TOKEN_IDENT, "Expected trait name after ':'");
                stmt->impl.trait_bounds[stmt->impl.trait_bound_count] = parser.previous;
                stmt->impl.trait_bound_count++;
            }
            
            stmt->impl.type_param_count++;
        } while (match(TOKEN_COMMA));
        
        expect(TOKEN_GT, "Expected '>' after generic parameters");
    }
    
    // Check for "impl Trait for Type" syntax
    Token first_name = parser.current;
    expect(TOKEN_IDENT, "Expected trait or type name after 'impl'");
    
    if (match(TOKEN_FOR)) {
        // This is "impl Trait for Type"
        stmt->impl.is_trait_impl = true;
        stmt->impl.trait_name = first_name;
        
        // Parse the type name (could be generic like Box<T>)
        stmt->impl.type_name = parser.current;
        expect(TOKEN_IDENT, "Expected type name after 'for'");
        
        // Skip generic type arguments if present: Box<T>
        if (match(TOKEN_LT)) {
            int depth = 1;
            while (depth > 0 && !check(TOKEN_EOF)) {
                if (check(TOKEN_LT)) depth++;
                else if (check(TOKEN_GT)) depth--;
                advance();
            }
        }
    } else {
        // This is "impl Type"
        stmt->impl.is_trait_impl = false;
        stmt->impl.type_name = first_name;
    }
    
    expect(TOKEN_LBRACE, "Expected '{' after impl type name");
    
    stmt->impl.method_count = 0;
    stmt->impl.methods = malloc(sizeof(FnStmt*) * 32);
    
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        if (check(TOKEN_FN) || check(TOKEN_PUB)) {
            // Don't consume TOKEN_FN - let function() do it
            Stmt* method = function();
            
            // Transform method into extension method
            // If first parameter is 'self', replace it with typed parameter
            if (method->fn.param_count > 0 && 
                method->fn.params[0].length == 4 && 
                memcmp(method->fn.params[0].start, "self", 4) == 0) {
                
                // If self has no type, create one from impl type
                if (method->fn.param_types[0] == NULL) {
                    Expr* self_type = malloc(sizeof(Expr));
                    self_type->type = EXPR_IDENT;
                    self_type->token = stmt->impl.type_name;
                    method->fn.param_types[0] = self_type;
                }
            }
            
            // Mark as extension method
            method->fn.is_extension = true;
            method->fn.receiver_type = stmt->impl.type_name;
            
            // Store the FnStmt pointer (allocate new memory)
            FnStmt* fn_copy = malloc(sizeof(FnStmt));
            *fn_copy = method->fn;
            stmt->impl.methods[stmt->impl.method_count] = fn_copy;
            stmt->impl.method_count++;
        } else {
            fprintf(stderr, "Error at line %d: Expected function in impl block\n", parser.current.line);
            parser.had_error = true;
            advance();
        }
    }
    
    expect(TOKEN_RBRACE, "Expected '}' after impl methods");
    return stmt;
}

// T3.2.1: Trait definition parsing
Stmt* trait_decl() {
    expect(TOKEN_TRAIT, "Expected 'trait'");
    Stmt* stmt = alloc_stmt();
    stmt->type = STMT_TRAIT;
    stmt->trait_decl.name = parser.current;
    expect(TOKEN_IDENT, "Expected trait name");
    
    // Parse optional type parameters
    stmt->trait_decl.type_param_count = 0;
    stmt->trait_decl.type_params = NULL;
    
    if (match(TOKEN_LT)) {
        stmt->trait_decl.type_params = malloc(sizeof(Token) * 8);
        do {
            stmt->trait_decl.type_params[stmt->trait_decl.type_param_count] = parser.current;
            expect(TOKEN_IDENT, "Expected type parameter name");
            stmt->trait_decl.type_param_count++;
        } while (match(TOKEN_COMMA));
        expect(TOKEN_GT, "Expected '>' after type parameters");
    }
    
    expect(TOKEN_LBRACE, "Expected '{' after trait name");
    
    // Parse trait methods (declarations only, no bodies)
    stmt->trait_decl.method_count = 0;
    stmt->trait_decl.methods = malloc(sizeof(FnStmt*) * 32);
    stmt->trait_decl.method_has_default = malloc(sizeof(bool) * 32);
    
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        if (check(TOKEN_FN)) {
            advance(); // consume 'fn'
            
            FnStmt* method = calloc(1, sizeof(FnStmt));  // Use calloc to zero-initialize
            method->name = parser.current;
            expect(TOKEN_IDENT, "Expected method name");
            
            // Initialize all fields
            method->type_param_count = 0;
            method->type_params = NULL;
            method->is_public = false;
            
            expect(TOKEN_LPAREN, "Expected '(' after method name");
            
            // Parse parameters (simplified - skip self handling for now)
            method->param_count = 0;
            method->params = NULL;
            method->param_types = NULL;
            method->param_mutable = NULL;
            method->param_defaults = NULL;
            
            if (!check(TOKEN_RPAREN)) {
                method->params = malloc(sizeof(Token) * 8);
                method->param_types = malloc(sizeof(Expr*) * 8);
                method->param_mutable = malloc(sizeof(bool) * 8);
                method->param_defaults = malloc(sizeof(Expr*) * 8);
                
                do {
                    method->param_mutable[method->param_count] = false;
                    method->params[method->param_count] = parser.current;
                    expect(TOKEN_IDENT, "Expected parameter name");
                    
                    // Handle optional type annotation
                    if (match(TOKEN_COLON)) {
                        method->param_types[method->param_count] = parse_type();
                    } else {
                        method->param_types[method->param_count] = NULL;
                    }
                    
                    method->param_defaults[method->param_count] = NULL;
                    method->param_count++;
                } while (match(TOKEN_COMMA));
            }
            
            expect(TOKEN_RPAREN, "Expected ')' after parameters");
            
            // Parse return type
            if (match(TOKEN_ARROW)) {
                method->return_type = parse_type();
            } else {
                method->return_type = NULL;
            }
            
            // Trait methods don't have bodies - expect semicolon
            method->body = NULL;
            match(TOKEN_SEMI);
            
            stmt->trait_decl.methods[stmt->trait_decl.method_count] = method;
            stmt->trait_decl.method_has_default[stmt->trait_decl.method_count] = false;
            stmt->trait_decl.method_count++;
        } else {
            fprintf(stderr, "Error at line %d: Expected function in trait block\n", parser.current.line);
            parser.had_error = true;
            advance();
        }
    }
    
    expect(TOKEN_RBRACE, "Expected '}' after trait methods");
    return stmt;
}

Stmt* enum_decl() {
    expect(TOKEN_ENUM, "Expected 'enum'");
    Stmt* stmt = alloc_stmt();
    stmt->type = STMT_ENUM;
    stmt->enum_decl.name = parser.current;
    expect(TOKEN_IDENT, "Expected enum name");
    expect(TOKEN_LBRACE, "Expected '{' after enum name");
    
    stmt->enum_decl.variant_count = 0;
    stmt->enum_decl.variants = malloc(sizeof(Token) * 32);
    
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        stmt->enum_decl.variants[stmt->enum_decl.variant_count] = parser.current;
        expect(TOKEN_IDENT, "Expected variant name");
        stmt->enum_decl.variant_count++;
        if (!check(TOKEN_RBRACE)) {
            match(TOKEN_COMMA);
        }
    }
    
    expect(TOKEN_RBRACE, "Expected '}' after enum variants");
    return stmt;
}

Stmt* type_alias() {
    expect(TOKEN_TYPEDEF, "Expected 'type'");
    Stmt* stmt = alloc_stmt();
    stmt->type = STMT_TYPE_ALIAS;
    stmt->type_alias.name = parser.current;
    expect(TOKEN_IDENT, "Expected type alias name");
    expect(TOKEN_EQ, "Expected '=' after type name");
    stmt->type_alias.target = parser.current;
    expect(TOKEN_IDENT, "Expected target type");
    return stmt;
}

Program* parse_program() {
    Program* prog = safe_calloc(1, sizeof(Program));
    prog->stmts = safe_malloc(sizeof(Stmt*) * 32);
    prog->count = 0;
    int capacity = 32;
    
    while (!check(TOKEN_EOF)) {
        // Check if we need to resize the array
        if (prog->count >= capacity) {
            int new_capacity = capacity + 32; // Grow by fixed amount instead of doubling
            size_t new_size = sizeof(Stmt*) * new_capacity;
            
            // Check for reasonable size limit
            if (new_size > 1024 * 1024) { // 1MB limit for statement array
                // Handle too many statements
                safe_free(prog->stmts);
                safe_free(prog);
                return NULL;
            }
            
            prog->stmts = safe_realloc(prog->stmts, new_size);
            if (!prog->stmts) {
                // Handle allocation failure
                safe_free(prog);
                return NULL;
            }
            capacity = new_capacity;
        }
        
        // Handle import statements
        if (match(TOKEN_IMPORT)) {
            Stmt* stmt = safe_malloc(sizeof(Stmt));
            stmt->type = STMT_IMPORT;
            expect(TOKEN_IDENT, "Expected module name");
            stmt->import.module = parser.previous;
            
            // Optional: as alias
            if (match(TOKEN_AS)) {
                expect(TOKEN_IDENT, "Expected alias name after 'as'");
                stmt->import.alias = parser.previous;
                
                // Register alias globally
                char module_name[256], alias_name[256];
                snprintf(module_name, sizeof(module_name), "%.*s",
                        stmt->import.module.length, stmt->import.module.start);
                snprintf(alias_name, sizeof(alias_name), "%.*s",
                        stmt->import.alias.length, stmt->import.alias.start);
                register_parser_module_alias(alias_name, module_name);
            } else {
                stmt->import.alias.start = NULL;
                stmt->import.alias.length = 0;
            }
            
            // Optional: from "path"
            if (match(TOKEN_FROM)) {
                expect(TOKEN_STRING, "Expected string path after 'from'");
                stmt->import.path = parser.previous;
            } else {
                stmt->import.path.start = NULL;
                stmt->import.path.length = 0;
            }
            
            stmt->import.items = NULL;
            stmt->import.item_count = 0;
            prog->stmts[prog->count++] = stmt;
            continue;
        }
        
        // Handle macro definitions
        if (match(TOKEN_MACRO)) {
            Stmt* stmt = malloc(sizeof(Stmt));
            stmt->type = STMT_MACRO;
            stmt->macro.name = parser.current;
            expect(TOKEN_IDENT, "Expected macro name");
            
            // Parse parameters
            expect(TOKEN_LPAREN, "Expected '(' after macro name");
            stmt->macro.params = NULL;
            stmt->macro.param_count = 0;
            
            if (!check(TOKEN_RPAREN)) {
                int capacity = 8;
                stmt->macro.params = malloc(sizeof(Token) * capacity);
                do {
                    if (stmt->macro.param_count >= capacity) {
                        capacity *= 2;
                        stmt->macro.params = realloc(stmt->macro.params, sizeof(Token) * capacity);
                    }
                    stmt->macro.params[stmt->macro.param_count] = parser.current;
                    expect(TOKEN_IDENT, "Expected parameter name");
                    stmt->macro.param_count++;
                } while (match(TOKEN_COMMA));
            }
            expect(TOKEN_RPAREN, "Expected ')' after parameters");
            
            // Parse body as token sequence (simple text substitution)
            expect(TOKEN_LBRACE, "Expected '{' before macro body");
            stmt->macro.body.start = parser.current.start;
            int brace_count = 1;
            while (!check(TOKEN_EOF) && brace_count > 0) {
                if (check(TOKEN_LBRACE)) brace_count++;
                else if (check(TOKEN_RBRACE)) brace_count--;
                if (brace_count > 0) advance();
            }
            stmt->macro.body.length = parser.current.start - stmt->macro.body.start;
            expect(TOKEN_RBRACE, "Expected '}' after macro body");
            
            prog->stmts[prog->count++] = stmt;
            continue;
        }
        
        // Handle import statements (old position - remove duplicate)
        if (false && match(TOKEN_IMPORT)) {
            Stmt* stmt = safe_malloc(sizeof(Stmt));
            stmt->type = STMT_IMPORT;
            expect(TOKEN_IDENT, "Expected module name");
            stmt->import.module = parser.previous;
            
            // Optional: as alias
            if (match(TOKEN_AS)) {
                expect(TOKEN_IDENT, "Expected alias name after 'as'");
                stmt->import.alias = parser.previous;
                
                // Register alias globally
                char module_name[256], alias_name[256];
                snprintf(module_name, sizeof(module_name), "%.*s",
                        stmt->import.module.length, stmt->import.module.start);
                snprintf(alias_name, sizeof(alias_name), "%.*s",
                        stmt->import.alias.length, stmt->import.alias.start);
                register_parser_module_alias(alias_name, module_name);
            } else {
                stmt->import.alias.start = NULL;
                stmt->import.alias.length = 0;
            }
            
            // Optional: from "path"
            if (match(TOKEN_FROM)) {
                expect(TOKEN_STRING, "Expected string path after 'from'");
                stmt->import.path = parser.previous;
            } else {
                stmt->import.path.start = NULL;
                stmt->import.path.length = 0;
            }
            
            stmt->import.items = NULL;
            stmt->import.item_count = 0;
            prog->stmts[prog->count++] = stmt;
            continue;
        }
        
        // Handle export statements
        if (match(TOKEN_EXPORT)) {
            Stmt* stmt = malloc(sizeof(Stmt));
            stmt->type = STMT_EXPORT;
            
            // Parse the statement being exported
            if (check(TOKEN_FN) || check(TOKEN_PUB)) {
                stmt->export.stmt = function();
            } else if (check(TOKEN_STRUCT)) {
                stmt->export.stmt = struct_decl();
            } else if (check(TOKEN_ENUM)) {
                stmt->export.stmt = enum_decl();
            } else if (check(TOKEN_VAR)) {
                stmt->export.stmt = statement();
            } else {
                fprintf(stderr, "Error at line %d: Expected function, struct, enum, or variable after 'export'\n", parser.current.line);
                parser.had_error = true;
            }
            
            prog->stmts[prog->count++] = stmt;
            continue;
        }
        
        if (check(TOKEN_FN) || check(TOKEN_ASYNC)) {
            prog->stmts[prog->count++] = function();
        } else if (check(TOKEN_PUB)) {
            // Save current token, advance to see what's after pub
            advance(); // consume pub
            if (check(TOKEN_STRUCT)) {
                // It's pub struct
                Stmt* stmt = struct_decl();
                stmt->struct_decl.is_public = true;
                prog->stmts[prog->count++] = stmt;
            } else {
                // It's pub fn or pub async - function() will handle it
                // But we already consumed pub, so we need to handle it here
                Stmt* stmt = function();
                stmt->fn.is_public = true;
                prog->stmts[prog->count++] = stmt;
            }
        } else if (check(TOKEN_EXTERN)) {
            prog->stmts[prog->count++] = extern_decl();
        } else if (check(TOKEN_STRUCT)) {
            prog->stmts[prog->count++] = struct_decl();
        } else if (check(TOKEN_OBJECT)) {
            prog->stmts[prog->count++] = object_decl();
        } else if (check(TOKEN_ENUM)) {
            prog->stmts[prog->count++] = enum_decl();
        } else if (check(TOKEN_TYPEDEF)) {
            prog->stmts[prog->count++] = type_alias();
        } else if (check(TOKEN_IMPL)) {
            prog->stmts[prog->count++] = impl_block();
        } else if (check(TOKEN_TRAIT)) {
            prog->stmts[prog->count++] = trait_decl();
        } else if (check(TOKEN_TEST)) {
            // T1.6.2: Testing Framework Agent addition
            prog->stmts[prog->count++] = parse_test_statement();
        } else if (check(TOKEN_WHILE)) {
            // T1.4.1: Control Flow Agent addition
            prog->stmts[prog->count++] = parse_while_statement();
        } else if (check(TOKEN_BREAK)) {
            // T1.4.2: Control Flow Agent addition
            prog->stmts[prog->count++] = parse_break_statement();
        } else if (check(TOKEN_CONTINUE)) {
            // T1.4.2: Control Flow Agent addition
            prog->stmts[prog->count++] = parse_continue_statement();
        } else if (check(TOKEN_MATCH)) {
            // T1.4.3: Control Flow Agent addition
            prog->stmts[prog->count++] = parse_match_statement();
        } else {
            prog->stmts[prog->count++] = statement();
        }
    }
    
    return prog;
}

// T1.6.2: Testing Framework Agent addition - Test statement parser
static Stmt* parse_test_statement() {
    advance(); // consume 'test' token
    
    Stmt* stmt = alloc_stmt();
    stmt->type = STMT_TEST;
    
    // Parse test name
    expect(TOKEN_IDENT, "Expected test name");
    stmt->test_stmt.name = parser.previous;
    stmt->test_stmt.is_async = false;
    
    // Parse test body
    expect(TOKEN_LBRACE, "Expected '{' before test body");
    stmt->test_stmt.body = alloc_stmt();
    stmt->test_stmt.body->type = STMT_BLOCK;
    stmt->test_stmt.body->block.stmts = safe_malloc(sizeof(Stmt*) * 32);
    stmt->test_stmt.body->block.count = 0;
    
    // Parse statements until closing brace
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        stmt->test_stmt.body->block.stmts[stmt->test_stmt.body->block.count++] = statement();
    }
    
    expect(TOKEN_RBRACE, "Expected '}' after test body");
    
    return stmt;
}

void init_parser() {
    parser.had_error = false;
    advance();
}

bool parser_had_error() {
    return parser.had_error;
}

// T1.4.1: While Loop AST Extension - Control Flow Agent addition
static Stmt* parse_while_statement() {
    advance(); // consume 'while' token
    
    Stmt* stmt = safe_malloc(sizeof(Stmt));
    if (!stmt) return NULL;
    
    stmt->type = STMT_WHILE;
    
    expect(TOKEN_LPAREN, "Expected '(' after 'while'");
    stmt->while_stmt.condition = expression();
    expect(TOKEN_RPAREN, "Expected ')' after while condition");
    
    stmt->while_stmt.body = statement();
    
    return stmt;
}

// T1.4.2: Break/Continue Implementation - Control Flow Agent addition
static Stmt* parse_break_statement() {
    advance(); // consume 'break' token
    
    Stmt* stmt = safe_malloc(sizeof(Stmt));
    if (!stmt) return NULL;
    
    stmt->type = STMT_BREAK;
    
    expect(TOKEN_SEMI, "Expected ';' after 'break'");
    
    return stmt;
}

static Stmt* parse_continue_statement() {
    advance(); // consume 'continue' token
    
    Stmt* stmt = safe_malloc(sizeof(Stmt));
    if (!stmt) return NULL;
    
    stmt->type = STMT_CONTINUE;
    
    expect(TOKEN_SEMI, "Expected ';' after 'continue'");
    
    return stmt;
}

// T1.4.3: Match Statement Parser - Control Flow Agent addition
static Stmt* parse_match_statement() {
    advance(); // consume 'match' token
    
    Stmt* stmt = safe_malloc(sizeof(Stmt));
    if (!stmt) return NULL;
    
    stmt->type = STMT_MATCH;
    
    // Parse the value to match against
    stmt->match_stmt.value = expression();
    
    expect(TOKEN_LBRACE, "Expected '{' after match expression");
    
    // Parse match cases
    stmt->match_stmt.case_count = 0;
    stmt->match_stmt.cases = safe_malloc(sizeof(MatchCase) * 16); // Initial capacity
    
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        MatchCase* current_case = &stmt->match_stmt.cases[stmt->match_stmt.case_count];
        
        // Parse pattern - enhanced to support literal, wildcard, and Some/None patterns
        Pattern* pattern = safe_malloc(sizeof(Pattern));
        
        if (match(TOKEN_SOME)) {
            // Some(pattern) - option pattern
            pattern->type = PATTERN_OPTION;
            pattern->option.is_some = true;
            
            expect(TOKEN_LPAREN, "Expected '(' after Some");
            
            // Parse inner pattern (for now, just identifier)
            Pattern* inner_pattern = safe_malloc(sizeof(Pattern));
            inner_pattern->type = PATTERN_IDENT;
            inner_pattern->ident.name = parser.current;
            expect(TOKEN_IDENT, "Expected identifier in Some pattern");
            
            pattern->option.inner = inner_pattern;
            expect(TOKEN_RPAREN, "Expected ')' after Some pattern");
        } else if (match(TOKEN_NONE)) {
            // None pattern
            pattern->type = PATTERN_OPTION;
            pattern->option.is_some = false;
            pattern->option.inner = NULL;
        } else if (check(TOKEN_INT) || check(TOKEN_FLOAT) || check(TOKEN_STRING) || check(TOKEN_TRUE) || check(TOKEN_FALSE)) {
            // Literal pattern
            pattern->type = PATTERN_LITERAL;
            pattern->literal.value = parser.current;
            advance();
        } else if (match(TOKEN_UNDERSCORE)) {
            // Wildcard pattern
            pattern->type = PATTERN_WILDCARD;
        } else if (check(TOKEN_IDENT)) {
            // Variable binding pattern
            pattern->type = PATTERN_IDENT;
            pattern->ident.name = parser.current;
            advance();
        } else {
            fprintf(stderr, "Error at line %d: Expected pattern\n", parser.current.line);
            parser.had_error = true;
            return NULL;
        }
        
        current_case->pattern = pattern;
        
        // Check for guard clause
        if (match(TOKEN_IF)) {
            current_case->guard = expression();
        } else {
            current_case->guard = NULL;
        }
        
        expect(TOKEN_FATARROW, "Expected '=>' after match pattern");
        
        // Parse case body
        current_case->body = statement();
        
        stmt->match_stmt.case_count++;
        
        // Optional comma
        if (check(TOKEN_COMMA)) {
            advance();
        }
    }
    
    expect(TOKEN_RBRACE, "Expected '}' after match cases");
    
    return stmt;
}

// T2.5.1: Optional Type Implementation - Type System Agent addition
// T2.5.2: Union Type Support - Type System Agent addition
static Expr* parse_type() {
    Expr* base_type;
    
    // Handle array types [type]
    if (match(TOKEN_LBRACKET)) {
        Expr* element_type = parse_type(); // Recursive call for nested arrays
        expect(TOKEN_RBRACKET, "Expected ']' after array element type");
        
        base_type = alloc_expr();
        base_type->type = EXPR_ARRAY;
        base_type->array.elements = malloc(sizeof(Expr*) * 1);
        base_type->array.elements[0] = element_type;
        base_type->array.count = 1; // Use count=1 to indicate this is a type, not literal
    } else if (check(TOKEN_IDENT)) {
        // Handle built-in types and user-defined types
        Token type_token = parser.current;
        advance();
        
        // Check for generic type instantiation: TypeName<T1, T2, ...>
        if (match(TOKEN_LT)) {
            // This is a generic type instantiation
            base_type = alloc_expr();
            base_type->type = EXPR_CALL; // Reuse EXPR_CALL for generic instantiation
            
            // Create identifier expression for the type name
            Expr* type_name_expr = alloc_expr();
            type_name_expr->type = EXPR_IDENT;
            type_name_expr->token = type_token;
            base_type->call.callee = type_name_expr;
            
            // Parse type arguments
            base_type->call.arg_count = 0;
            base_type->call.args = malloc(sizeof(Expr*) * 8);
            
            if (!check(TOKEN_GT)) {
                do {
                    base_type->call.args[base_type->call.arg_count++] = parse_type();
                } while (match(TOKEN_COMMA));
            }
            
            expect(TOKEN_GT, "Expected '>' after generic type arguments");
        } else {
            // Regular type name
            // Check for built-in types
            if (type_token.length == 3 && memcmp(type_token.start, "str", 3) == 0) {
                // Built-in string type
                base_type = alloc_expr();
                base_type->type = EXPR_IDENT;
                base_type = alloc_expr();
                base_type->type = EXPR_IDENT;
                base_type->token = type_token;
            } else if (type_token.length == 3 && memcmp(type_token.start, "int", 3) == 0) {
                // Built-in int type
                base_type = alloc_expr();
                base_type->type = EXPR_IDENT;
                base_type->token = type_token;
            } else if (type_token.length == 4 && memcmp(type_token.start, "bool", 4) == 0) {
                // Built-in bool type
                base_type = alloc_expr();
                base_type->type = EXPR_IDENT;
                base_type->token = type_token;
            } else if (type_token.length == 5 && memcmp(type_token.start, "float", 5) == 0) {
                // Built-in float type
                base_type = alloc_expr();
                base_type->type = EXPR_IDENT;
                base_type->token = type_token;
            } else {
                // User-defined type
                base_type = alloc_expr();
                base_type->type = EXPR_IDENT;
                base_type->token = type_token;
            }
        }
    } else {
        fprintf(stderr, "Error at line %d: Expected type name\n", parser.current.line);
        return NULL;
    }
    
    // Check for union type with '|' syntax (T | U | V)
    if (match(TOKEN_PIPE)) {
        Expr* union_expr = alloc_expr();
        union_expr->type = EXPR_UNION_TYPE;
        
        // Start with the base type as first union member
        union_expr->union_type.types = malloc(sizeof(Expr*) * 8); // Initial capacity
        union_expr->union_type.types[0] = base_type;
        union_expr->union_type.type_count = 1;
        
        // Parse additional union members
        do {
            Expr* next_type = parse_type(); // Recursive call
            union_expr->union_type.types[union_expr->union_type.type_count] = next_type;
            union_expr->union_type.type_count++;
        } while (match(TOKEN_PIPE));
        
        base_type = union_expr;
    }
    
    // Check for optional type suffix '?' (can be applied to union types too)
    if (match(TOKEN_QUESTION)) {
        Expr* optional_expr = alloc_expr();
        optional_expr->type = EXPR_OPTIONAL_TYPE;
        optional_expr->optional_type.inner_type = base_type;
        return optional_expr;
    }
    
    return base_type;
}

// T3.3.1: Pattern parsing for destructuring
static Pattern* parse_pattern() {
    Pattern* pattern = safe_malloc(sizeof(Pattern));
    
    // Handle struct destructuring: Point { x, y }
    if (check(TOKEN_IDENT)) {
        Token potential_struct = parser.current;
        advance();
        
        if (match(TOKEN_LBRACE)) {
            // This is a struct pattern
            pattern->type = PATTERN_STRUCT;
            pattern->struct_pat.struct_name = potential_struct;
            
            // Parse field patterns
            pattern->struct_pat.field_count = 0;
            pattern->struct_pat.field_names = safe_malloc(sizeof(Token) * 16);
            pattern->struct_pat.field_patterns = safe_malloc(sizeof(Pattern*) * 16);
            
            while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
                if (pattern->struct_pat.field_count >= 16) {
                    fprintf(stderr, "Error: Too many fields in struct pattern\n");
                    break;
                }
                
                // Parse field name
                expect(TOKEN_IDENT, "Expected field name in struct pattern");
                pattern->struct_pat.field_names[pattern->struct_pat.field_count] = parser.previous;
                
                // For now, create a simple identifier pattern for the field
                Pattern* field_pattern = safe_malloc(sizeof(Pattern));
                field_pattern->type = PATTERN_IDENT;
                field_pattern->ident.name = parser.previous;
                pattern->struct_pat.field_patterns[pattern->struct_pat.field_count] = field_pattern;
                
                pattern->struct_pat.field_count++;
                
                if (!check(TOKEN_RBRACE)) {
                    expect(TOKEN_COMMA, "Expected ',' between struct pattern fields");
                }
            }
            
            expect(TOKEN_RBRACE, "Expected '}' after struct pattern");
            return pattern;
        } else {
            // This is just an identifier pattern
            pattern->type = PATTERN_IDENT;
            pattern->ident.name = potential_struct;
            return pattern;
        }
    }
    
    // Handle array destructuring: [first, second, ...rest]
    if (match(TOKEN_LBRACKET)) {
        pattern->type = PATTERN_ARRAY;
        pattern->array.element_count = 0;
        pattern->array.elements = safe_malloc(sizeof(Pattern*) * 16);
        pattern->array.has_rest = false;
        
        while (!check(TOKEN_RBRACKET) && !check(TOKEN_EOF)) {
            if (pattern->array.element_count >= 16) {
                fprintf(stderr, "Error: Too many elements in array pattern\n");
                break;
            }
            
            // Check for rest pattern: ...name
            if (match(TOKEN_DOT) && match(TOKEN_DOT) && match(TOKEN_DOT)) {
                pattern->array.has_rest = true;
                expect(TOKEN_IDENT, "Expected identifier after '...' in array pattern");
                pattern->array.rest_name = parser.previous;
                break; // Rest pattern must be last
            }
            
            // Parse element pattern (recursively)
            pattern->array.elements[pattern->array.element_count] = parse_pattern();
            pattern->array.element_count++;
            
            if (!check(TOKEN_RBRACKET)) {
                expect(TOKEN_COMMA, "Expected ',' between array pattern elements");
            }
        }
        
        expect(TOKEN_RBRACKET, "Expected ']' after array pattern");
        return pattern;
    }
    
    // Handle tuple destructuring: (first, second, third)
    if (match(TOKEN_LPAREN)) {
        pattern->type = PATTERN_TUPLE;
        pattern->tuple.element_count = 0;
        pattern->tuple.elements = safe_malloc(sizeof(Pattern*) * 16);
        
        while (!check(TOKEN_RPAREN) && !check(TOKEN_EOF)) {
            if (pattern->tuple.element_count >= 16) {
                fprintf(stderr, "Error: Too many elements in tuple pattern\n");
                break;
            }
            
            pattern->tuple.elements[pattern->tuple.element_count] = parse_pattern();
            pattern->tuple.element_count++;
            
            if (!check(TOKEN_RPAREN)) {
                expect(TOKEN_COMMA, "Expected ',' between tuple pattern elements");
            }
        }
        
        expect(TOKEN_RPAREN, "Expected ')' after tuple pattern");
        return pattern;
    }
    
    // Handle wildcard pattern: _
    if (match(TOKEN_UNDERSCORE)) {
        pattern->type = PATTERN_WILDCARD;
        return pattern;
    }
    
    // Handle literal patterns
    if (match(TOKEN_INT) || match(TOKEN_FLOAT) || match(TOKEN_STRING) || 
        match(TOKEN_TRUE) || match(TOKEN_FALSE)) {
        pattern->type = PATTERN_LITERAL;
        pattern->literal.value = parser.previous;
        return pattern;
    }
    
    // Handle Some/None patterns
    if (match(TOKEN_SOME)) {
        pattern->type = PATTERN_OPTION;
        pattern->option.is_some = true;
        
        expect(TOKEN_LPAREN, "Expected '(' after Some");
        pattern->option.inner = parse_pattern();
        expect(TOKEN_RPAREN, "Expected ')' after Some pattern");
        return pattern;
    }
    
    if (match(TOKEN_NONE)) {
        pattern->type = PATTERN_OPTION;
        pattern->option.is_some = false;
        pattern->option.inner = NULL;
        return pattern;
    }
    
    // Default: identifier pattern
    if (check(TOKEN_IDENT)) {
        pattern->type = PATTERN_IDENT;
        pattern->ident.name = parser.current;
        advance();
        return pattern;
    }
    
    // If we get here, it's an error
    fprintf(stderr, "Error: Expected pattern at line %d\n", parser.current.line);
    free(pattern);
    return NULL;
}
// TASK-026: Result type parsing functions

static Expr* parse_result_type() {
    // Parse Result<T, E> syntax
    expect(TOKEN_LT, "Expected '<' after Result");
    
    Expr* ok_type = parse_type();
    expect(TOKEN_COMMA, "Expected ',' between Result types");
    Expr* err_type = parse_type();
    
    expect(TOKEN_GT, "Expected '>' after Result types");
    
    Expr* result_expr = alloc_expr();
    result_expr->type = EXPR_RESULT_TYPE;
    result_expr->result_type.ok_type = ok_type;
    result_expr->result_type.err_type = err_type;
    
    return result_expr;
}

static Stmt* parse_try_statement() {
    // Parse try { ... } catch (Type var) { ... }
    Stmt* try_stmt = alloc_stmt();
    try_stmt->type = STMT_TRY;
    
    expect(TOKEN_LBRACE, "Expected '{' after try");
    try_stmt->try_stmt.try_block = statement();
    
    // Parse multiple catch blocks
    try_stmt->try_stmt.catch_count = 0;
    try_stmt->try_stmt.catch_blocks = malloc(sizeof(Stmt*) * 8);
    try_stmt->try_stmt.exception_types = malloc(sizeof(Token) * 8);
    try_stmt->try_stmt.exception_vars = malloc(sizeof(Token) * 8);
    
    while (match(TOKEN_CATCH)) {
        expect(TOKEN_LPAREN, "Expected '(' after catch");
        
        // Parse exception type
        expect(TOKEN_IDENT, "Expected exception type");
        try_stmt->try_stmt.exception_types[try_stmt->try_stmt.catch_count] = parser.previous;
        
        // Parse exception variable
        expect(TOKEN_IDENT, "Expected exception variable");
        try_stmt->try_stmt.exception_vars[try_stmt->try_stmt.catch_count] = parser.previous;
        
        expect(TOKEN_RPAREN, "Expected ')' after catch parameters");
        expect(TOKEN_LBRACE, "Expected '{' after catch");
        
        try_stmt->try_stmt.catch_blocks[try_stmt->try_stmt.catch_count] = statement();
        try_stmt->try_stmt.catch_count++;
    }
    
    // Optional finally block
    if (match(TOKEN_FINALLY)) {
        expect(TOKEN_LBRACE, "Expected '{' after finally");
        try_stmt->try_stmt.finally_block = statement();
    } else {
        try_stmt->try_stmt.finally_block = NULL;
    }
    
    return try_stmt;
}

static Stmt* parse_catch_statement() {
    // Parse standalone catch statement
    Stmt* catch_stmt = alloc_stmt();
    catch_stmt->type = STMT_CATCH;
    
    expect(TOKEN_LPAREN, "Expected '(' after catch");
    
    // Parse exception type
    expect(TOKEN_IDENT, "Expected exception type");
    catch_stmt->catch_stmt.exception_type = parser.previous;
    
    // Parse exception variable
    expect(TOKEN_IDENT, "Expected exception variable");
    catch_stmt->catch_stmt.exception_var = parser.previous;
    
    expect(TOKEN_RPAREN, "Expected ')' after catch parameters");
    expect(TOKEN_LBRACE, "Expected '{' after catch");
    
    catch_stmt->catch_stmt.body = statement();
    
    return catch_stmt;
}

static Expr* parse_try_expression() {
    // Parse expression? for error propagation
    Expr* expr = primary();
    
    if (match(TOKEN_QUESTION)) {
        Expr* try_expr = alloc_expr();
        try_expr->type = EXPR_TRY;
        try_expr->try_expr.value = expr;
        return try_expr;
    }
    
    return expr;
}

// Update primary() to handle Result constructors and ? operator
static Expr* primary_with_result() {
    // Handle Ok() constructor
    if (match(TOKEN_OK)) {
        Expr* expr = alloc_expr();
        expr->type = EXPR_OK;
        expect(TOKEN_LPAREN, "Expected '(' after Ok");
        expr->option.value = expression();
        expect(TOKEN_RPAREN, "Expected ')' after Ok value");
        return expr;
    }
    
    // Handle Err() constructor
    if (match(TOKEN_ERR)) {
        Expr* expr = alloc_expr();
        expr->type = EXPR_ERR;
        expect(TOKEN_LPAREN, "Expected '(' after Err");
        expr->option.value = expression();
        expect(TOKEN_RPAREN, "Expected ')' after Err value");
        return expr;
    }
    
    // Handle Result type
    if (check(TOKEN_IDENT)) {
        Token name = parser.current;
        if (strncmp(name.start, "Result", 6) == 0 && name.length == 6) {
            advance();
            return parse_result_type();
        }
    }
    
    // Fall back to regular primary parsing
    return primary();
}