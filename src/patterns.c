#include "types.h"
#include "ast.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// T3.3.1: Destructuring Patterns Implementation
// Pattern matching and destructuring for advanced language features

// Forward declarations
Type* check_expr(Expr* expr, SymbolTable* scope);
Symbol* find_symbol(SymbolTable* scope, Token name);
Type* make_type(TypeKind kind);
void add_symbol(SymbolTable* scope, Token name, Type* type, bool is_mutable);

// Pattern matching statistics
typedef struct {
    size_t patterns_created;
    size_t patterns_matched;
    size_t destructuring_operations;
    size_t guard_evaluations;
} PatternStats;

static PatternStats g_pattern_stats = {0};

// Initialize pattern matching system
void wyn_patterns_init(void) {
    memset(&g_pattern_stats, 0, sizeof(PatternStats));
}

// T3.3.1: Create a literal pattern
Pattern* wyn_create_literal_pattern(Token value) {
    Pattern* pattern = malloc(sizeof(Pattern));
    if (!pattern) return NULL;
    
    pattern->type = PATTERN_LITERAL;
    pattern->literal.value = value;
    
    g_pattern_stats.patterns_created++;
    return pattern;
}

// T3.3.1: Create an identifier pattern (variable binding)
Pattern* wyn_create_ident_pattern(Token name) {
    Pattern* pattern = malloc(sizeof(Pattern));
    if (!pattern) return NULL;
    
    pattern->type = PATTERN_IDENT;
    pattern->ident.name = name;
    
    g_pattern_stats.patterns_created++;
    return pattern;
}

// T3.3.1: Create a wildcard pattern
Pattern* wyn_create_wildcard_pattern(void) {
    Pattern* pattern = malloc(sizeof(Pattern));
    if (!pattern) return NULL;
    
    pattern->type = PATTERN_WILDCARD;
    
    g_pattern_stats.patterns_created++;
    return pattern;
}

// T3.3.1: Create a struct destructuring pattern
Pattern* wyn_create_struct_pattern(Token struct_name, Token* field_names, Pattern** field_patterns, int field_count) {
    Pattern* pattern = malloc(sizeof(Pattern));
    if (!pattern) return NULL;
    
    pattern->type = PATTERN_STRUCT;
    pattern->struct_pat.struct_name = struct_name;
    pattern->struct_pat.field_names = field_names;
    pattern->struct_pat.field_patterns = field_patterns;
    pattern->struct_pat.field_count = field_count;
    
    g_pattern_stats.patterns_created++;
    return pattern;
}

// T3.3.1: Create an array destructuring pattern
Pattern* wyn_create_array_pattern(Pattern** elements, int element_count, bool has_rest, Token rest_name) {
    Pattern* pattern = malloc(sizeof(Pattern));
    if (!pattern) return NULL;
    
    pattern->type = PATTERN_ARRAY;
    pattern->array.elements = elements;
    pattern->array.element_count = element_count;
    pattern->array.has_rest = has_rest;
    pattern->array.rest_name = rest_name;
    
    g_pattern_stats.patterns_created++;
    return pattern;
}

// T3.3.1: Create a tuple destructuring pattern
Pattern* wyn_create_tuple_pattern(Pattern** elements, int element_count) {
    Pattern* pattern = malloc(sizeof(Pattern));
    if (!pattern) return NULL;
    
    pattern->type = PATTERN_TUPLE;
    pattern->tuple.elements = elements;
    pattern->tuple.element_count = element_count;
    
    g_pattern_stats.patterns_created++;
    return pattern;
}

// T3.3.1: Create a range pattern
Pattern* wyn_create_range_pattern(Expr* start, Expr* end, bool inclusive) {
    Pattern* pattern = malloc(sizeof(Pattern));
    if (!pattern) return NULL;
    
    pattern->type = PATTERN_RANGE;
    pattern->range.start = start;
    pattern->range.end = end;
    pattern->range.inclusive = inclusive;
    
    g_pattern_stats.patterns_created++;
    return pattern;
}

// T3.3.1: Create an option pattern (Some/None)
Pattern* wyn_create_option_pattern(Pattern* inner, bool is_some) {
    Pattern* pattern = malloc(sizeof(Pattern));
    if (!pattern) return NULL;
    
    pattern->type = PATTERN_OPTION;
    pattern->option.inner = inner;
    pattern->option.is_some = is_some;
    
    g_pattern_stats.patterns_created++;
    return pattern;
}

