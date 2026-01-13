#ifndef GENERICS_H
#define GENERICS_H

#include "types.h"
#include "ast.h"

// Forward declarations from generics.c
typedef struct GenericFunction GenericFunction;
typedef struct GenericStruct GenericStruct;
typedef struct TypeConstraint TypeConstraint;
typedef struct GenericInstantiation GenericInstantiation;

// Function declarations
void wyn_generics_init(void);
void wyn_register_generic_function(void* fn);
GenericFunction* wyn_find_generic_function(Token name);
Type* wyn_instantiate_generic_function(const char* name, Type** type_args, int arg_count);
bool wyn_is_generic_function_call(Token function_name);
Type* wyn_infer_generic_call_type(Token function_name, Expr** args, int arg_count);
void wyn_register_generic_instantiation(Token func_name, Type** type_args, int type_arg_count);
bool wyn_types_equal(Type* a, Type* b);
void wyn_generate_monomorphic_instances(void);
GenericInstantiation* wyn_get_instantiations(void);
void wyn_emit_monomorphic_function(GenericFunction* generic_fn, Type** type_args, int type_arg_count, const char* monomorphic_name);

#endif // GENERICS_H
