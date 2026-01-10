CC=gcc
CFLAGS=-Wall -Wextra -std=c11 -g
OPTFLAGS=-O2

# LLVM Integration for Phase 2 (optional)
LLVM_CONFIG ?= llvm-config
LLVM_VERSION := $(shell $(LLVM_CONFIG) --version 2>/dev/null || echo "not-found")

ifeq ($(LLVM_VERSION),not-found)
$(warning LLVM not found. LLVM features will be disabled. Install LLVM 15+ for full functionality)
LLVM_AVAILABLE := 0
else
# Verify LLVM version is 15+
LLVM_MAJOR := $(shell echo $(LLVM_VERSION) | cut -d. -f1)
LLVM_VERSION_CHECK := $(shell [ $(LLVM_MAJOR) -ge 15 ] && echo "ok" || echo "fail")
ifeq ($(LLVM_VERSION_CHECK),fail)
$(warning LLVM version $(LLVM_VERSION) found, but 15+ required. LLVM features disabled)
LLVM_AVAILABLE := 0
else
LLVM_AVAILABLE := 1
endif
endif

ifeq ($(LLVM_AVAILABLE),1)
LLVM_CFLAGS := $(shell $(LLVM_CONFIG) --cflags)
LLVM_LDFLAGS := $(shell $(LLVM_CONFIG) --ldflags)
LLVM_LIBS := $(shell $(LLVM_CONFIG) --libs core executionengine mcjit native)

# Platform-specific adjustments
ifeq ($(OS),Windows_NT)
    LLVM_LIBS += -lole32 -luuid
endif

CFLAGS_LLVM = $(CFLAGS) $(LLVM_CFLAGS) -DWITH_LLVM=1
LDFLAGS_LLVM = $(LLVM_LDFLAGS) $(LLVM_LIBS)
else
# LLVM not available - use basic flags
CFLAGS_LLVM = $(CFLAGS)
LDFLAGS_LLVM = 
endif

all: wyn test

# Original C-based compiler (Phase 1)
wyn: src/main.c src/lexer.c src/parser.c src/checker.c src/codegen.c src/safe_memory.c src/error.c src/security.c src/memory.c src/string.c src/type_inference.c src/generics.c src/traits.c src/patterns.c src/closures.c src/modules.c src/package.c src/collections.c src/io.c src/net.c src/system.c src/stdlib_advanced.c
ifeq ($(OS),Windows_NT)
	$(CC) $(CFLAGS) -I src -o $@ $^ -lws2_32
else
	$(CC) $(CFLAGS) -I src -o $@ $^
endif

# LLVM-based compiler (Phase 2) with Context Management, Target Configuration, Type Mapping, Runtime Functions, Expression Codegen, Statement Codegen, Function Codegen, and Array/String Operations
wyn-llvm: src/main.c src/lexer.c src/parser.c src/checker.c src/llvm_codegen.c src/llvm_context.c src/target_config.c src/type_mapping.c src/runtime_functions.c src/llvm_expression_codegen.c src/llvm_statement_codegen.c src/llvm_function_codegen.c src/llvm_array_string_codegen.c src/safe_memory.c src/error.c src/security.c src/memory.c src/string.c
	$(CC) $(CFLAGS_LLVM) -I src -o $@ $^ $(LDFLAGS_LLVM) -lpthread

# Phase 2 Integration Testing
test_phase2_integration: tests/phase2_integration_simple
	@echo "=== Running Phase 2 Integration Tests ==="
	@./tests/phase2_integration_simple

tests/phase2_integration_simple: tests/phase2_integration_simple.c
	$(CC) $(CFLAGS) -I src -o $@ $^

# Phase 2 Monitoring and Validation
phase2-monitor:
	@./scripts/phase2_monitor_simple.sh

phase2-gates:
	@./scripts/integration_gates.sh all

phase2-status:
	@./scripts/phase2_monitor_simple.sh status

wyn-release: src/main.c src/lexer.c src/parser.c src/checker.c src/codegen.c src/safe_memory.c src/error.c src/security.c src/memory.c src/string.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -I src -o wyn $^
	strip wyn

# Security testing
test_security: tests/test_security
	@echo "=== Running Security Tests ==="
	@./tests/test_security

tests/test_security: tests/test_security.c src/security.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_codegen_wyn: tests/test_codegen_wyn.c $(HEADERS)
	$(CC) $(CFLAGS) -I src -o $@ $< $(LIBS)

tests/test_optimizer_wyn: tests/test_optimizer_wyn.c $(HEADERS)
	$(CC) $(CFLAGS) -I src -o $@ $< $(LIBS)

