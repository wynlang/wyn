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
bool llvm_write_ir_to_file(const char* filename);
bool llvm_compile_to_object(const char* ir_file, const char* obj_file);
bool llvm_link_binary(const char* obj_file, const char* output, const char* wyn_root);
bool codegen_generate_bitcode(const char* filename);
bool codegen_generate_object(const char* filename);
LLVMCodegenContext* get_current_llvm_context(void);

#else
// Fallback declarations when LLVM is not available
void init_codegen(FILE* output);
void codegen_c_header(void);
void codegen_program(Program* prog);
void cleanup_codegen(void);

// LLVM-specific features (not implemented in C backend)
static inline bool codegen_generate_bitcode(const char* filename) { (void)filename; return false; }
static inline bool codegen_generate_object(const char* filename) { (void)filename; return false; }
static inline void* get_current_llvm_context(void) { return NULL; }

#endif

#endif // LLVM_CODEGEN_H
