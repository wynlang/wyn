#include "module_loader.h"
#include "common.h"
#include "types.h"
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// External symbols from other compilation units
extern void init_lexer(const char* source, const char* filename);
extern Program* parse_program(void);
extern Type* make_type(TypeKind kind);
extern void add_symbol(SymbolTable* table, Token name, Type* type, bool is_mutable);
extern Program* load_module(const char* module_name);  // From module.c

// Extract exported symbols from a module and add them to the target program
void extract_exports(Program* module, SymbolTable* target_scope, ImportStmt* import) {
    if (!module || !target_scope) return;
    
    extern Type* make_type(TypeKind kind);
    extern void add_symbol(SymbolTable* table, Token name, Type* type, bool is_mutable);
    
    for (int i = 0; i < module->count; i++) {
        Stmt* stmt = module->stmts[i];
        
        // Check if this is an export statement
        if (stmt->type == STMT_EXPORT && stmt->export.stmt) {
            Stmt* exported = stmt->export.stmt;
            
            if (exported->type == STMT_FN) {
                // Check if this function is in the import list (if selective import)
                bool should_import = true;
                if (import && import->item_count > 0) {
                    should_import = false;
                    for (int j = 0; j < import->item_count; j++) {
                        Token item = import->items[j];
                        if (exported->fn.name.length == item.length &&
                            memcmp(exported->fn.name.start, item.start, item.length) == 0) {
                            should_import = true;
                            break;
                        }
                    }
                }
                
                if (should_import) {
                    // Check if already in symbol table
                    Symbol* existing = find_symbol(target_scope, exported->fn.name);
                    if (!existing) {
                        // Add function to target scope
                        Type* fn_type = make_type(TYPE_FUNCTION);
                        fn_type->fn_type.param_count = exported->fn.param_count;
                        fn_type->fn_type.param_types = malloc(sizeof(Type*) * exported->fn.param_count);
                        
                        Type* int_type = make_type(TYPE_INT);
                        for (int j = 0; j < exported->fn.param_count; j++) {
                            // Simplified: assume int for now
                            fn_type->fn_type.param_types[j] = int_type;
                        }
                        
                        // Get return type - simplified
                        fn_type->fn_type.return_type = int_type;
                        
                        add_symbol(target_scope, exported->fn.name, fn_type, false);
                    }
                }
            }
        }
    }
}

// Merge exported functions from module into target program
void merge_module_exports(Program* module, Program* target, ImportStmt* import) {
    if (!module || !target) return;
    
    // Merge ALL functions for codegen (including private ones for dependencies)
    // Exported functions are wrapped in STMT_EXPORT, private ones are just STMT_FN
    // The function registration loop will only register STMT_EXPORT functions
    for (int i = 0; i < module->count; i++) {
        Stmt* stmt = module->stmts[i];
        
        if (stmt->type == STMT_EXPORT && stmt->export.stmt && stmt->export.stmt->type == STMT_FN) {
            // Exported function - add the export wrapper
            bool already_exists = false;
            for (int k = 0; k < target->count; k++) {
                if (target->stmts[k] == stmt) {
                    already_exists = true;
                    break;
                }
            }
            
            if (!already_exists) {
                target->stmts = realloc(target->stmts, sizeof(Stmt*) * (target->count + 1));
                target->stmts[target->count++] = stmt;
            }
        } else if (stmt->type == STMT_FN) {
            // Private function - add for codegen but won't be registered
            bool already_exists = false;
            for (int k = 0; k < target->count; k++) {
                if (target->stmts[k] == stmt) {
                    already_exists = true;
                    break;
                }
            }
            
            if (!already_exists) {
                target->stmts = realloc(target->stmts, sizeof(Stmt*) * (target->count + 1));
                target->stmts[target->count++] = stmt;
            }
        }
    }
}