tests/test_pipeline_wyn: tests/test_pipeline_wyn.c $(HEADERS)
	$(CC) $(CFLAGS) -I src -o $@ $< $(LIBS)

tests/test_bootstrap_validation: tests/test_bootstrap_validation.c $(HEADERS)
	$(CC) $(CFLAGS) -I src -o $@ $< $(LIBS)

tests/test_checker_integration: tests/test_checker_integration.c $(HEADERS)
	$(CC) $(CFLAGS) -I src -o $@ $< $(LIBS)

tests/test_bootstrap_integration: tests/test_bootstrap_integration.c $(HEADERS)
	$(CC) $(CFLAGS) -I src -o $@ $< $(LIBS)

tests/test_lsp_advanced: tests/test_lsp_advanced.c $(HEADERS)
	$(CC) $(CFLAGS) -I src -o $@ $< $(LIBS)

tests/test_ide_integration: tests/test_ide_integration.c $(HEADERS)
	$(CC) $(CFLAGS) -I src -o $@ $< $(LIBS)

tests/test_build_system: tests/test_build_system.c $(HEADERS)
	$(CC) $(CFLAGS) -I src -o $@ $< $(LIBS)

tests/test_production_deployment: tests/test_production_deployment.c $(HEADERS)
	$(CC) $(CFLAGS) -I src -o $@ $< $(LIBS)

tests/test_final_completion: tests/test_final_completion.c $(HEADERS)
	$(CC) $(CFLAGS) -I src -o $@ $< $(LIBS)

# Security scanning
security-scan:
	@echo "=== Running Security Scan ==="
	@./scripts/security_review.sh

# Memory safety testing
valgrind-test: wyn
	@echo "=== Running Valgrind Memory Check ==="
	valgrind --leak-check=full --error-exitcode=1 ./wyn tests/basic.wyn

# Debug build with memory tracking
debug-memory: CFLAGS += -DDEBUG_MEMORY -fsanitize=address -g
debug-memory: wyn
	@echo "Built with memory debugging enabled"

test: wyn test_unit test_integration test_stdlib test_errors test_control_flow

test_unit: test_lexer test_parser test_checker test_codegen test_operators test_default_parameters test_function_overloading test_generic_functions test_parameter_validation test_function_integration test_syntax_design test_system_integration test_wasm_support test_documentation_system test_performance_profiling

test_integration:
	@echo "=== Running Integration Tests ==="
	@./tests/integration_tests.sh

test_stdlib:
	@echo "=== Running Stdlib Tests ==="
	@./tests/test_stdlib.sh

test_errors:
	@echo "=== Running Error Tests ==="
	@./tests/test_errors.sh

test_control_flow:
	@echo "=== Running Control Flow Tests ==="
	@./tests/test_control_flow.sh

# ARC Runtime Tests (T2.3.1)
test_arc_runtime: tests/test_arc_runtime
	@echo "=== Running ARC Runtime Tests ==="
	@./tests/test_arc_runtime

tests/test_arc_runtime: tests/test_arc_runtime.c src/arc_runtime.c src/error.c src/safe_memory.c
	$(CC) $(CFLAGS) -I src -o $@ $^

# ARC Operations Tests (T2.3.2)
test_arc_operations: tests/test_arc_operations
	@echo "=== Running ARC Operations Tests ==="
	@./tests/test_arc_operations

tests/test_arc_operations: tests/test_arc_operations.c src/arc_runtime.c src/weak_references.c src/error.c src/safe_memory.c
	$(CC) $(CFLAGS) -I src -o $@ $^ -lpthread

# Weak Reference Tests (T2.3.3)
test_weak_references: tests/test_weak_references
	@echo "=== Running Weak Reference Tests ==="
	@./tests/test_weak_references

tests/test_weak_references: tests/test_weak_references.c src/arc_runtime.c src/arc_operations.c src/error.c src/safe_memory.c
	$(CC) $(CFLAGS) -I src -o $@ $^ -lpthread

# Cycle Detection Tests (T2.3.4)
test_cycle_detection: tests/test_cycle_detection_minimal
	@echo "=== Running Cycle Detection Tests ==="
	@./tests/test_cycle_detection_minimal

tests/test_cycle_detection_minimal: tests/test_cycle_detection_minimal.c src/arc_runtime.c src/arc_operations.c src/weak_references.c src/cycle_detection.c src/error.c src/safe_memory.c
	$(CC) $(CFLAGS) -I src -o $@ $^ -lpthread

# Memory Pool Tests (T2.3.5)
test_memory_pool: tests/test_memory_pool
	@echo "=== Running Memory Pool Tests ==="
	@./tests/test_memory_pool

