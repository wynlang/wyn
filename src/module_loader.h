#ifndef MODULE_LOADER_H
#define MODULE_LOADER_H

#include "ast.h"
#include "types.h"

// Extract exported symbols from a module
void extract_exports(Program* module, SymbolTable* target_scope, ImportStmt* import);

// Merge exported functions into target program
void merge_module_exports(Program* module, Program* target, ImportStmt* import);

#endif
