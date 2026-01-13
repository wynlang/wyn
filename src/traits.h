#ifndef WYN_TRAITS_H
#define WYN_TRAITS_H

#include "common.h"
#include "ast.h"

// Initialize trait system
void wyn_traits_init(void);

// Register a trait definition
void wyn_register_trait(void* trait_ptr);

// Find a trait by name
void* wyn_find_trait(Token name);

// Register a trait implementation
void wyn_register_trait_impl(Token trait_name, Token type_name, void** methods, int method_count);

// Find trait implementation for a type
void* wyn_find_trait_impl(Token trait_name, Token type_name);

// Check if a type implements a trait
bool wyn_type_implements_trait(Token type_name, Token trait_name);

// Cleanup trait system
void wyn_cleanup_traits(void);

// Standard trait support
void wyn_register_standard_traits(void);
void wyn_implement_standard_traits_for_basic_types(void);
bool wyn_is_standard_trait(Token trait_name);
const char* wyn_get_standard_trait_method(Token trait_name);
bool wyn_can_derive_trait(Token type_name, Token trait_name);

// Statistics and debugging
void wyn_print_trait_stats(void);
void wyn_list_traits(void);

#endif