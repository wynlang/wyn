#ifndef WYN_LLVM_OPTIMIZATION_H
#define WYN_LLVM_OPTIMIZATION_H

#include <stdbool.h>

// LLVM optimization levels
typedef enum {
    WYN_OPT_NONE = 0,    // -O0
    WYN_OPT_LESS = 1,    // -O1
    WYN_OPT_DEFAULT = 2, // -O2
    WYN_OPT_AGGRESSIVE = 3 // -O3
} WynOptLevel;

// Optimization pass types
typedef enum {
    WYN_PASS_DEAD_CODE_ELIMINATION,
    WYN_PASS_CONSTANT_FOLDING,
    WYN_PASS_FUNCTION_INLINING,
    WYN_PASS_LOOP_UNROLLING,
    WYN_PASS_VECTORIZATION,
    WYN_PASS_TAIL_CALL_OPTIMIZATION,
    WYN_PASS_MEMORY_TO_REGISTER,
    WYN_PASS_GLOBAL_VALUE_NUMBERING,
    WYN_PASS_INSTRUCTION_COMBINING,
    WYN_PASS_REASSOCIATION
} WynOptPass;

// Optimization statistics
typedef struct {
    int functions_inlined;
    int dead_instructions_removed;
    int constants_folded;
    int loops_unrolled;
    int tail_calls_optimized;
    double optimization_time_ms;
} WynOptStats;

// Initialize LLVM optimization system
int wyn_opt_init(void);

// Configure optimization level
int wyn_opt_set_level(WynOptLevel level);

// Enable/disable specific optimization passes
int wyn_opt_enable_pass(WynOptPass pass);
int wyn_opt_disable_pass(WynOptPass pass);

// Run optimization passes on LLVM module
int wyn_opt_run_passes(void* llvm_module);

// Advanced optimization passes
int wyn_opt_run_dead_code_elimination(void* llvm_module);
int wyn_opt_run_constant_folding(void* llvm_module);
int wyn_opt_run_function_inlining(void* llvm_module);
int wyn_opt_run_loop_optimization(void* llvm_module);
int wyn_opt_run_vectorization(void* llvm_module);

// Link-time optimization
int wyn_opt_enable_lto(bool enable);
int wyn_opt_run_lto(void* llvm_module);

// Profile-guided optimization
int wyn_opt_enable_pgo(const char* profile_data_path);
int wyn_opt_run_pgo(void* llvm_module);

// Get optimization statistics
WynOptStats wyn_opt_get_stats(void);
void wyn_opt_print_stats(void);
void wyn_opt_reset_stats(void);

// Cleanup
void wyn_opt_cleanup(void);

#endif // WYN_LLVM_OPTIMIZATION_H
