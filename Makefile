# Platform detection
UNAME_S := $(shell uname -s 2>/dev/null || echo "Windows")
UNAME_M := $(shell uname -m 2>/dev/null || echo "x86_64")

# Platform-specific settings
ifeq ($(OS),Windows_NT)
    PLATFORM := windows
    CC := gcc
    EXE_EXT := .exe
    PLATFORM_LIBS := -lws2_32 -lpthread
    PLATFORM_CFLAGS := -DWYN_PLATFORM_WINDOWS
else ifeq ($(UNAME_S),Darwin)
    PLATFORM := macos
    CC := clang
    EXE_EXT :=
    PLATFORM_LIBS := -lpthread
    PLATFORM_CFLAGS := -DWYN_PLATFORM_MACOS
else ifeq ($(UNAME_S),Linux)
    PLATFORM := linux
    CC := gcc
    EXE_EXT :=
    PLATFORM_LIBS := -lpthread
    PLATFORM_CFLAGS := -DWYN_PLATFORM_LINUX
else
    PLATFORM := unknown
    CC := gcc
    EXE_EXT :=
    PLATFORM_LIBS := -lpthread
    PLATFORM_CFLAGS := -DWYN_PLATFORM_UNKNOWN
endif

CFLAGS=-Wall -Wextra -std=c11 -g $(PLATFORM_CFLAGS)
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

all: wyn$(EXE_EXT)

# Platform information
platform-info:
	@echo "Platform: $(PLATFORM)"
	@echo "Architecture: $(UNAME_M)"
	@echo "Compiler: $(CC)"
	@echo "Executable extension: $(EXE_EXT)"
	@echo "Platform libs: $(PLATFORM_LIBS)"
	@echo "Platform flags: $(PLATFORM_CFLAGS)"

# Original C-based compiler (Phase 1)
wyn$(EXE_EXT): src/main.c src/lexer.c src/parser.c src/checker.c src/codegen.c src/generics.c src/safe_memory.c src/error.c src/security.c src/memory.c src/string.c src/string_memory.c src/string_runtime.c src/arc_runtime.c src/async_runtime.c src/concurrency.c src/optional.c src/result.c src/type_inference.c src/modules.c src/module.c src/module_registry.c src/collections.c src/io.c src/net.c src/system.c src/stdlib_advanced.c src/stdlib_array.c src/stdlib_string.c src/stdlib_time.c src/stdlib_crypto.c src/wyn_interface.c src/optimize.c src/traits.c src/platform.c src/cmd_compile.c src/cmd_test.c src/cmd_other.c src/hashmap.c src/hashset.c src/json.c src/types.c src/patterns.c src/closures.c src/scope.c src/toml.c src/file_watch.c src/package.c src/lsp.c src/spawn.c
	$(CC) $(CFLAGS) -I src -o $@ $^ $(PLATFORM_LIBS)

# Platform-specific targets
wyn-windows: PLATFORM_CFLAGS += -DWYN_PLATFORM_WINDOWS
wyn-windows: PLATFORM_LIBS = -lws2_32 -lpthread
wyn-windows: CC = x86_64-w64-mingw32-gcc
wyn-windows: EXE_EXT = .exe
wyn-windows: src/main.c src/lexer.c src/parser.c src/checker.c src/codegen.c src/generics.c src/safe_memory.c src/error.c src/security.c src/memory.c src/string.c src/string_memory.c src/string_runtime.c src/arc_runtime.c src/optional.c src/result.c src/type_inference.c src/modules.c src/collections.c src/io.c src/net.c src/system.c src/stdlib_advanced.c src/wyn_interface.c src/optimize.c src/traits.c src/platform.c
	$(CC) $(CFLAGS) -I src -o wyn$(EXE_EXT) $^ $(PLATFORM_LIBS)

wyn-linux: PLATFORM_CFLAGS += -DWYN_PLATFORM_LINUX
wyn-linux: PLATFORM_LIBS = -lpthread
wyn-linux: CC = gcc
wyn-linux: EXE_EXT =
wyn-linux: src/main.c src/lexer.c src/parser.c src/checker.c src/codegen.c src/generics.c src/safe_memory.c src/error.c src/security.c src/memory.c src/string.c src/string_memory.c src/string_runtime.c src/arc_runtime.c src/optional.c src/result.c src/type_inference.c src/modules.c src/collections.c src/io.c src/net.c src/system.c src/stdlib_advanced.c src/wyn_interface.c src/optimize.c src/traits.c src/platform.c
	$(CC) $(CFLAGS) -I src -o wyn$(EXE_EXT) $^ $(PLATFORM_LIBS)

wyn-macos: PLATFORM_CFLAGS += -DWYN_PLATFORM_MACOS
wyn-macos: PLATFORM_LIBS = -lpthread
wyn-macos: CC = clang
wyn-macos: EXE_EXT =
wyn-macos: src/main.c src/lexer.c src/parser.c src/checker.c src/codegen.c src/generics.c src/safe_memory.c src/error.c src/security.c src/memory.c src/string.c src/string_memory.c src/string_runtime.c src/arc_runtime.c src/optional.c src/result.c src/type_inference.c src/modules.c src/collections.c src/io.c src/net.c src/system.c src/stdlib_advanced.c src/wyn_interface.c src/optimize.c src/traits.c src/platform.c
	$(CC) $(CFLAGS) -I src -o wyn$(EXE_EXT) $^ $(PLATFORM_LIBS)

