#ifndef LLVM_CODEGEN_H
#define LLVM_CODEGEN_H

#include <stdio.h>
#include <stdbool.h>
#include "ast.h"

#ifdef WITH_LLVM
#include "llvm_context.h"

// Main codegen interface (compatible with existing parser integration)
void init_codegen(FILE* output);
void codegen_c_header(void);
void codegen_program(Program* prog);
void cleanup_codegen(void);

// LLVM-specific functions
bool codegen_generate_bitcode(const char* filename);
bool codegen_generate_object(const char* filename);
LLVMCodegenContext* get_current_llvm_context(void);

#else
// Fallback declarations when LLVM is not available
void init_codegen(FILE* output);
void codegen_c_header(void);
void codegen_program(Program* prog);
void cleanup_codegen(void);

// Stub functions for LLVM-specific features
static inline bool codegen_generate_bitcode(const char* filename) { (void)filename; return false; }
static inline bool codegen_generate_object(const char* filename) { (void)filename; return false; }
static inline void* get_current_llvm_context(void) { return NULL; }

#endif

#endif // LLVM_CODEGEN_H