// T3.3.1: Create a guard pattern
Pattern* wyn_create_guard_pattern(Pattern* base_pattern, Expr* guard) {
    Pattern* pattern = malloc(sizeof(Pattern));
    if (!pattern) return NULL;
    
    pattern->type = PATTERN_GUARD;
    pattern->guard.pattern = base_pattern;
    pattern->guard.guard = guard;
    
    g_pattern_stats.patterns_created++;
    return pattern;
}

// T3.3.1: Check if a pattern matches a value (simplified implementation)
bool wyn_pattern_matches(Pattern* pattern, Expr* value, SymbolTable* scope) {
    if (!pattern || !value) return false;
    
    g_pattern_stats.patterns_matched++;
    
    switch (pattern->type) {
        case PATTERN_LITERAL:
            // Compare literal values
            if (value->type == EXPR_INT && pattern->literal.value.type == TOKEN_INT) {
                // Simplified: assume match for now
                return true;
            }
            return false;
            
        case PATTERN_IDENT:
            // Variable binding always matches
            return true;
            
        case PATTERN_WILDCARD:
            // Wildcard always matches
            return true;
            
        case PATTERN_STRUCT:
            // Check if value is a struct of the right type
            if (value->type == EXPR_STRUCT_INIT) {
                // Simplified: assume match for now
                g_pattern_stats.destructuring_operations++;
                return true;
            }
            return false;
            
        case PATTERN_ARRAY:
            // Check if value is an array
            if (value->type == EXPR_ARRAY) {
                g_pattern_stats.destructuring_operations++;
                return true;
            }
            return false;
            
        case PATTERN_TUPLE:
            // Check if value is a tuple
            if (value->type == EXPR_TUPLE) {
                g_pattern_stats.destructuring_operations++;
                return true;
            }
            return false;
            
        case PATTERN_RANGE:
            // Check if value is within range
            // Simplified implementation
            return true;
            
        case PATTERN_OPTION:
            // Check if value matches Some/None pattern
            if (value->type == EXPR_SOME || value->type == EXPR_NONE) {
                return true;
            }
            return false;
            
        case PATTERN_GUARD:
            // Check base pattern and evaluate guard
            if (wyn_pattern_matches(pattern->guard.pattern, value, scope)) {
                g_pattern_stats.guard_evaluations++;
                // Simplified: assume guard passes
                return true;
            }
            return false;
    }
    
    return false;
}

// T3.3.1: Extract bindings from a pattern match
void wyn_extract_pattern_bindings(Pattern* pattern, Expr* value, SymbolTable* scope) {
    if (!pattern || !value || !scope) return;
    
    switch (pattern->type) {
        case PATTERN_IDENT:
            // Bind variable to value
            // Simplified: create a symbol with the pattern name
            {
                Type* value_type = make_type(TYPE_INT); // Simplified
                value_type->name = pattern->ident.name;
                add_symbol(scope, pattern->ident.name, value_type, true);
            }
            break;
            
        case PATTERN_STRUCT:
            // Extract struct field bindings
            for (int i = 0; i < pattern->struct_pat.field_count; i++) {
                if (pattern->struct_pat.field_patterns[i]) {
                    wyn_extract_pattern_bindings(pattern->struct_pat.field_patterns[i], value, scope);
                }
            }
            break;
            
        case PATTERN_ARRAY:
            // Extract array element bindings
            for (int i = 0; i < pattern->array.element_count; i++) {
                if (pattern->array.elements[i]) {
                    wyn_extract_pattern_bindings(pattern->array.elements[i], value, scope);
                }
            }
            if (pattern->array.has_rest) {
                // Bind rest elements
                Type* array_type = make_type(TYPE_ARRAY);
                array_type->name = pattern->array.rest_name;
                add_symbol(scope, pattern->array.rest_name, array_type, true);
            }
            break;
            
        case PATTERN_TUPLE:
            // Extract tuple element bindings
            for (int i = 0; i < pattern->tuple.element_count; i++) {
                if (pattern->tuple.elements[i]) {
                    wyn_extract_pattern_bindings(pattern->tuple.elements[i], value, scope);
                }
            }
            break;
            
        case PATTERN_OPTION:
            // Extract option inner binding
            if (pattern->option.inner && pattern->option.is_some) {
                wyn_extract_pattern_bindings(pattern->option.inner, value, scope);
            }
            break;
            
        case PATTERN_GUARD:
            // Extract bindings from base pattern
            wyn_extract_pattern_bindings(pattern->guard.pattern, value, scope);
            break;
            
        default:
            // No bindings for literal, wildcard, range patterns
            break;
    }
}