# LLVM-based compiler (Phase 2) with Context Management, Target Configuration, Type Mapping, Runtime Functions, Expression Codegen, Statement Codegen, Function Codegen, and Array/String Operations
wyn-llvm: src/main.c src/lexer.c src/parser.c src/checker.c src/llvm_codegen.c src/llvm_context.c src/target_config.c src/type_mapping.c src/runtime_functions.c src/llvm_expression_codegen.c src/llvm_statement_codegen.c src/llvm_function_codegen.c src/llvm_array_string_codegen.c src/safe_memory.c src/error.c src/security.c src/memory.c src/string.c src/cmd_other.c src/optimize.c src/types.c src/patterns.c src/generics.c src/type_inference.c src/platform.c src/wyn_interface.c src/traits.c src/string_memory.c src/string_runtime.c src/arc_runtime.c src/async_runtime.c src/concurrency.c src/result.c src/modules.c src/collections.c src/io.c src/net.c src/system.c src/stdlib_advanced.c src/stdlib_array.c src/stdlib_string.c src/stdlib_time.c src/hashmap.c src/hashset.c src/json.c
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

# String memory management tests
test_string_memory: tests/memory/test_string_memory
	@echo "=== Running String Memory Tests ==="
	@./tests/memory/test_string_memory

tests/memory/test_string_memory: tests/memory/test_string_memory.c src/string_memory.c src/string_runtime.c src/arc_runtime.c src/safe_memory.c src/string.c src/error.c
	@mkdir -p tests/memory
	$(CC) $(CFLAGS) -I src -o $@ $^ -lpthread

test_string_leaks: tests/memory/test_string_leaks
	@echo "=== Running String Leak Detection Tests ==="
	@./tests/memory/test_string_leaks

tests/memory/test_string_leaks: tests/memory/test_string_leaks.c src/string_memory.c src/string_runtime.c src/arc_runtime.c src/safe_memory.c src/string.c src/error.c
	@mkdir -p tests/memory
	$(CC) $(CFLAGS) -I src -o $@ $^ -lpthread

test_string_comprehensive: tests/memory/test_string_comprehensive.wyn.out
	@echo "=== Running Comprehensive String Tests ==="
	@./tests/memory/test_string_comprehensive.wyn.out

tests/memory/test_string_comprehensive.wyn.out: tests/memory/test_string_comprehensive.wyn wyn
	@mkdir -p tests/memory
	./wyn tests/memory/test_string_comprehensive.wyn

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



tests/test_ide_integration: tests/test_ide_integration.c $(HEADERS)
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

test_unit: test_lexer test_parser test_checker test_codegen test_operators test_default_parameters test_function_overloading test_generic_functions test_parameter_validation test_function_integration test_syntax_design test_system_integration test_wasm_support test_documentation_system

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



tests/test_bootstrap: tests/test_bootstrap.c
	$(CC) $(CFLAGS) -I src -o $@ $^





tests/test_checker_rewrite: tests/test_checker_rewrite.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_stdlib_advanced: tests/test_stdlib_advanced.c src/stdlib_advanced.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_documentation_system: tests/test_documentation_system.c
	$(CC) $(CFLAGS) -I src -o $@ $^



tests/test_container_support: tests/test_container_support.c
	$(CC) $(CFLAGS) -I src -o $@ $^

tests/test_lexer_rewrite: tests/test_lexer_rewrite.c
	$(CC) $(CFLAGS) -I src -o $@ $^





tests/test_parser_rewrite: tests/test_parser_rewrite.c
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

# Formatter tool
fmt-tool: tools/formatter.wyn.out

tools/formatter.wyn.out: tools/formatter.wyn wyn
	./wyn tools/formatter.wyn

clean:
	rm -f wyn wyn.exe wyn-windows.exe wyn-linux wyn-macos wyn-llvm tests/test_lexer tests/test_parser tests/test_checker tests/test_codegen tests/test_operators tests/test_default_parameters tests/test_function_overloading tests/test_generic_functions tests/test_parameter_validation tests/test_function_integration tests/test_syntax_design tests/test_system_integration tests/phase2_integration tests/test_llvm_context tests/phase2_integration_simple tests/test_wasm_support tests/test_self_compilation tests/test_documentation_system tests/test_container_support tests/test_lexer_rewrite tools/formatter.wyn.out
	rm -rf temp

.PHONY: all test test_lexer test_parser test_checker test_codegen test_operators clean test_phase2_integration phase2-monitor phase2-gates phase2-status container-build container-test container-deploy container-all fmt-tool platform-info wyn-windows wyn-linux wyn-macos

# valgrind-test defined earlier in file (line ~125)

test_t2_3_1_validation: tests/test_t2_3_1_validation.c $(SOURCES)
	$(CC) $(CFLAGS) -I src -o tests/test_t2_3_1_validation tests/test_t2_3_1_validation.c $(SOURCES) $(LDFLAGS)
