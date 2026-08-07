# Platform detection
UNAME_S := $(shell uname -s 2>/dev/null || echo "Windows")
UNAME_M := $(shell uname -m 2>/dev/null || echo "x86_64")

# Platform-specific settings
ifeq ($(OS),Windows_NT)
    PLATFORM := windows
    CC := gcc
    EXE_EXT := .exe
    PLATFORM_LIBS := -lws2_32 -lpthread -lm
    # -include forces src/mingw_unistd_fix.h to the top of EVERY translation unit,
    # before any #include can pull in <unistd.h>. 21 of our .c files include
    # <unistd.h>, and mingw defines ftruncate there as a __CRT_INLINE body calling
    # _chsize - an underscore-prefixed CRT extension that -std=c11 (strict ANSI)
    # hides, so gcc 14+ hard-errors on the implicit declaration. Patching each file
    # by hand is whack-a-mole: the failing release log only named the first three
    # (make stops early), and a 4th (src/cpkg.c) was found only by enumerating
    # CORE_SRCS. Forcing the include once cannot be got wrong by include ORDER and
    # cannot be missed by a new file.
    PLATFORM_CFLAGS := -DWYN_PLATFORM_WINDOWS -include src/mingw_unistd_fix.h
else ifeq ($(UNAME_S),Darwin)
    PLATFORM := macos
    CC := clang
    EXE_EXT :=
    PLATFORM_LIBS := -lpthread -lm
    PLATFORM_CFLAGS := -DWYN_PLATFORM_MACOS
else ifeq ($(UNAME_S),Linux)
    PLATFORM := linux
    CC := gcc
    EXE_EXT :=
    PLATFORM_LIBS := -lpthread -lm
    PLATFORM_CFLAGS := -DWYN_PLATFORM_LINUX
else
    PLATFORM := unknown
    CC := gcc
    EXE_EXT :=
    PLATFORM_LIBS := -lpthread -lm
    PLATFORM_CFLAGS := -DWYN_PLATFORM_UNKNOWN
endif

# OPT is the debug/optimization level, split out so a release build can override
# JUST this (`make OPT=-O2`) instead of replacing CFLAGS wholesale. A command-line
# CFLAGS= beats this assignment entirely and silently drops $(PLATFORM_CFLAGS).
# That is exactly how the v1.20.0 release build lost -DWYN_PLATFORM_WINDOWS and
# failed on Windows alone, inside mingw's own unistd.h (the __CRT_INLINE ftruncate
# body calls _chsize, which -O2 causes to be emitted and gcc 14+ treats as a hard
# error when implicitly declared). Override OPT, never CFLAGS.
OPT?=-g

# RELEASE_BUILD marks a binary as an official release. It DEFAULTS TO OFF, so every
# ordinary `make` produces a binary that reports e.g. "v1.20.0-dev". Only the release
# workflow passes RELEASE_BUILD=1, which drops the suffix.
#
# The default is off ON PURPOSE. A dev build and a released build previously reported
# the identical string, so there was no way to tell whether the compiler you were
# running contained a fix — the Wynshop dogfood session hit exactly this: its suite
# was green against the INSTALLED v1.20.0 binary, which did not contain the fixes
# under test, and only a byte-size comparison revealed it.
#
# Deliberately NOT `git describe --exact-match`: release.yml checks out with
# actions/checkout@v4 at fetch-depth 1 and does not fetch tags, so describe would
# fail there and label genuine releases as "-dev". An explicit opt-in flag cannot
# fail that way, and if it is ever forgotten the error is in the honest direction:
# a real release mislabelled as dev, never a dev build passing itself off as
# official.
RELEASE_BUILD?=0
ifeq ($(RELEASE_BUILD),1)
    VERSION_SUFFIX=
else
    VERSION_SUFFIX=-dev
endif

CFLAGS=-Wall -Wextra -std=c11 -D_GNU_SOURCE $(OPT) $(PLATFORM_CFLAGS) -DWYN_VERSION=\"$(shell cat VERSION 2>/dev/null || echo 0.0.0)$(VERSION_SUFFIX)\"
OPTFLAGS=-O2

all: wyn$(EXE_EXT) runtime

# Platform information
platform-info:
	@echo "Platform: $(PLATFORM)"
	@echo "Architecture: $(UNAME_M)"
	@echo "Compiler: $(CC)"
	@echo "Executable extension: $(EXE_EXT)"
	@echo "Platform libs: $(PLATFORM_LIBS)"
	@echo "Platform flags: $(PLATFORM_CFLAGS)"