// T3.3.1: Check if a pattern is exhaustive (covers all cases)
bool wyn_is_pattern_exhaustive(Pattern** patterns, int pattern_count, Type* match_type) {
    (void)match_type;  // Reserved for type-specific exhaustiveness checking
    if (!patterns || pattern_count == 0) return false;
    
    // Check for wildcard pattern (always exhaustive)
    for (int i = 0; i < pattern_count; i++) {
        if (patterns[i] && patterns[i]->type == PATTERN_WILDCARD) {
            return true;
        }
    }
    
    // For now, assume non-exhaustive unless wildcard is present
    // A full implementation would analyze all possible values of match_type
    return false;
}

// T3.3.1: Free a pattern and its resources
void wyn_free_pattern(Pattern* pattern) {
    if (!pattern) return;
    
    switch (pattern->type) {
        case PATTERN_STRUCT:
            if (pattern->struct_pat.field_names) {
                free(pattern->struct_pat.field_names);
            }
            if (pattern->struct_pat.field_patterns) {
                for (int i = 0; i < pattern->struct_pat.field_count; i++) {
                    wyn_free_pattern(pattern->struct_pat.field_patterns[i]);
                }
                free(pattern->struct_pat.field_patterns);
            }
            break;
            
        case PATTERN_ARRAY:
            if (pattern->array.elements) {
                for (int i = 0; i < pattern->array.element_count; i++) {
                    wyn_free_pattern(pattern->array.elements[i]);
                }
                free(pattern->array.elements);
            }
            break;
            
        case PATTERN_TUPLE:
            if (pattern->tuple.elements) {
                for (int i = 0; i < pattern->tuple.element_count; i++) {
                    wyn_free_pattern(pattern->tuple.elements[i]);
                }
                free(pattern->tuple.elements);
            }
            break;
            
        case PATTERN_OPTION:
            wyn_free_pattern(pattern->option.inner);
            break;
            
        case PATTERN_GUARD:
            wyn_free_pattern(pattern->guard.pattern);
            break;
            
        default:
            // No additional cleanup needed for other pattern types
            break;
    }
    
    free(pattern);
}

// T3.3.1: Get pattern matching statistics
PatternStats wyn_get_pattern_stats(void) {
    return g_pattern_stats;
}

// T3.3.1: Print pattern matching statistics
void wyn_print_pattern_stats(void) {
    printf("=== Pattern Matching Statistics ===\n");
    printf("Patterns created: %zu\n", g_pattern_stats.patterns_created);
    printf("Patterns matched: %zu\n", g_pattern_stats.patterns_matched);
    printf("Destructuring operations: %zu\n", g_pattern_stats.destructuring_operations);
    printf("Guard evaluations: %zu\n", g_pattern_stats.guard_evaluations);
    printf("==================================\n");
}

// T3.3.1: Reset pattern matching statistics
void wyn_reset_pattern_stats(void) {
    memset(&g_pattern_stats, 0, sizeof(PatternStats));
}

// T3.3.2: Check if a pattern is irrefutable (always matches)
bool wyn_is_pattern_irrefutable(Pattern* pattern) {
    if (!pattern) return false;
    
    switch (pattern->type) {
        case PATTERN_IDENT:
        case PATTERN_WILDCARD:
            // Variable bindings and wildcards always match
            return true;
            
        case PATTERN_STRUCT:
            // Struct patterns are irrefutable if all field patterns are irrefutable
            for (int i = 0; i < pattern->struct_pat.field_count; i++) {
                if (pattern->struct_pat.field_patterns[i] && 
                    !wyn_is_pattern_irrefutable(pattern->struct_pat.field_patterns[i])) {
                    return false;
                }
            }
            return true;
            
        case PATTERN_ARRAY:
            // Array patterns are irrefutable if all element patterns are irrefutable
            for (int i = 0; i < pattern->array.element_count; i++) {
                if (pattern->array.elements[i] && 
                    !wyn_is_pattern_irrefutable(pattern->array.elements[i])) {
                    return false;
                }
            }
            return true;
            
        case PATTERN_TUPLE:
            // Tuple patterns are irrefutable if all element patterns are irrefutable
            for (int i = 0; i < pattern->tuple.element_count; i++) {
                if (pattern->tuple.elements[i] && 
                    !wyn_is_pattern_irrefutable(pattern->tuple.elements[i])) {
                    return false;
                }
            }
            return true;
            
        case PATTERN_GUARD:
            // Guard patterns are never irrefutable (guard might fail)
            return false;
            
        case PATTERN_LITERAL:
        case PATTERN_RANGE:
        case PATTERN_OPTION:
            // These patterns can fail to match
            return false;
    }
    
    return false;
}

