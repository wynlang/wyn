// T3.3.1: Destructuring Patterns Test Program
// Test pattern matching and destructuring functionality

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

// Simple add_symbol implementation for testing
void add_symbol(SymbolTable* scope, Token name, Type* type, bool is_mutable) {
    // Simplified implementation for testing
    if (!scope) return;
    printf("   Binding variable: %.*s\n", (int)name.length, name.start);
}

// Test destructuring patterns functionality
int main() {
    printf("=== T3.3.1: Destructuring Patterns Test ===\n");
    
    // Initialize pattern system
    wyn_patterns_init();
    
    // Test pattern creation
    printf("1. Testing pattern creation...\n");
    
    // Create literal pattern
    Token int_token = {TOKEN_INT, "42", 2, 0};
    Pattern* literal_pat = wyn_create_literal_pattern(int_token);
    printf("   Created literal pattern: %s\n", literal_pat ? "Success" : "Failed");
    
    // Create identifier pattern
    Token var_token = {TOKEN_IDENT, "x", 1, 0};
    Pattern* ident_pat = wyn_create_ident_pattern(var_token);
    printf("   Created identifier pattern: %s\n", ident_pat ? "Success" : "Failed");
    
    // Create wildcard pattern
    Pattern* wildcard_pat = wyn_create_wildcard_pattern();
    printf("   Created wildcard pattern: %s\n", wildcard_pat ? "Success" : "Failed");
    
    // Create struct pattern
    Token struct_name = {TOKEN_IDENT, "Point", 5, 0};
    Token field_names[2] = {
        {TOKEN_IDENT, "x", 1, 0},
        {TOKEN_IDENT, "y", 1, 0}
    };
    Pattern* field_pat1 = wyn_create_ident_pattern(field_names[0]);
    Pattern* field_pat2 = wyn_create_ident_pattern(field_names[1]);
    Pattern* field_patterns[2] = {field_pat1, field_pat2};
    Pattern* struct_pat = wyn_create_struct_pattern(struct_name, field_names, field_patterns, 2);
    printf("   Created struct pattern: %s\n", struct_pat ? "Success" : "Failed");
    
    // Create array pattern
    Pattern* array_elem1 = wyn_create_ident_pattern(var_token);
    Pattern* array_elem2 = wyn_create_wildcard_pattern();
    Pattern* array_elements[2] = {array_elem1, array_elem2};
    Token rest_name = {TOKEN_IDENT, "rest", 4, 0};
    Pattern* array_pat = wyn_create_array_pattern(array_elements, 2, true, rest_name);
    printf("   Created array pattern: %s\n", array_pat ? "Success" : "Failed");
    
    // Create tuple pattern
    Pattern* tuple_elem1 = wyn_create_ident_pattern(var_token);
    Pattern* tuple_elem2 = wyn_create_literal_pattern(int_token);
    Pattern* tuple_elem3 = wyn_create_wildcard_pattern();
    Pattern* tuple_elements[3] = {tuple_elem1, tuple_elem2, tuple_elem3};
    Pattern* tuple_pat = wyn_create_tuple_pattern(tuple_elements, 3);
    printf("   Created tuple pattern: %s\n", tuple_pat ? "Success" : "Failed");
    
    // Create option pattern
    Pattern* some_inner = wyn_create_ident_pattern(var_token);
    Pattern* some_pat = wyn_create_option_pattern(some_inner, true);
    Pattern* none_pat = wyn_create_option_pattern(NULL, false);
    printf("   Created Some pattern: %s\n", some_pat ? "Success" : "Failed");
    printf("   Created None pattern: %s\n", none_pat ? "Success" : "Failed");
    
    // Test pattern matching
    printf("2. Testing pattern matching...\n");
    
    // Create test expressions
    Expr int_expr = {EXPR_INT};
    Expr struct_expr = {EXPR_STRUCT_INIT};
    Expr array_expr = {EXPR_ARRAY};
    Expr some_expr = {EXPR_SOME};
    
    // Test matches
    bool literal_matches = wyn_pattern_matches(literal_pat, &int_expr, NULL);
    bool ident_matches = wyn_pattern_matches(ident_pat, &int_expr, NULL);
    bool wildcard_matches = wyn_pattern_matches(wildcard_pat, &int_expr, NULL);
    bool struct_matches = wyn_pattern_matches(struct_pat, &struct_expr, NULL);
    bool array_matches = wyn_pattern_matches(array_pat, &array_expr, NULL);
    bool some_matches = wyn_pattern_matches(some_pat, &some_expr, NULL);
    
    printf("   Literal pattern matches int: %s\n", literal_matches ? "Yes" : "No");
    printf("   Identifier pattern matches int: %s\n", ident_matches ? "Yes" : "No");
    printf("   Wildcard pattern matches int: %s\n", wildcard_matches ? "Yes" : "No");
    printf("   Struct pattern matches struct: %s\n", struct_matches ? "Yes" : "No");
    printf("   Array pattern matches array: %s\n", array_matches ? "Yes" : "No");
    printf("   Some pattern matches Some: %s\n", some_matches ? "Yes" : "No");
    
    // Test pattern binding extraction
    printf("3. Testing pattern binding extraction...\n");
    SymbolTable scope = {0};
    
    wyn_extract_pattern_bindings(ident_pat, &int_expr, &scope);
    wyn_extract_pattern_bindings(struct_pat, &struct_expr, &scope);
    wyn_extract_pattern_bindings(array_pat, &array_expr, &scope);
    
    // Test exhaustiveness checking
    printf("4. Testing exhaustiveness checking...\n");
    Pattern* patterns_with_wildcard[] = {literal_pat, wildcard_pat};
    Pattern* patterns_without_wildcard[] = {literal_pat, ident_pat};
    
    Type* int_type = make_type(TYPE_INT);
    bool exhaustive_with = wyn_is_pattern_exhaustive(patterns_with_wildcard, 2, int_type);
    bool exhaustive_without = wyn_is_pattern_exhaustive(patterns_without_wildcard, 2, int_type);
    
    printf("   Patterns with wildcard exhaustive: %s\n", exhaustive_with ? "Yes" : "No");
    printf("   Patterns without wildcard exhaustive: %s\n", exhaustive_without ? "Yes" : "No");
    
    // Print statistics
    printf("5. Pattern matching statistics:\n");
    wyn_print_pattern_stats();
    
    // Cleanup - now each pattern owns its sub-patterns
    wyn_free_pattern(literal_pat);
    wyn_free_pattern(ident_pat);
    wyn_free_pattern(wildcard_pat);
    wyn_free_pattern(struct_pat);
    wyn_free_pattern(array_pat);
    wyn_free_pattern(tuple_pat);
    wyn_free_pattern(some_pat);
    wyn_free_pattern(none_pat);
    free(int_type);
    wyn_cleanup_patterns();
    
    printf("=== T3.3.1: Destructuring Patterns Test Complete ===\n");
    return 0;
}