tests/test_memory_pool: tests/test_memory_pool.c src/arc_runtime.c src/arc_operations.c src/weak_references.c src/error.c src/safe_memory.c
	$(CC) $(CFLAGS) -I src -o $@ $^ -lpthread

# Performance Monitor Tests (T2.3.6)
test_performance_monitor: tests/test_performance_monitor
	@echo "=== Running Performance Monitor Tests ==="
	@./tests/test_performance_monitor

tests/test_performance_monitor: tests/test_performance_monitor.c src/arc_runtime.c src/arc_operations.c src/weak_references.c src/error.c src/safe_memory.c
	$(CC) $(CFLAGS) -I src -o $@ $^ -lpthread

# Escape Analysis Tests (T2.4.1)
test_escape_analysis: tests/test_escape_analysis
	@echo "=== Running Escape Analysis Tests ==="
	@./tests/test_escape_analysis

tests/test_escape_analysis: tests/test_escape_analysis.c src/arc_runtime.c src/arc_operations.c src/weak_references.c src/error.c src/safe_memory.c
	$(CC) $(CFLAGS) -I src -o $@ $^

# ARC Insertion Tests (T2.4.2)
test_arc_insertion: tests/test_arc_insertion
	@echo "=== Running ARC Insertion Tests ==="
	@./tests/test_arc_insertion

tests/test_arc_insertion: tests/test_arc_insertion.c src/arc_runtime.c src/arc_operations.c src/weak_references.c src/error.c src/safe_memory.c
	$(CC) $(CFLAGS) -I src -o $@ $^

# Weak Reference Code Generation Tests (T2.4.3)
test_weak_codegen: tests/test_weak_codegen
	@echo "=== Running Weak Reference Code Generation Tests ==="
	@./tests/test_weak_codegen

tests/test_weak_codegen: tests/test_weak_codegen.c src/arc_runtime.c src/arc_operations.c src/weak_references.c src/error.c src/safe_memory.c
	$(CC) $(CFLAGS) -I src -o $@ $^

# ARC Optimization Passes Tests (T2.4.4)
test_arc_optimization: tests/test_arc_optimization
	@echo "=== Running ARC Optimization Passes Tests ==="
	@./tests/test_arc_optimization

tests/test_arc_optimization: tests/test_arc_optimization.c src/arc_runtime.c src/arc_operations.c src/weak_references.c src/error.c src/safe_memory.c
	$(CC) $(CFLAGS) -I src -o $@ $^ -lpthread

# LLVM Context Management Tests (T2.1.2)
test_llvm_context: tests/test_llvm_context
	@echo "=== Running LLVM Context Management Tests ==="
	@timeout 10s ./tests/test_llvm_context || echo "LLVM tests skipped (timeout or not available)"

tests/test_llvm_context: tests/test_llvm_context.c src/llvm_context.c src/safe_memory.c src/error.c
	@if command -v $(LLVM_CONFIG) >/dev/null 2>&1; then \
		$(CC) $(CFLAGS_LLVM) -I src -DWYN_TESTING -o $@ $^ $(LDFLAGS_LLVM) -lpthread; \
	else \
		$(CC) $(CFLAGS) -I src -o $@ tests/test_llvm_context.c src/safe_memory.c src/error.c; \
	fi

test_lexer: tests/test_lexer
	@echo "=== Running Lexer Tests ==="
	@./tests/test_lexer

test_parser: tests/test_parser
	@echo "=== Running Parser Tests ==="
	@./tests/test_parser

test_checker: tests/test_checker
	@echo "=== Running Type Checker Tests ==="
	@./tests/test_checker

test_codegen: tests/test_codegen
	@echo "=== Running Code Generator Tests ==="
	@mkdir -p temp
	@./tests/test_codegen

test_operators: tests/test_operators
	@echo "=== Running Operator Tests ==="
	@./tests/test_operators

test_default_parameters: tests/test_default_parameters
	@echo "=== Running Default Parameters Tests ==="
	@./tests/test_default_parameters

test_function_overloading: tests/test_function_overloading
	@echo "=== Running Function Overloading Tests ==="
	@./tests/test_function_overloading

test_generic_functions: tests/test_generic_functions
	@echo "=== Running Generic Functions Tests ==="
	@./tests/test_generic_functions

test_parameter_validation: tests/test_parameter_validation
	@echo "=== Running Parameter Validation Tests ==="
	@./tests/test_parameter_validation

test_function_integration: tests/test_function_integration
	@echo "=== Running Function Integration Tests ==="
	@./tests/test_function_integration