// T3.3.2: Process a let binding with pattern matching
bool wyn_process_let_binding(Pattern* pattern, Expr* init_expr, SymbolTable* scope) {
    if (!pattern || !init_expr || !scope) return false;
    
    // Check if pattern is irrefutable for let bindings
    if (!wyn_is_pattern_irrefutable(pattern)) {
        printf("Warning: Pattern in let binding is refutable and may fail at runtime\n");
    }
    
    // Check if pattern matches the initialization expression
    if (!wyn_pattern_matches(pattern, init_expr, scope)) {
        printf("Error: Pattern does not match initialization expression in let binding\n");
        return false;
    }
    
    // Extract bindings from the pattern
    wyn_extract_pattern_bindings(pattern, init_expr, scope);
    
    return true;
}

// T3.3.2: Validate function parameter patterns
bool wyn_validate_function_parameter_patterns(Pattern** param_patterns, int param_count) {
    if (!param_patterns) return true;
    
    for (int i = 0; i < param_count; i++) {
        if (param_patterns[i]) {
            // Function parameter patterns should be irrefutable
            if (!wyn_is_pattern_irrefutable(param_patterns[i])) {
                printf("Error: Function parameter pattern %d is refutable\n", i + 1);
                return false;
            }
        }
    }
    
    return true;
}

// T3.3.2: Check for incomplete patterns in let bindings
void wyn_check_let_pattern_completeness(Pattern* pattern, Type* value_type) {
    if (!pattern || !value_type) return;
    
    // For now, just warn about potentially incomplete patterns
    switch (pattern->type) {
        case PATTERN_OPTION:
            if (pattern->option.is_some) {
                printf("Warning: Let binding with Some pattern may fail if value is None\n");
            }
            break;
            
        case PATTERN_LITERAL:
            printf("Warning: Let binding with literal pattern may fail if value doesn't match\n");
            break;
            
        case PATTERN_RANGE:
            printf("Warning: Let binding with range pattern may fail if value is out of range\n");
            break;
            
        default:
            // Other patterns are generally safe for let bindings
            break;
    }
}

// T3.3.2: Extract all variable names from a pattern (for scope analysis)
void wyn_extract_pattern_variables(Pattern* pattern, Token** variables, int* var_count, int max_vars) {
    if (!pattern || !variables || !var_count || *var_count >= max_vars) return;
    
    switch (pattern->type) {
        case PATTERN_IDENT:
            variables[(*var_count)++] = &pattern->ident.name;
            break;
            
        case PATTERN_STRUCT:
            for (int i = 0; i < pattern->struct_pat.field_count && *var_count < max_vars; i++) {
                if (pattern->struct_pat.field_patterns[i]) {
                    wyn_extract_pattern_variables(pattern->struct_pat.field_patterns[i], 
                                                variables, var_count, max_vars);
                }
            }
            break;
            
        case PATTERN_ARRAY:
            for (int i = 0; i < pattern->array.element_count && *var_count < max_vars; i++) {
                if (pattern->array.elements[i]) {
                    wyn_extract_pattern_variables(pattern->array.elements[i], 
                                                variables, var_count, max_vars);
                }
            }
            if (pattern->array.has_rest && *var_count < max_vars) {
                variables[(*var_count)++] = &pattern->array.rest_name;
            }
            break;
            
        case PATTERN_TUPLE:
            for (int i = 0; i < pattern->tuple.element_count && *var_count < max_vars; i++) {
                if (pattern->tuple.elements[i]) {
                    wyn_extract_pattern_variables(pattern->tuple.elements[i], 
                                                variables, var_count, max_vars);
                }
            }
            break;
            
        case PATTERN_OPTION:
            if (pattern->option.inner) {
                wyn_extract_pattern_variables(pattern->option.inner, variables, var_count, max_vars);
            }
            break;
            
        case PATTERN_GUARD:
            if (pattern->guard.pattern) {
                wyn_extract_pattern_variables(pattern->guard.pattern, variables, var_count, max_vars);
            }
            break;
            
        default:
            // Other patterns don't introduce variables
            break;
    }
}

// T3.3.1: Cleanup pattern matching system
void wyn_cleanup_patterns(void) {
    wyn_reset_pattern_stats();
}