# C-based compiler
CORE_SRCS = src/main.c src/lexer.c src/parser.c src/checker.c src/codegen.c src/generics.c src/safe_memory.c src/error.c src/security.c src/memory.c src/string_runtime.c src/arc_runtime.c src/async_runtime.c src/concurrency.c src/optional.c src/result.c src/type_inference.c src/module_loader.c src/module.c src/module_registry.c src/io.c src/net.c src/stdlib_array.c src/stdlib_string.c src/stdlib_time.c src/stdlib_crypto.c src/stdlib_math.c src/wyn_interface.c src/optimize.c src/traits.c src/platform.c src/cmd_compile.c src/cmd_test.c src/cmd_other.c src/cmd_ui.c src/hashmap.c src/hashset.c src/json.c src/types.c src/patterns.c  src/toml.c src/package.c src/pkgspec.c src/lsp.c src/bindgen.c src/cpkg.c src/tcc_backend.c src/wyn_arena.c src/wyn_rc.c src/coroutine.c
# NOTE: src/spawn.c is deliberately NOT linked into the compiler. The compiler
# only registers Task_send/Task_recv/etc. as builtin NAME strings (checker.c) -
# it never calls the spawn runtime in-process; compiled programs get it from
# runtime/libwyn_rt.a. Linking spawn.c here forced the whole thread-pool
# scheduler (spawn_fast.c: wyn_sched_pump_one/inflight) into the compiler too.

# codegen.c #includes these .c files directly (single translation unit), so they
# are NOT in CORE_SRCS (compiling them standalone would duplicate symbols). List
# them here as prerequisites so editing one triggers a rebuild - otherwise make
# sees no changed prerequisite and silently keeps a stale binary.
# Sources #included directly into another translation unit (codegen.c pulls in the
# codegen_* files, checker.c pulls in checker_builtins.c). They are NOT in CORE_SRCS -
# compiling them standalone would duplicate symbols - but they must be prerequisites,
# or make sees no changed prerequisite and silently keeps a stale binary.
TU_INCLUDED_SRCS = src/codegen_expr.c src/codegen_stmt.c src/codegen_lambda.c src/codegen_program.c \
                   src/checker_builtins.c