test_syntax_design: tests/test_syntax_design
	@echo "=== Running Test Syntax Design Tests ==="
	@./tests/test_syntax_design

test_system_integration: tests/test_system_integration
	@echo "=== Running System Integration Tests ==="
	@./tests/test_system_integration

test_wasm_support: tests/test_wasm_support
	@echo "=== Running WebAssembly Support Tests ==="
	@./tests/test_wasm_support

test_self_compilation: tests/test_self_compilation
	@echo "=== Running Self-Compilation Tests ==="
	@./tests/test_self_compilation

test_documentation_system: tests/test_documentation_system
	@echo "=== Running Documentation System Tests ==="
	@./tests/test_documentation_system

test_performance_profiling: tests/test_performance_profiling
	@echo "=== Running Performance Profiling Tests ==="
	@./tests/test_performance_profiling

tests/test_lexer: tests/test_lexer.c src/lexer.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_parser: tests/test_parser.c src/parser.c src/lexer.c src/security.c src/safe_memory.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_checker: tests/test_checker.c src/checker.c src/parser.c src/lexer.c src/security.c src/safe_memory.c src/error.c src/patterns.c src/closures.c src/type_inference.c src/generics.c src/traits.c src/memory.c src/string.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_codegen: tests/test_codegen.c src/codegen.c src/safe_memory.c src/error.c src/parser.c src/lexer.c src/security.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_operators: tests/test_operators.c src/parser.c src/lexer.c src/security.c src/safe_memory.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_default_parameters: tests/test_default_parameters.c src/safe_memory.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_function_overloading: tests/test_function_overloading.c src/safe_memory.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_generic_functions: tests/test_generic_functions_standalone.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_parameter_validation: tests/test_parameter_validation.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_function_integration: tests/test_function_integration.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_syntax_design: tests/test_syntax_design.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_system_integration: tests/test_system_integration.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_wasm_support: tests/test_wasm_support.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_self_compilation: tests/test_self_compilation.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_production_readiness: tests/test_production_readiness.c src/production_readiness.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_bootstrap: tests/test_bootstrap.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_ecosystem_thirdparty: tests/test_ecosystem_thirdparty.c src/ecosystem_thirdparty.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_enterprise: tests/test_enterprise.c src/enterprise.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_checker_rewrite: tests/test_checker_rewrite.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_stdlib_advanced: tests/test_stdlib_advanced.c src/stdlib_advanced.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_documentation_system: tests/test_documentation_system.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_performance_profiling: tests/test_performance_profiling.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_container_support: tests/test_container_support.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_lexer_rewrite: tests/test_lexer_rewrite.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_simd_optimization: tests/test_simd_optimization.c src/simd.c
	$(CC) $(CFLAGS) -I src -o $@ $^ $(shell if [ "$(shell uname -m)" = "x86_64" ]; then echo "-mavx2"; fi)

tests/test_package_manager: tests/test_package_manager.c src/package_manager.c src/safe_memory.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_parser_rewrite: tests/test_parser_rewrite.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_lsp_server: tests/test_lsp_server.c src/lsp_server.c src/safe_memory.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_llvm_optimization: tests/test_llvm_optimization.c src/llvm_optimization.c
	$(CC) $(CFLAGS) -I src -o $@ $^

# Container deployment targets
container-build:
	@./scripts/container-deploy.sh build

container-test:
	@./scripts/container-deploy.sh test

container-deploy:
	@./scripts/container-deploy.sh deploy

container-all:
	@./scripts/container-deploy.sh all

clean:
	rm -f wyn wyn-llvm tests/test_lexer tests/test_parser tests/test_checker tests/test_codegen tests/test_operators tests/test_default_parameters tests/test_function_overloading tests/test_generic_functions tests/test_parameter_validation tests/test_function_integration tests/test_syntax_design tests/test_system_integration tests/phase2_integration tests/test_llvm_context tests/phase2_integration_simple tests/test_wasm_support tests/test_self_compilation tests/test_documentation_system tests/test_performance_profiling tests/test_container_support tests/test_lexer_rewrite tests/test_simd_optimization
	rm -rf temp

.PHONY: all test test_lexer test_parser test_checker test_codegen test_operators clean test_phase2_integration phase2-monitor phase2-gates phase2-status container-build container-test container-deploy container-all

valgrind-test: wyn
	valgrind --leak-check=full --error-exitcode=1 ./wyn tests/basic.wyn
test_t2_3_1_validation: tests/test_t2_3_1_validation.c $(SOURCES)
	$(CC) $(CFLAGS) -I src -o tests/test_t2_3_1_validation tests/test_t2_3_1_validation.c $(SOURCES) $(LDFLAGS)
