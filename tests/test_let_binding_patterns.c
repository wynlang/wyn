// T3.3.2: Pattern Matching in Let Bindings Test Program
// Test let binding patterns and function parameter patterns

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
    printf("   Binding variable: %.*s (mutable: %s)\n", 
           (int)name.length, name.start, is_mutable ? "yes" : "no");
}

// Test let binding patterns functionality
int main() {
    printf("=== T3.3.2: Pattern Matching in Let Bindings Test ===\n");
    
    // Initialize pattern system
    wyn_patterns_init();
    
    // Test irrefutable pattern checking
    printf("1. Testing irrefutable pattern checking...\n");
    
    Token var_token = {TOKEN_IDENT, "x", 1, 0};
    Pattern* ident_pat = wyn_create_ident_pattern(var_token);
    Pattern* wildcard_pat = wyn_create_wildcard_pattern();
    Token int_token = {TOKEN_INT, "42", 2, 0};
    Pattern* literal_pat = wyn_create_literal_pattern(int_token);
    Pattern* some_pat = wyn_create_option_pattern(ident_pat, true);
    
    printf("   Identifier pattern irrefutable: %s\n", 
           wyn_is_pattern_irrefutable(ident_pat) ? "Yes" : "No");
    printf("   Wildcard pattern irrefutable: %s\n", 
           wyn_is_pattern_irrefutable(wildcard_pat) ? "Yes" : "No");
    printf("   Literal pattern irrefutable: %s\n", 
           wyn_is_pattern_irrefutable(literal_pat) ? "Yes" : "No");
    printf("   Some pattern irrefutable: %s\n", 
           wyn_is_pattern_irrefutable(some_pat) ? "Yes" : "No");
    
    // Test struct pattern irrefutability
    Token struct_name = {TOKEN_IDENT, "Point", 5, 0};
    Token field_names[2] = {
        {TOKEN_IDENT, "x", 1, 0},
        {TOKEN_IDENT, "y", 1, 0}
    };
    Pattern* field_pat1 = wyn_create_ident_pattern(field_names[0]);
    Pattern* field_pat2 = wyn_create_ident_pattern(field_names[1]);
    Pattern* field_patterns[2] = {field_pat1, field_pat2};
    Pattern* struct_pat = wyn_create_struct_pattern(struct_name, field_names, field_patterns, 2);
    
    printf("   Struct pattern (all idents) irrefutable: %s\n", 
           wyn_is_pattern_irrefutable(struct_pat) ? "Yes" : "No");
    
    // Test let binding processing
    printf("2. Testing let binding processing...\n");
    
    SymbolTable scope = {0};
    Expr int_expr = {EXPR_INT, .token = int_token};
    Expr struct_expr = {EXPR_STRUCT_INIT, .token = struct_name};
    
    bool ident_result = wyn_process_let_binding(ident_pat, &int_expr, &scope);
    bool struct_result = wyn_process_let_binding(struct_pat, &struct_expr, &scope);
    
    printf("   Identifier let binding processed: %s\n", ident_result ? "Success" : "Failed");
    printf("   Struct let binding processed: %s\n", struct_result ? "Success" : "Failed");
    
    // Test function parameter pattern validation
    printf("3. Testing function parameter pattern validation...\n");
    
    Pattern* param_patterns_good[] = {ident_pat, wildcard_pat, struct_pat};
    Pattern* param_patterns_bad[] = {literal_pat, some_pat};
    
    bool good_params = wyn_validate_function_parameter_patterns(param_patterns_good, 3);
    bool bad_params = wyn_validate_function_parameter_patterns(param_patterns_bad, 2);
    
    printf("   Good parameter patterns valid: %s\n", good_params ? "Yes" : "No");
    printf("   Bad parameter patterns valid: %s\n", bad_params ? "Yes" : "No");
    
    // Test pattern completeness checking
    printf("4. Testing pattern completeness checking...\n");
    
    Type* int_type = make_type(TYPE_INT);
    Type* optional_type = make_type(TYPE_OPTIONAL);
    
    printf("   Checking literal pattern completeness:\n");
    wyn_check_let_pattern_completeness(literal_pat, int_type);
    
    printf("   Checking Some pattern completeness:\n");
    wyn_check_let_pattern_completeness(some_pat, optional_type);
    
    printf("   Checking identifier pattern completeness:\n");
    wyn_check_let_pattern_completeness(ident_pat, int_type);
    
    // Test variable extraction from patterns
    printf("5. Testing variable extraction from patterns...\n");
    
    Token* variables[10];
    int var_count = 0;
    
    wyn_extract_pattern_variables(struct_pat, variables, &var_count, 10);
    
    printf("   Variables extracted from struct pattern: %d\n", var_count);
    for (int i = 0; i < var_count; i++) {
        printf("     Variable %d: %.*s\n", i + 1, 
               (int)variables[i]->length, variables[i]->start);
    }
    
    // Test array pattern with rest
    Token rest_name = {TOKEN_IDENT, "rest", 4, 0};
    Pattern* array_elem1 = wyn_create_ident_pattern((Token){TOKEN_IDENT, "first", 5, 0});
    Pattern* array_elem2 = wyn_create_ident_pattern((Token){TOKEN_IDENT, "second", 6, 0});
    Pattern* array_elements[2] = {array_elem1, array_elem2};
    Pattern* array_pat = wyn_create_array_pattern(array_elements, 2, true, rest_name);
    
    var_count = 0;
    wyn_extract_pattern_variables(array_pat, variables, &var_count, 10);
    
    printf("   Variables extracted from array pattern: %d\n", var_count);
    for (int i = 0; i < var_count; i++) {
        printf("     Variable %d: %.*s\n", i + 1, 
               (int)variables[i]->length, variables[i]->start);
    }
    
    // Print statistics
    printf("6. Pattern matching statistics:\n");
    wyn_print_pattern_stats();
    
    // Cleanup
    wyn_free_pattern(literal_pat);
    wyn_free_pattern(wildcard_pat);
    wyn_free_pattern(some_pat);
    wyn_free_pattern(struct_pat);
    wyn_free_pattern(array_pat);
    free(int_type);
    free(optional_type);
    wyn_cleanup_patterns();
    
    printf("=== T3.3.2: Pattern Matching in Let Bindings Test Complete ===\n");
    return 0;
}