wyn$(EXE_EXT): $(CORE_SRCS) $(TU_INCLUDED_SRCS) $(wildcard src/*.h)
	$(CC) $(CFLAGS) -I src -I vendor/tcc/include -I vendor/minicoro -o $@ $(CORE_SRCS) vendor/tcc/lib/libtcc.a $(PLATFORM_LIBS)

# Platform-specific targets
wyn-windows: PLATFORM_CFLAGS += -DWYN_PLATFORM_WINDOWS
wyn-windows: PLATFORM_LIBS = -lws2_32 -lpthread -lm
wyn-windows: CC = x86_64-w64-mingw32-gcc
wyn-windows: EXE_EXT = .exe
wyn-windows: src/main.c src/lexer.c src/parser.c src/checker.c src/codegen.c src/generics.c src/safe_memory.c src/error.c src/security.c src/memory.c src/string_runtime.c src/arc_runtime.c src/optional.c src/result.c src/type_inference.c src/module_loader.c src/io.c src/net.c src/wyn_interface.c src/optimize.c src/traits.c src/platform.c
	$(CC) $(CFLAGS) -I src -o wyn$(EXE_EXT) $^ $(PLATFORM_LIBS)

wyn-linux: PLATFORM_CFLAGS += -DWYN_PLATFORM_LINUX
wyn-linux: PLATFORM_LIBS = -lpthread -lm
wyn-linux: CC = gcc
wyn-linux: EXE_EXT =
wyn-linux: src/main.c src/lexer.c src/parser.c src/checker.c src/codegen.c src/generics.c src/safe_memory.c src/error.c src/security.c src/memory.c src/string_runtime.c src/arc_runtime.c src/optional.c src/result.c src/type_inference.c src/module_loader.c src/io.c src/net.c src/wyn_interface.c src/optimize.c src/traits.c src/platform.c
	$(CC) $(CFLAGS) -I src -o wyn$(EXE_EXT) $^ $(PLATFORM_LIBS)

wyn-macos: PLATFORM_CFLAGS += -DWYN_PLATFORM_MACOS
wyn-macos: PLATFORM_LIBS = -lpthread -lm
wyn-macos: CC = clang
wyn-macos: EXE_EXT =
wyn-macos: src/main.c src/lexer.c src/parser.c src/checker.c src/codegen.c src/generics.c src/safe_memory.c src/error.c src/security.c src/memory.c src/string_runtime.c src/arc_runtime.c src/optional.c src/result.c src/type_inference.c src/module_loader.c src/io.c src/net.c src/wyn_interface.c src/optimize.c src/traits.c src/platform.c
	$(CC) $(CFLAGS) -I src -o wyn$(EXE_EXT) $^ $(PLATFORM_LIBS)

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

wyn-release: src/main.c src/lexer.c src/parser.c src/checker.c src/codegen.c src/safe_memory.c src/error.c src/security.c src/memory.c
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

tests/memory/test_string_memory: tests/memory/test_string_memory.c src/string_runtime.c src/arc_runtime.c src/safe_memory.c src/error.c
	@mkdir -p tests/memory
	$(CC) $(CFLAGS) -I src -o $@ $^ -lpthread

test_string_leaks: tests/memory/test_string_leaks
	@echo "=== Running String Leak Detection Tests ==="
	@./tests/memory/test_string_leaks

tests/memory/test_string_leaks: tests/memory/test_string_leaks.c src/string_runtime.c src/arc_runtime.c src/safe_memory.c src/error.c
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

# THE full test suite - and, as of 2026-07, what CI actually runs on every PR
# (Linux + macOS-arm64 + macOS-x64). It drives, in order:
#   * run_bdd.sh          - tests/expect/ + tests/regression/ `// EXPECT:` checks
#   * golden-C snapshots, GPU, bindgen, cpkg, sqlite, pkg, pkg-audit, LSP
#   * tests/errors/       - 40+ negative / behavioral / soundness gates
#   * fuzz smoke
#   * tests/stdlib/       - allowlist-gated (see the end of the recipe)
#
# Do NOT let CI drift back to running only run_bdd.sh. It did exactly that until
# 2026-07, so everything in the list above except run_bdd.sh was ungated - which
# is how tests/errors/run_channel_deadlock_test.sh shipped failing at the
# v1.20.0 tag while the changelog claimed the "full suite" was green.
#
# Each line is a separate recipe command, so make stops at the FIRST failing
# suite and exits nonzero. Consequence worth knowing: later suites are then not
# run at all, so fix failures top-down.
#
# (The old test_unit/test_integration/test_stdlib/... targets referenced C unit
# sources and shell scripts that no longer exist; they are gone. The separate
# run_tests_parallel.sh needs a tests/test_list.txt that isn't in the tree, so
# it's not wired into this target - run it manually if you regenerate the list.)
# check-fast: the EDIT-LOOP gate, not a merge gate. Target <= 30s.
#
# WHY: `make test` chains ~100 steps and takes ~9 minutes; run_bdd.sh alone is
# ~250s. That cost is per-iteration during debugging, so a 10-round session spends
# over an hour waiting. This runs the two things that actually catch codegen
# mistakes fast: the build (0 warnings) and the golden-C snapshots, which pin the
# generated C and are exactly what the soundness work perturbs.
#
# THIS IS NOT A SUBSTITUTE FOR `make test`, AND MUST NOT BECOME ONE. `make test`
# stays the merge gate and the source of truth. The lesson from the v1.20.0 cycle
# (CI never ran `make test`, so ~80 test files were ungated for months) is that a
# suite people trust but which does not run everything is worse than no suite. Run
# check-fast while editing; run `make test` before you push.
check-fast: wyn
	@echo "=== Golden-C snapshots (pins generated C) ==="
	@WYN=./wyn bash tests/golden/run_golden_tests.sh
	@echo ""
	@echo "check-fast passed. This is NOT 'make test' - run that before pushing."

test: wyn
	@echo "=== Running assertion tests (run_bdd.sh) ==="
	@WYN=./wyn bash tests/run_bdd.sh
	@echo "=== Running golden-C snapshot tests ==="
	@WYN=./wyn bash tests/golden/run_golden_tests.sh
	@echo "=== Running GPU transparent-dispatch test ==="
	@WYN=./wyn bash tests/gpu/run_gpu_test.sh
	@echo "=== Running bindgen test ==="
	@WYN=./wyn bash tests/bindgen/run_bindgen_test.sh
	@echo "=== Running module struct-array return test ==="
	@WYN=./wyn bash tests/module_tests/run_struct_array_return_test.sh
	@echo "=== Running lambda-in-imported-module test ==="
	@WYN=./wyn bash tests/module_tests/run_lambda_in_module_test.sh
	@echo "=== Running module global-initializer test ==="
	@WYN=./wyn bash tests/module_tests/run_module_global_init_test.sh
	@echo "=== Running src/ module-layout resolution test ==="
	@WYN=./wyn bash tests/module_tests/run_src_layout_test.sh
	@echo "=== Running module extern-fn naming test ==="
	@WYN=./wyn bash tests/module_tests/run_extern_prefix_test.sh
	@echo "=== Running cross-module struct/enum type test ==="
	@WYN=./wyn bash tests/module_tests/run_cross_module_type_test.sh
	@echo "=== Running imported-type checker test ==="
	@WYN=./wyn bash tests/module_tests/run_imported_type_test.sh
	@echo "=== Running multi-module package test ==="
	@WYN=./wyn bash tests/module_tests/run_pkg_multimodule_test.sh
	@echo "=== Running argv-forwarding test ==="
	@WYN=./wyn bash tests/errors/run_argv_forward_test.sh
	@echo "=== Running cc-error isolation test (parallel wyn run) ==="
	@WYN=./wyn bash tests/errors/run_cc_err_isolation_test.sh
	@echo "=== Running unresolved-import abort test ==="
	@WYN=./wyn bash tests/errors/run_unresolved_import_test.sh
	@echo "=== Running selective-import alias rejection test ==="
	@WYN=./wyn bash tests/errors/run_selective_import_alias_test.sh
	@echo "=== Running for-in-string check-time rejection test ==="
	@WYN=./wyn bash tests/errors/run_for_in_string_test.sh
	@echo "=== Running run-cache import-staleness test ==="
	@WYN=./wyn bash tests/errors/run_run_cache_imports_test.sh
	@echo "=== Running --release link/parity test ==="
	@WYN=./wyn bash tests/errors/run_release_link_test.sh
	@echo "=== Running native app-bundle (wyn build --app) test ==="
	@WYN=./wyn bash tests/errors/run_app_bundle_test.sh
	@echo "=== Running assert_eq float-comparison test ==="
	@WYN=./wyn bash tests/errors/run_assert_eq_float_test.sh
	@echo "=== Running wyn design subcommand test ==="
	@WYN=./wyn bash tests/errors/run_design_cmd_test.sh
	@echo "=== Running test-name percent-escaping test ==="
	@WYN=./wyn bash tests/errors/run_test_name_percent_test.sh
	@echo "=== Running C-package (wyn add) test ==="
	@WYN=./wyn bash tests/cpkg/run_cpkg_test.sh
	@echo "=== Running SQLite dogfood (wyn add sqlite3) test ==="
	@WYN=./wyn bash tests/cpkg/run_sqlite_test.sh
	@echo "=== Running git-deps (wyn add <url>) test ==="
	@WYN=./wyn bash tests/pkg/run_pkg_test.sh
	@echo "=== Running pkg audit test ==="
	@WYN=./wyn bash tests/pkg/run_audit_test.sh
	@echo "=== Running LSP protocol test ==="
	@WYN=./wyn bash tests/lsp/run_lsp_test.sh
	@echo "=== Running removed-syntax negative test ==="
	@WYN=./wyn bash tests/errors/run_removed_syntax_test.sh
	@echo "=== Running wyn fix migrator test ==="
	@WYN=./wyn bash tests/errors/run_fix_test.sh
	@echo "=== Running lambda param-type test ==="
	@WYN=./wyn bash tests/errors/run_lambda_param_test.sh
	@echo "=== Running recursive-struct negative test ==="
	@WYN=./wyn bash tests/errors/run_recursive_struct_test.sh
	@echo "=== Running nested-aggregate feature+gate test ==="
	@WYN=./wyn bash tests/errors/run_nested_aggregate_test.sh
	@echo "=== Running generic-enum negative test ==="
	@WYN=./wyn bash tests/errors/run_generic_enum_test.sh
	@echo "=== Running unknown-method negative test ==="
	@WYN=./wyn bash tests/errors/run_unknown_method_test.sh
	@echo "=== Running bug-batch-2 test ==="
	@WYN=./wyn bash tests/errors/run_bug_batch2_test.sh
	@echo "=== Running user test-runner test ==="
	@WYN=./wyn bash tests/errors/run_user_test_runner_test.sh
	@echo "=== Running module-codegen (M1-M4) test ==="
	@WYN=./wyn bash tests/errors/run_module_codegen_test.sh
	@echo "=== Running pub-visibility enforcement test ==="
	@WYN=./wyn bash tests/errors/run_pub_visibility_test.sh
	@echo "=== Running module-call arity test ==="
	@WYN=./wyn bash tests/errors/run_module_arity_test.sh
	@echo "=== Running bool/int argument test ==="
	@WYN=./wyn bash tests/errors/run_bool_int_arg_test.sh
	@echo "=== Running module struct type test ==="
	@WYN=./wyn bash tests/errors/run_module_struct_test.sh
	@echo "=== Running clean-output test ==="
	@WYN=./wyn bash tests/errors/run_clean_output_test.sh
	@echo "=== Running interpolated-receiver method test ==="
	@WYN=./wyn bash tests/errors/run_interp_method_test.sh
	@echo "=== Running UTF-8 padding test ==="
	@WYN=./wyn bash tests/errors/run_pad_utf8_test.sh
	@echo "=== Running struct string-field ownership test ==="
	@WYN=./wyn bash tests/errors/run_struct_string_field_test.sh
	@echo "=== Running parenthesized-condition test ==="
	@WYN=./wyn bash tests/errors/run_paren_condition_test.sh
	@echo "=== Running enum variant-name collision test ==="
	@WYN=./wyn bash tests/errors/run_enum_variant_name_test.sh
	@echo "=== Running lambda-in-interpolation test ==="
	@WYN=./wyn bash tests/errors/run_lambda_interp_test.sh
	@echo "=== Running struct-array return-type test ==="
	@WYN=./wyn bash tests/errors/run_struct_array_return_test.sh
	@echo "=== Running int-array var-leak test ==="
	@WYN=./wyn bash tests/errors/run_int_array_var_leak_test.sh
	@echo "=== Running mut-self mutation test ==="
	@WYN=./wyn bash tests/errors/run_mut_self_test.sh
	@echo "=== Running shadowed-string retain test ==="
	@WYN=./wyn bash tests/errors/run_shadowed_string_retain_test.sh
	@echo "=== Running global string-assign leak test ==="
	@WYN=./wyn bash tests/errors/run_global_string_leak_test.sh
	@echo "=== Running global string-copy lifetime test ==="
	@WYN=./wyn bash tests/errors/run_global_string_copy_test.sh
	@echo "=== Running pub-declaration parse test ==="
	@WYN=./wyn bash tests/errors/run_pub_decl_test.sh
	@echo "=== Running enum-value representation test ==="
	@WYN=./wyn bash tests/errors/run_enum_value_repr_test.sh
	@echo "=== Running handle-in-array-literal test ==="
	@WYN=./wyn bash tests/errors/run_handle_in_array_literal_test.sh
	@echo "=== Running module enum-type test ==="
	@WYN=./wyn bash tests/errors/run_module_enum_type_test.sh
	@echo "=== Running module array-param test ==="
	@WYN=./wyn bash tests/errors/run_module_array_param_test.sh
	@echo "=== Running import-list size test ==="
	@WYN=./wyn bash tests/errors/run_import_list_test.sh
	@echo "=== Running GUI build-link test ==="
	@WYN=./wyn bash tests/errors/run_gui_build_test.sh
	@echo "=== Running var-type-scope test ==="
	@WYN=./wyn bash tests/errors/run_var_type_scope_test.sh
	@echo "=== Running bool-method formatting test ==="
	@WYN=./wyn bash tests/errors/run_bool_method_format_test.sh
	@echo "=== Running python/shared-library build test ==="
	@WYN=./wyn bash tests/errors/run_python_lib_test.sh
	@echo "=== Running pkg search test ==="
	@WYN=./wyn bash tests/errors/run_search_test.sh
	@echo "=== Running scaffold (wyn new) test ==="
	@WYN=./wyn bash tests/errors/run_scaffold_test.sh
	@echo "=== Running bindgen robustness test ==="
	@WYN=./wyn bash tests/errors/run_bindgen_test.sh
	@echo "=== Running parser stability test ==="
	@WYN=./wyn bash tests/errors/run_parser_stability_test.sh
	@echo "=== Running unterminated-string test ==="
	@WYN=./wyn bash tests/errors/run_unterminated_string_test.sh
	@echo "=== Running struct-eq negative test ==="
	@WYN=./wyn bash tests/errors/run_struct_eq_test.sh
	@echo "=== Running cross-type comparison soundness test ==="
	@WYN=./wyn bash tests/errors/run_cross_type_cmp_test.sh
	@echo "=== Running data-race + file-IO soundness test ==="
	@WYN=./wyn bash tests/errors/run_race_and_io_soundness_test.sh
	@echo "=== Running struct-field validation test ==="
	@WYN=./wyn bash tests/errors/run_struct_field_test.sh
	@echo "=== Running map missing-key panic test ==="
	@WYN=./wyn bash tests/errors/run_map_missing_key_test.sh
	@echo "=== Running nesting-depth guard test ==="
	@WYN=./wyn bash tests/errors/run_nesting_depth_test.sh
	@echo "=== Running empty-radix-literal test ==="
	@WYN=./wyn bash tests/errors/run_empty_radix_literal_test.sh
	@echo "=== Running doctor + version honesty test ==="
	@WYN=./wyn bash tests/errors/run_doctor_version_test.sh
	@echo "=== Running select-deadlock test ==="
	@WYN=./wyn bash tests/errors/run_select_deadlock_test.sh
	@echo "=== Running channel-deadlock test ==="
	@WYN=./wyn bash tests/errors/run_channel_deadlock_test.sh
	@echo "=== Running collection type-safety test ==="
	@WYN=./wyn bash tests/errors/run_collection_type_test.sh
	@echo "=== Running silent-wrong-answer test ==="
	@WYN=./wyn bash tests/errors/run_silent_wrong_test.sh
	@echo "=== Running diagnostic-location + panic-path test ==="
	@WYN=./wyn bash tests/errors/run_diagnostic_location_test.sh
	@echo "=== Running checker-soundness gate (K5-K11) test ==="
	@WYN=./wyn bash tests/errors/run_checker_soundness_test.sh
	@echo "=== Running await_all element-typing gate ==="
	@WYN=./wyn bash tests/errors/run_await_all_type_test.sh
	@echo "=== Running crucible-P0 (fatal-by-default) test ==="
	@WYN=./wyn bash tests/errors/run_crucible_p0_test.sh
	@echo "=== Running CLI DX test ==="
	@WYN=./wyn bash tests/errors/run_cli_dx_test.sh
	@echo "=== Running wyn-run orphan-child test ==="
	@WYN=./wyn bash tests/errors/run_orphan_child_test.sh
	@echo "=== Running sqlite link-order gate ==="
	@WYN=./wyn bash tests/errors/run_sqlite_link_order_test.sh
	@echo "=== Running wyn ui coverage test ==="
	@WYN=./wyn bash tests/errors/run_ui_coverage_test.sh
	@echo "=== Running install-layout canary ==="
	@WYN=./wyn bash tests/errors/run_install_layout_test.sh
	@echo "=== Running unsupported-field-type honesty gates ==="
	@WYN=./wyn bash tests/errors/run_unsupported_field_type_test.sh
	@echo "=== Running function-typed struct field test ==="
	@WYN=./wyn bash tests/errors/run_fn_field_test.sh
	@echo "=== Running unused-variable shadowing test ==="
	@WYN=./wyn bash tests/errors/run_unused_shadow_test.sh
	@echo "=== Running StringBuilder aliasing test ==="
	@WYN=./wyn bash tests/errors/run_stringbuilder_test.sh
	@echo "=== Running Task.select diagnostic gate ==="
	@WYN=./wyn bash tests/errors/run_task_select_diagnostic_test.sh
	@echo "=== Running HTTP server concurrent-load gate ==="
	@WYN=./wyn bash tests/errors/run_http_server_load_test.sh
	@echo "=== Running fuzz smoke (seed 1) ==="
	@WYN=./wyn bash tests/fuzz/run_fuzz.sh 1 60
	# tests/stdlib/ (68 files) used to be run by NOTHING - not run_bdd.sh (which
	# only walks expect/ + regression/), not this target, and not
	# run_tests_parallel.sh (its tests/test_list.txt isn't in the tree, so it
	# executes zero tests). It is gated through a known-failure ALLOWLIST
	# (tests/stdlib/known_failures.txt) because the suite is not clean yet: any
	# NEW breakage fails, the listed entries are a visible, shrinking debt list.
	# Runs LAST because it is the slowest and the only non-deterministic part.
	@echo "=== Running stdlib suite (allowlist-gated) ==="
	@WYN=./wyn bash tests/stdlib/run_stdlib_tests.sh

# Alias kept for muscle memory.
test_bdd: test

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

tests/test_checker: tests/test_checker.c src/checker.c src/parser.c src/lexer.c src/security.c src/safe_memory.c src/error.c src/patterns.c src/type_inference.c src/generics.c src/traits.c src/memory.c
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

tests/test_documentation_system: tests/test_documentation_system.c
	$(CC) $(CFLAGS) -I src -o $@ $^

# Coroutine unit tests
test_coroutine: tests/test_coroutine
	@echo "=== Running Coroutine Tests ==="
	@./tests/test_coroutine

tests/test_coroutine: tests/test_coroutine.c src/coroutine.c
	$(CC) $(CFLAGS) -I src -I vendor/minicoro -o $@ $^ -lpthread

test_coroutine_advanced: tests/test_coroutine_advanced
	@echo "=== Running Advanced Coroutine Tests ==="
	@./tests/test_coroutine_advanced

tests/test_coroutine_advanced: tests/test_coroutine_advanced.c src/coroutine.c src/spawn_fast.c src/future.c src/io_loop.c src/spawn.c
	$(CC) $(CFLAGS) -I src -I vendor/minicoro -o $@ $^ -lpthread



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


# Precompile runtime library for fast compilation
# runtime_exports.c is the ONLY translation unit that includes wyn_runtime.h, so
# it is where every runtime function defined *in that header* becomes a linkable
# symbol. The default path does not need it (the generated program .c includes
# wyn_runtime.h itself and so defines them all locally), but `--release` emits
# `#include "wyn_runtime_slim.h"` - declarations only - and then has nothing to
# link against. Omitting it here made EVERY --release build fail at link
# (Math_pow, System_args, __wyn_argc, print_float_no_nl, array_push_float, ...).
# It is safe on the default path because the linker only pulls an archive member
# in to resolve an undefined symbol, and the program's own object already defines
# all of them; see tests/regression/test_release_link.sh, which guards both paths.
# Additional .c files provide functions NOT in the header
RT_SRCS = src/wyn_arena.c src/wyn_rc.c src/runtime_exports.c src/wyn_wrapper.c \
          src/wyn_interface.c src/coroutine.c src/spawn_fast.c src/spawn.c src/future.c \
          src/io.c src/io_loop.c src/optional.c src/result.c \
          src/arc_runtime.c src/concurrency.c src/async_runtime.c \
          src/safe_memory.c src/error.c src/string_runtime.c \
          src/hashmap.c src/hashset.c src/json.c \
          src/stdlib_runtime.c src/hashmap_runtime.c \
          src/stdlib_string.c src/stdlib_array.c src/stdlib_time.c \
          src/stdlib_crypto.c src/stdlib_math.c \
          src/net.c src/net_runtime.c src/net_advanced.c \
          src/test_runtime.c src/file_io_simple.c src/stdlib_enhanced.c

# The runtime library must be rebuilt whenever any runtime source (or a header
# they include, notably wyn_runtime.h/io_loop.h) changes - otherwise compiled
# programs silently link a stale libwyn_rt.a. Depend on the sources + headers so
# `make` detects the change instead of reporting "Nothing to be done".
runtime: runtime/libwyn_rt.a
runtime/libwyn_rt.a: $(RT_SRCS) $(wildcard src/*.h) | wyn$(EXE_EXT)
	@echo "Building runtime library..."
	@mkdir -p runtime/obj
	@set -e; for f in $(RT_SRCS); do \
		$(CC) -std=c11 -O2 -w -D_GNU_SOURCE -I src -I vendor/minicoro \
		-c $$f -o runtime/obj/$$(basename $$f .c).o; \
	done
	@# `ar r` REPLACES members in an existing archive. If a stale runtime/obj/
	@# holds an .o from a previous build that this loop did not just recompile
	@# (or the archive holds a member whose source is gone), that stale code
	@# silently survives into the lib and every compiled program links it.
	@# This has already cost real debugging time: a runtime fix appeared to have
	@# no effect, and a perf regression looked unreproducible, because the lib
	@# still contained pre-fix objects. Build the archive from scratch instead.
	@rm -f runtime/libwyn_rt.a
	@ar rcs runtime/libwyn_rt.a runtime/obj/*.o
	@echo "Built runtime/libwyn_rt.a ($$(du -h runtime/libwyn_rt.a | cut -f1))"

# ASan-instrumented runtime: compile RT_SRCS with -fsanitize=address into a
# separate lib, then build+run a set of representative tests against it. The
# RC/string/IO bugs live in the RUNTIME - `make debug-memory` only instruments
# the compiler, so this is the check that has caught every real UAF. Used by
# the sanitizer CI job; run locally with `make asan-runtime-test`.
runtime-asan: runtime/libwyn_rt_asan.a
runtime/libwyn_rt_asan.a: $(RT_SRCS) $(wildcard src/*.h)
	@echo "Building ASan runtime library..."
	@mkdir -p runtime/obj_asan
	@set -e; for f in $(RT_SRCS); do \
		$(CC) -std=c11 -O1 -g -w -fsanitize=address -fno-omit-frame-pointer \
		-D_GNU_SOURCE -I src -I vendor/minicoro \
		-c $$f -o runtime/obj_asan/$$(basename $$f .c).o; \
	done
	@ar rcs runtime/libwyn_rt_asan.a runtime/obj_asan/*.o
	@echo "Built runtime/libwyn_rt_asan.a"

# Compile a representative test set's generated C against the ASan runtime
# and run each binary. Any ASan report (UAF, overflow, leak-at-exit is NOT
# checked - detect_leaks=0 keeps signal high) fails the target.
ASAN_TESTS = tests/expect/test_string_utf8.wyn \
             tests/expect/test_lambda_typed_variants.wyn \
             tests/expect/test_arrow_lambda.wyn \
             tests/expect/test_string_lambda.wyn \
             tests/expect/test_reduce_both_orders.wyn \
             tests/expect/test_match_stmt_patterns.wyn \
             tests/expect/test_println_rich_types.wyn \
             tests/expect/test_closure_env_lifetime.wyn \
             tests/regression/test_closure_copy_call.wyn \
             tests/expect/test_channels.wyn \
             tests/expect/test_parallel.wyn \
             tests/expect/test_await_twice.wyn \
             tests/expect/test_select_arms.wyn \
             tests/regression/test_map_get_default.wyn \
             tests/regression/test_index_compound_assign.wyn \
             tests/regression/test_float_array_reductions.wyn \
             tests/regression/test_map_value_overwrite_read.wyn \
             tests/regression/test_stringbuilder_many.wyn \
             tests/regression/test_await_all_string_results.wyn \
             tests/regression/test_await_all_float_results.wyn \
             tests/regression/test_await_all_struct_results.wyn \
             tests/regression/test_retain_on_return.wyn \
             tests/regression/test_rc_stage2_reconcile.wyn

asan-runtime-test: wyn$(EXE_EXT) runtime/libwyn_rt_asan.a
	@echo "=== ASan runtime test (representative set) ==="
	@set -e; for t in $(ASAN_TESTS); do \
		[ -f $$t ] || continue; \
		./wyn build $$t --debug >/dev/null 2>&1 || { echo "  skip (build) $$t"; continue; }; \
		$(CC) -std=c11 -O0 -g -w -fsanitize=address -fno-omit-frame-pointer \
			-I src -o $${t%.wyn}.asan $$t.c runtime/libwyn_rt_asan.a $(PLATFORM_LIBS); \
		ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 ./$${t%.wyn}.asan >/dev/null 2>$${t%.wyn}.asan.log \
			|| { echo "  ASAN FAIL: $$t"; cat $${t%.wyn}.asan.log; exit 1; }; \
		echo "  ok    $$t"; \
		rm -f $${t%.wyn}.asan $${t%.wyn}.asan.log $$t.c; \
	done
	@echo "asan-runtime: all clean"

# TSan-instrumented runtime: same shape as the ASan lib, but for data races.
# Two executors exist (coroutine scheduler + legacy thread pool behind
# WYN_ASYNC_POOL=1) and races hide in whichever one a test doesn't exercise,
# so every test runs under BOTH configs. Used by the sanitizer CI job; run
# locally with `make tsan-runtime-test`.
runtime-tsan: runtime/libwyn_rt_tsan.a
runtime/libwyn_rt_tsan.a: $(RT_SRCS) $(wildcard src/*.h)
	@echo "Building TSan runtime library..."
	@mkdir -p runtime/obj_tsan
	@set -e; for f in $(RT_SRCS); do \
		$(CC) -std=c11 -O1 -g -w -fsanitize=thread -fno-omit-frame-pointer \
		-D_GNU_SOURCE -I src -I vendor/minicoro \
		-c $$f -o runtime/obj_tsan/$$(basename $$f .c).o; \
	done
	@ar rcs runtime/libwyn_rt_tsan.a runtime/obj_tsan/*.o
	@echo "Built runtime/libwyn_rt_tsan.a"

# Concurrency-focused test set: spawn/await/parallel/channels are where the
# two executors interleave threads. Each binary runs twice - default
# (coroutine) and WYN_ASYNC_POOL=1 (thread pool). Any TSan report fails.
TSAN_TESTS = tests/expect/test_channels.wyn \
             tests/expect/test_parallel.wyn \
             tests/expect/test_parallel_timeout.wyn \
             tests/expect/test_spawn_await.wyn \
             tests/expect/test_spawn_parallel.wyn \
             tests/expect/test_spawn_typed_args.wyn \
             tests/expect/test_concurrent_strings.wyn \
             tests/expect/test_await_twice.wyn \
             tests/expect/test_select_arms.wyn \
             tests/regression/test_channel_many_senders_race.wyn

tsan-runtime-test: wyn$(EXE_EXT) runtime/libwyn_rt_tsan.a
	@echo "=== TSan runtime test (both executor configs) ==="
	@set -e; for t in $(TSAN_TESTS); do \
		[ -f $$t ] || continue; \
		./wyn build $$t --debug >/dev/null 2>&1 || { echo "  skip (build) $$t"; continue; }; \
		$(CC) -std=c11 -O0 -g -w -fsanitize=thread -fno-omit-frame-pointer \
			-I src -o $${t%.wyn}.tsan $$t.c runtime/libwyn_rt_tsan.a $(PLATFORM_LIBS); \
		for pool in "" "WYN_ASYNC_POOL=1"; do \
			env $$pool TSAN_OPTIONS=halt_on_error=1:abort_on_error=1 \
				./$${t%.wyn}.tsan >/dev/null 2>$${t%.wyn}.tsan.log \
				|| { echo "  TSAN FAIL: $$t ($${pool:-default})"; cat $${t%.wyn}.tsan.log; exit 1; }; \
			echo "  ok    $$t ($${pool:-default})"; \
		done; \
		rm -f $${t%.wyn}.tsan $${t%.wyn}.tsan.log $$t.c; \
	done
	@echo "tsan-runtime: all clean"

# Precompiled header for the dev loop (macOS/clang). Flags MUST match the -O0
# dev compile in main.c exactly - clang refuses a pch whose flags differ.
ifeq ($(shell uname),Darwin)
runtime: runtime/wyn_runtime.pch
runtime/wyn_runtime.pch: src/wyn_runtime.h
	@$(CC) -x c-header -std=c11 -O0 -w -Wno-int-conversion -ffunction-sections -fdata-sections -I src \
		src/wyn_runtime.h -o runtime/wyn_runtime.pch 2>/dev/null && \
		echo "Built runtime/wyn_runtime.pch ($$(du -h runtime/wyn_runtime.pch | cut -f1))" || true
endif

# TCC runtime - excludes spawn.c, coroutine.c (can't compile macOS headers with TCC)
TCC_BIN = vendor/tcc/bin/tcc
TCC_RT_SRCS = src/wyn_arena.c src/wyn_rc.c src/io_loop.c src/runtime_exports.c src/wyn_wrapper.c src/wyn_interface.c src/optional.c src/result.c src/arc_runtime.c src/concurrency.c src/async_runtime.c src/safe_memory.c src/error.c src/string_runtime.c src/hashmap.c src/hashset.c src/json.c src/json_runtime.c src/stdlib_runtime.c src/hashmap_runtime.c src/stdlib_string.c src/stdlib_array.c src/stdlib_time.c src/stdlib_crypto.c src/stdlib_math.c src/net.c src/net_runtime.c src/test_runtime.c src/net_advanced.c src/file_io_simple.c src/stdlib_enhanced.c
runtime-tcc:
	@echo "Building TCC runtime library..."
	@mkdir -p /tmp/tcc_rt_obj
	@for f in $(TCC_RT_SRCS); do $(TCC_BIN) -c -I src -I vendor/minicoro -I vendor/tcc/tcc_include -w -DMCO_NO_MULTITHREAD -DMCO_USE_UCONTEXT -D_XOPEN_SOURCE=600 $$f -o /tmp/tcc_rt_obj/$$(basename $$f .c).o 2>/dev/null; done
	@ar rcs vendor/tcc/lib/libwyn_rt_tcc.a /tmp/tcc_rt_obj/*.o
	@rm -rf /tmp/tcc_rt_obj
	@echo "Built vendor/tcc/lib/libwyn_rt_tcc.a"

clean:
	rm -f wyn wyn.exe wyn-windows.exe wyn-linux wyn-macos tests/test_lexer tests/test_parser tests/test_checker tests/test_codegen tests/test_operators tests/test_default_parameters tests/test_function_overloading tests/test_generic_functions tests/test_parameter_validation tests/test_function_integration tests/test_syntax_design tests/test_system_integration tests/phase2_integration tests/phase2_integration_simple tests/test_wasm_support tests/test_self_compilation tests/test_documentation_system tests/test_container_support tests/test_lexer_rewrite tests/test_coroutine tools/formatter.wyn.out
	rm -rf temp runtime/obj runtime/libwyn_rt.a

.PHONY: all test test_bdd clean container-build container-test container-deploy container-all fmt-tool platform-info wyn-windows wyn-linux wyn-macos

# valgrind-test defined earlier in file (line ~125)

test_t2_3_1_validation: tests/test_t2_3_1_validation.c $(SOURCES)
	$(CC) $(CFLAGS) -I src -o tests/test_t2_3_1_validation tests/test_t2_3_1_validation.c $(SOURCES) $(LDFLAGS)

# Runtime library
runtime/libwyn_runtime.a:
	$(MAKE) -C runtime
