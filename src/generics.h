#ifndef GENERICS_H
#define GENERICS_H

#include "types.h"
#include "ast.h"

// Forward declarations from generics.c
typedef struct GenericFunction GenericFunction;
typedef struct GenericStruct GenericStruct;
typedef struct TypeConstraint TypeConstraint;

// Function declarations
void wyn_generics_init(void);
void wyn_register_generic_function(void* fn);
GenericFunction* wyn_find_generic_function(Token name);
Type* wyn_instantiate_generic_function(const char* name, Type** type_args, int arg_count);
bool wyn_is_generic_function_call(Token function_name);
Type* wyn_infer_generic_call_type(Token function_name, Expr** args, int arg_count);

#endif // GENERICS_H
