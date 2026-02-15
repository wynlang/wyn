// Runtime Library Function Declarations for LLVM Codegen
#ifndef LLVM_RUNTIME_H
#define LLVM_RUNTIME_H

#ifdef WITH_LLVM
#include <llvm-c/Core.h>
#include "llvm_context.h"

// Declare runtime functions in LLVM module
void llvm_declare_runtime_functions(LLVMCodegenContext* ctx);

// Declare specific function groups
void llvm_declare_system_functions(LLVMCodegenContext* ctx);
void llvm_declare_file_functions(LLVMCodegenContext* ctx);
void llvm_declare_time_functions(LLVMCodegenContext* ctx);

#endif // WITH_LLVM
#endif // LLVM_RUNTIME_H
