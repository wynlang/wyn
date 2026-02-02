// Runtime Library Function Declarations for LLVM
#include "llvm_runtime.h"

#ifdef WITH_LLVM

void llvm_declare_system_functions(LLVMCodegenContext* ctx) {
    LLVMTypeRef int_type = ctx->int_type;
    LLVMTypeRef str_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
    
    // int wyn_system_argc(void)
    LLVMTypeRef argc_type = LLVMFunctionType(int_type, NULL, 0, false);
    LLVMAddFunction(ctx->module, "wyn_system_argc", argc_type);
    
    // const char* wyn_system_argv(int index)
    LLVMTypeRef argv_params[] = { int_type };
    LLVMTypeRef argv_type = LLVMFunctionType(str_type, argv_params, 1, false);
    LLVMAddFunction(ctx->module, "wyn_system_argv", argv_type);
    
    // const char* wyn_system_env(const char* name)
    LLVMTypeRef env_params[] = { str_type };
    LLVMTypeRef env_type = LLVMFunctionType(str_type, env_params, 1, false);
    LLVMAddFunction(ctx->module, "wyn_system_env", env_type);
    
    // int wyn_system_set_env(const char* name, const char* value)
    LLVMTypeRef setenv_params[] = { str_type, str_type };
    LLVMTypeRef setenv_type = LLVMFunctionType(int_type, setenv_params, 2, false);
    LLVMAddFunction(ctx->module, "wyn_system_set_env", setenv_type);
    
    // int wyn_system_exec(const char* command)
    LLVMTypeRef exec_params[] = { str_type };
    LLVMTypeRef exec_type = LLVMFunctionType(int_type, exec_params, 1, false);
    LLVMAddFunction(ctx->module, "wyn_system_exec", exec_type);
}

void llvm_declare_file_functions(LLVMCodegenContext* ctx) {
    LLVMTypeRef int_type = ctx->int_type;
    LLVMTypeRef str_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
    
    // char* wyn_file_read(const char* path)
    LLVMTypeRef read_params[] = { str_type };
    LLVMTypeRef read_type = LLVMFunctionType(str_type, read_params, 1, false);
    LLVMAddFunction(ctx->module, "wyn_file_read", read_type);
    
    // int wyn_file_write(const char* path, const char* content)
    LLVMTypeRef write_params[] = { str_type, str_type };
    LLVMTypeRef write_type = LLVMFunctionType(int_type, write_params, 2, false);
    LLVMAddFunction(ctx->module, "wyn_file_write", write_type);
    
    // int wyn_file_exists(const char* path)
    LLVMTypeRef exists_params[] = { str_type };
    LLVMTypeRef exists_type = LLVMFunctionType(int_type, exists_params, 1, false);
    LLVMAddFunction(ctx->module, "wyn_file_exists", exists_type);
    
    // int wyn_file_is_file(const char* path)
    LLVMAddFunction(ctx->module, "wyn_file_is_file", exists_type);
    
    // int wyn_file_is_dir(const char* path)
    LLVMAddFunction(ctx->module, "wyn_file_is_dir", exists_type);
    
    // char* wyn_file_join(const char* a, const char* b)
    LLVMTypeRef join_params[] = { str_type, str_type };
    LLVMTypeRef join_type = LLVMFunctionType(str_type, join_params, 2, false);
    LLVMAddFunction(ctx->module, "wyn_file_join", join_type);
    
    // char* wyn_file_basename(const char* path)
    LLVMAddFunction(ctx->module, "wyn_file_basename", read_type);
    
    // char* wyn_file_dirname(const char* path)
    LLVMAddFunction(ctx->module, "wyn_file_dirname", read_type);
    
    // char* wyn_file_extension(const char* path)
    LLVMAddFunction(ctx->module, "wyn_file_extension", read_type);
    
    // char* wyn_file_cwd(void)
    LLVMTypeRef cwd_type = LLVMFunctionType(str_type, NULL, 0, false);
    LLVMAddFunction(ctx->module, "wyn_file_cwd", cwd_type);
}

void llvm_declare_time_functions(LLVMCodegenContext* ctx) {
    LLVMTypeRef int64_type = LLVMInt64TypeInContext(ctx->context);
    LLVMTypeRef void_type = LLVMVoidTypeInContext(ctx->context);
    
    // int64_t wyn_time_now(void)
    LLVMTypeRef now_type = LLVMFunctionType(int64_type, NULL, 0, false);
    LLVMAddFunction(ctx->module, "wyn_time_now", now_type);
    
    // void wyn_time_sleep(int64_t ms)
    LLVMTypeRef sleep_params[] = { int64_type };
    LLVMTypeRef sleep_type = LLVMFunctionType(void_type, sleep_params, 1, false);
    LLVMAddFunction(ctx->module, "wyn_time_sleep", sleep_type);
}

void llvm_declare_runtime_functions(LLVMCodegenContext* ctx) {
    if (!ctx) return;
    
    llvm_declare_system_functions(ctx);
    llvm_declare_file_functions(ctx);
    llvm_declare_time_functions(ctx);
}

#endif // WITH_LLVM
