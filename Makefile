# Platform detection
UNAME_S := $(shell uname -s 2>/dev/null || echo "Windows")
UNAME_M := $(shell uname -m 2>/dev/null || echo "x86_64")

# Platform-specific settings
ifeq ($(OS),Windows_NT)
    PLATFORM := windows
    CC := gcc
    EXE_EXT := .exe
    # -lcrypt32 / -lbcrypt: needed by anything that links src/wyn_tls.c or the
    # vendored mbedTLS, and MEASURED, not guessed (mingw-w64 gcc 12 cross-link):
    #   crypt32  the Windows trust store is CryptoAPI's "ROOT", not a PEM file, so
    #            wyn_tls.c calls CertOpenSystemStoreA / CertEnumCertificatesInStore /
    #            CertCloseStore there and nowhere else;
    #   bcrypt   mbedTLS's own entropy_poll.c calls BCryptGenRandom on Windows - a
    #            link with crypt32 ALONE still failed on that symbol.
    # The same pair is added to the link line of every COMPILED PROGRAM by
    # wyn_tls_build_flags() in src/main.c; this assignment covers the compiler binary
    # and the sanitizer test links.
    PLATFORM_LIBS := -lws2_32 -lcrypt32 -lbcrypt -lpthread -lm
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

# App module (desktop GUI) - macOS links a prebuilt Objective-C object because
# `wyn build` shells out to clang for C, not ObjC, and cannot compile a .m itself.
# Nothing built this before, so `src/wyn_webview.o` existed only on machines where
# someone had run clang by hand: every release tarball shipped without it and
# every App.* program failed at the link step with
#   clang: error: no such file or directory: '.../src/wyn_webview.o'
# for 100% of installed users, while working fine from a source checkout.
# Windows compiles src/wyn_webview_win.c at link time and needs no object here.
# Must be defined BEFORE `all:` - make expands a rule's prerequisites when the
# rule is read, so a later assignment would silently expand to nothing.
ifeq ($(UNAME_S),Darwin)
WEBVIEW_OBJ := src/wyn_webview.o
endif

# Same rule as WEBVIEW_OBJ above, and it was already broken: MBEDTLS_LIB was
# defined ~20 lines BELOW `all:`, so `$(MBEDTLS_LIB)` in the prerequisite list
# expanded to NOTHING and a plain `make` never built the TLS library. `make test`
# did (its own rule sits after the assignment), which is exactly why nobody
# noticed. Now that the runtime links wyn_tls.c, a missing library is a link
# failure for every compiled program, so the definition moves up here.
MBEDTLS_DIR  = vendor/mbedtls
MBEDTLS_SRCS = $(wildcard $(MBEDTLS_DIR)/library/*.c)
MBEDTLS_LIB  = $(MBEDTLS_DIR)/lib/libmbedtls_wyn.a

all: wyn$(EXE_EXT) $(MBEDTLS_LIB) runtime $(WEBVIEW_OBJ)

src/wyn_webview.o: src/wyn_webview.m src/wyn_webview.h
	$(CC) -ObjC -fobjc-arc -O2 -I src -c $< -o $@

# --- Vendored mbedTLS (3.6 LTS) -----------------------------------------------
# In-process TLS, so HTTPS stops shelling out to `openssl s_client` (an RCE: the
# URL and POST body were spliced into a command string). One vendored library for
# every target rather than three system-TLS backends - see the release plan
# ROADMAP "OWNER DECISIONS 2026-09-22".
#
# Compiled with the UPSTREAM DEFAULT config (vendor/mbedtls/include/mbedtls/
# mbedtls_config.h, untouched): correctness first. Trimming the config is a
# separate concern - nothing links the unused objects anyway, because this is a
# static archive and the linker pulls members on demand.
#
# Built with `-w`: third-party code, and $(CFLAGS)'s -Wall -Wextra is our bar for
# our code, not theirs. Nothing here is in CFLAGS' -D_GNU_SOURCE world either -
# mbedTLS picks its own feature macros per platform.
# (MBEDTLS_DIR / MBEDTLS_SRCS / MBEDTLS_LIB are assigned above `all:` - make
# expands a rule's prerequisites when the rule is READ, so they cannot live here.)

mbedtls: $(MBEDTLS_LIB)

# The TLS seam on its own, for the edit loop. `make test` runs it too.
test-tls-seam: $(MBEDTLS_LIB)
	@bash tests/tls/run_tls_seam_test.sh

# JSON Schema derivation on its own, for the edit loop. `make test` runs it too.
# Needs no `wyn` binary: it links src/wyn_schema.c directly.
test-schema:
	@bash tests/schema/run_schema_test.sh
# The native HTTPS transport on its own, for the edit loop. `make test` runs it too.
test-https: $(MBEDTLS_LIB) runtime
	@bash tests/https/run_https_test.sh

$(MBEDTLS_LIB): $(MBEDTLS_SRCS) $(wildcard $(MBEDTLS_DIR)/library/*.h) $(wildcard $(MBEDTLS_DIR)/include/mbedtls/*.h)
	@echo "Building vendored mbedTLS ($$(sed -n 's/.*MBEDTLS_VERSION_STRING  *"\(.*\)".*/\1/p' $(MBEDTLS_DIR)/include/mbedtls/build_info.h))..."
	@mkdir -p $(MBEDTLS_DIR)/obj $(MBEDTLS_DIR)/lib
	@set -e; for f in $(MBEDTLS_SRCS); do \
		$(CC) -std=c11 -O2 -w -I $(MBEDTLS_DIR)/include -I $(MBEDTLS_DIR)/library \
		-c $$f -o $(MBEDTLS_DIR)/obj/$$(basename $$f .c).o; \
	done
	@# `ar r` REPLACES members, it does not remove stale ones, and this archive is
	@# regenerated whenever the vendored tree moves - same footgun documented on
	@# runtime/libwyn_rt.a below. Delete, then create.
	@rm -f $@
	@ar rcs $@ $(MBEDTLS_DIR)/obj/*.o
	@echo "Built $@ ($$(du -h $@ | cut -f1))"

# Platform information
platform-info:
	@echo "Platform: $(PLATFORM)"
	@echo "Architecture: $(UNAME_M)"
	@echo "Compiler: $(CC)"
	@echo "Executable extension: $(EXE_EXT)"
	@echo "Platform libs: $(PLATFORM_LIBS)"
	@echo "Platform flags: $(PLATFORM_CFLAGS)"

# C-based compiler
CORE_SRCS = src/main.c src/lexer.c src/parser.c src/checker.c src/codegen.c src/generics.c src/safe_memory.c src/error.c src/security.c src/memory.c src/string_runtime.c src/async_runtime.c src/concurrency.c src/optional.c src/result.c src/type_inference.c src/module_loader.c src/module.c src/module_registry.c src/io.c src/stdlib_array.c src/stdlib_string.c src/stdlib_time.c src/stdlib_crypto.c src/stdlib_math.c src/wyn_interface.c src/optimize.c src/traits.c src/platform.c src/cmd_compile.c src/cmd_test.c src/cmd_other.c src/cmd_ui.c src/hashmap.c src/hashset.c src/json.c src/types.c src/patterns.c  src/toml.c src/package.c src/pkgspec.c src/lsp.c src/bindgen.c src/cpkg.c src/tcc_backend.c src/wyn_arena.c src/wyn_rc.c src/coroutine.c src/wyn_schema.c
# src/wyn_schema.c is here before anything calls it (the `ai fn` parser and codegen
# land in later changes). It is listed anyway so every `make`, on all four CI
# platforms, compiles it under -Wall -Wextra: a module that only the test runner
# builds is a module whose portability nobody checks until the day it matters.
#
# NOTE: src/spawn.c is deliberately NOT linked into the compiler. The compiler
# only registers Task_send/Task_recv/etc. as builtin NAME strings (checker.c) -
# it never calls the spawn runtime in-process; compiled programs get it from
# runtime/libwyn_rt.a. Linking spawn.c here forced the whole thread-pool
# scheduler (spawn_fast.c: wyn_sched_pump_one/inflight) into the compiler too.

# Sources #included directly into another translation unit (codegen.c pulls in the
# codegen_* files, checker.c pulls in checker_builtins.c). They are NOT in CORE_SRCS -
# compiling them standalone would duplicate symbols - but they must be prerequisites,
# or make sees no changed prerequisite and silently keeps a stale binary.
#
# THIS LIST IS DERIVED, NOT REMEMBERED. tests/errors/run_tu_include_list_test.sh greps
# src/ for `#include "<name>.c"` and fails on a set difference in either direction. It
# was forgotten once: src/codegen_gpu.c (codegen.c:2820) was absent for its whole life,
# so every edit to the GPU codegen left `make` reporting success over the OLD binary.
# The `$(wildcard src/*.h)` prerequisite on the rule below does NOT cover these - they
# are .c files. The gate is only as good as what its regex can SEE, so it self-tests
# that regex against every include spelling (trailing `//` and `/* */` comments
# included - an early version was anchored at end-of-line and blind to them).
TU_INCLUDED_SRCS = src/codegen_expr.c src/codegen_stmt.c src/codegen_lambda.c src/codegen_program.c \
                   src/codegen_gpu.c src/checker_builtins.c

# libtcc.a AND vendor/tcc/include ARE NOT LINK INPUTS, because nothing uses them.
# `grep -rcoE '\btcc_[a-z_]+\(' src/` is 0 and libtcc.h is included nowhere:
# src/tcc_backend.c runs `vendor/tcc/bin/tcc` as a child process via system(). The
# archive was on this line regardless, and since no member was referenced the linker
# discarded all of them - `nm wyn | grep -i tcc` showed only Wyn's own two symbols.
#
# That dead input was not harmless. It is where LICENSE's claim that the binary
# "statically links TinyCC (libtcc)" came from, and that claim (a) is false, (b) made
# GitHub unable to classify LICENSE at all, so the project displayed no licence, and
# (c) produced a page of LGPL-2.1 §6 relinking analysis for a link that does not
# exist. Removing the input makes the artefact unambiguous instead of documenting
# around it.
wyn$(EXE_EXT): $(CORE_SRCS) $(TU_INCLUDED_SRCS) $(wildcard src/*.h)
	$(CC) $(CFLAGS) -I src -I vendor/minicoro -o $@ $(CORE_SRCS) $(PLATFORM_LIBS)

# Platform-specific targets
wyn-windows: PLATFORM_CFLAGS += -DWYN_PLATFORM_WINDOWS
wyn-windows: PLATFORM_LIBS = -lws2_32 -lpthread -lm
wyn-windows: CC = x86_64-w64-mingw32-gcc
wyn-windows: EXE_EXT = .exe
wyn-windows: src/main.c src/lexer.c src/parser.c src/checker.c src/codegen.c src/generics.c src/safe_memory.c src/error.c src/security.c src/memory.c src/string_runtime.c src/optional.c src/result.c src/type_inference.c src/module_loader.c src/io.c src/wyn_interface.c src/optimize.c src/traits.c src/platform.c
	$(CC) $(CFLAGS) -I src -o wyn$(EXE_EXT) $^ $(PLATFORM_LIBS)

wyn-linux: PLATFORM_CFLAGS += -DWYN_PLATFORM_LINUX
wyn-linux: PLATFORM_LIBS = -lpthread -lm
wyn-linux: CC = gcc
wyn-linux: EXE_EXT =
wyn-linux: src/main.c src/lexer.c src/parser.c src/checker.c src/codegen.c src/generics.c src/safe_memory.c src/error.c src/security.c src/memory.c src/string_runtime.c src/optional.c src/result.c src/type_inference.c src/module_loader.c src/io.c src/wyn_interface.c src/optimize.c src/traits.c src/platform.c
	$(CC) $(CFLAGS) -I src -o wyn$(EXE_EXT) $^ $(PLATFORM_LIBS)

wyn-macos: PLATFORM_CFLAGS += -DWYN_PLATFORM_MACOS
wyn-macos: PLATFORM_LIBS = -lpthread -lm
wyn-macos: CC = clang
wyn-macos: EXE_EXT =
wyn-macos: src/main.c src/lexer.c src/parser.c src/checker.c src/codegen.c src/generics.c src/safe_memory.c src/error.c src/security.c src/memory.c src/string_runtime.c src/optional.c src/result.c src/type_inference.c src/module_loader.c src/io.c src/wyn_interface.c src/optimize.c src/traits.c src/platform.c
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

# NOTE: `test_string_memory` and `test_string_leaks` used to live here. Their
# prerequisites tests/memory/test_string_memory.c and .../test_string_leaks.c were
# deleted from the tree long ago (they are still in git history), so both targets
# could only fail with "No rule to make target". Neither was in the `test:` roster
# and neither is named anywhere outside this file, so nothing lost a gate. They were
# removed rather than re-pointed when src/arc_runtime.c left.

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
# WHY: `make test` runs 147 `bash tests/...` gates - counted from the recipe below,
# 146 of them before the scripts/ syntax gate was added to it - and takes ~38
# MINUTES, not the "~9 minutes" this comment claimed until 2026-10-05. Measured:
# median of the 11 green scripts/suite.sh runs (29,33,35,37,37,38,38,39,40,41,44 min),
# recovered from each log's birth->mtime delta because the harness stamped nothing -
# which is why scripts/suite.sh now timestamps every line and prints a total elapsed.
# run_bdd.sh alone is 173-233s (timed twice, serially, in the per-gate timing pass).
# That cost is per-iteration during debugging, so a 10-round session spends most of a
# day waiting. check-fast itself was 12-14s here (timed twice). It runs the two things
# actually catch codegen mistakes fast: the build (0 warnings) and the golden-C
# snapshots, which pin the generated C and are exactly what the soundness work
# perturbs.
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

# runtime/libwyn_rt.a is a PREREQUISITE and not an assumption: arm 3 of
# tests/errors/run_slim_header_parity_test.sh reads the archive with nm, and that
# gate FAILS rather than skips when the archive is absent. Depending on it here is
# what keeps "make test on a fresh clone" from reporting a vacuous arm.
test: wyn $(MBEDTLS_LIB) runtime/libwyn_rt.a
	@# FIRST, because it costs under a second and because scripts/ is the one
	@# directory in this repo that no gate watched. The measurement harness
	@# (scripts/suite.sh, scripts/sweep_*.py) rotted while it lived outside any
	@# repo; being tracked does not stop that, being GATED does. This roster is an
	@# explicit list and not a glob, so a new gate is invisible until this line
	@# exists - which is the whole reason it is here rather than assumed.
	@echo "=== Running scripts/ syntax gate (bash -n + py_compile, with floors) ==="
	@bash tests/lint_scripts.sh
	@echo "=== Running assertion tests (run_bdd.sh) ==="
# WYN_TEST_FILTER= is NOT redundant. The filter run_bdd.sh gained is an edit-loop
# tool, and a gate must not be thinnable by its caller's environment: an exported
# WYN_TEST_FILTER would otherwise narrow this step to a handful of programs and
# still report success - "a gate passes because its matching rule got smaller" is
# the failure this suite exists to catch. Clearing it here makes the gate's own
# invocation define the gate. Same clearing, same reason, at ci.yml's
# WYN_ASYNC_CORO run_bdd step.
	@WYN=./wyn WYN_TEST_FILTER= bash tests/run_bdd.sh
	@echo "=== Running TLS seam test ==="
	@bash tests/tls/run_tls_seam_test.sh
	@echo "=== Running JSON Schema derivation test ==="
	@bash tests/schema/run_schema_test.sh
	@echo "=== Running native HTTPS transport test (+ no-openssl tripwire) ==="
	@bash tests/https/run_https_test.sh
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
	@echo "=== Running print() atomicity test ==="
	@WYN=./wyn bash tests/errors/run_print_atomicity_test.sh
	@echo "=== Running cc-error isolation test (parallel wyn run) ==="
	@WYN=./wyn bash tests/errors/run_cc_err_isolation_test.sh
	@echo "=== Running stale-pch recovery test ==="
	@WYN=./wyn bash tests/errors/run_stale_pch_test.sh
	@echo "=== Running TU-included-sources list test (derived from the #include sites) ==="
	@bash tests/errors/run_tu_include_list_test.sh
	@echo "=== Running runtime source-list existence test (16 hand-maintained lists) ==="
	@bash tests/errors/run_runtime_source_lists_test.sh
	@echo "=== Running unresolved-import abort test ==="
	@WYN=./wyn bash tests/errors/run_unresolved_import_test.sh
	@echo "=== Running selective-import alias rejection test ==="
	@WYN=./wyn bash tests/errors/run_selective_import_alias_test.sh
	@echo "=== Running mut-param non-lvalue rejection test ==="
	@WYN=./wyn bash tests/errors/run_mut_param_nonlvalue_test.sh
	@echo "=== Running namespace-typo message test ==="
	@WYN=./wyn bash tests/errors/run_namespace_typo_message_test.sh
	@echo "=== Running namespace unknown-method check-time rejection test ==="
	@WYN=./wyn bash tests/errors/run_namespace_unknown_method_test.sh
	@echo "=== Running spawn-on-a-closure rejection test ==="
	@WYN=./wyn bash tests/errors/run_spawn_closure_test.sh
	@echo "=== Running for-in-string check-time rejection test ==="
	@WYN=./wyn bash tests/errors/run_for_in_string_test.sh
	@echo "=== Running run-cache import-staleness test ==="
	@WYN=./wyn bash tests/errors/run_run_cache_imports_test.sh
	@echo "=== Running --release link/parity test ==="
	@WYN=./wyn bash tests/errors/run_release_link_test.sh
	@echo "=== Running --release slim-header registry coverage gate ==="
	@WYN=./wyn bash tests/errors/run_release_slim_registry_test.sh
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
	@echo "=== Running returned-aggregate string-lifetime test (V-1) ==="
	@WYN=./wyn bash tests/errors/run_returned_aggregate_string_test.sh
	@echo "=== Running generic-enum negative test ==="
	@WYN=./wyn bash tests/errors/run_generic_enum_test.sh
	@echo "=== Running unknown-method negative test ==="
	@WYN=./wyn bash tests/errors/run_unknown_method_test.sh
	@echo "=== Running unknown-collection-method test (map/set fail the build, #426) ==="
	@WYN=./wyn bash tests/errors/run_unknown_collection_method_test.sh
# The .wyn regression tests for #465 cover `wyn build`, because run_bdd.sh only ever
# invokes `$WYN build`. This covers the `wyn run` enforcement site, which had none.
	@echo "=== Running codegen-fails-the-build test (wyn run path, #465) ==="
	@WYN=./wyn bash tests/errors/run_codegen_fails_the_build_test.sh
	@echo "=== Running Result-as-a-parameter test (#424) ==="
	@WYN=./wyn bash tests/errors/run_result_param_test.sh
	@echo "=== Running argument-type test (wrong-typed argument is rejected, #425) ==="
	@WYN=./wyn bash tests/errors/run_arg_type_test.sh
	@echo "=== Running Option/Result-predicate-on-a-scalar test (V-28) ==="
	@WYN=./wyn bash tests/errors/run_scalar_option_method_test.sh
	@echo "=== Running typed-HashSet test, debug half + the wyn-check arms (V-38, #391) ==="
	@WYN=./wyn bash tests/errors/run_typed_set_debug_test.sh
	@echo "=== Running typed-HashSet test, --release half (V-38, #391) ==="
	@WYN=./wyn bash tests/errors/run_typed_set_release_test.sh
	@echo "=== Running map value-type test (HashMap.set/get namespace spelling, #429) ==="
	@WYN=./wyn bash tests/errors/run_map_value_type_test.sh
	@echo "=== Running void-call-type test (a void call is not an int) ==="
	@WYN=./wyn bash tests/errors/run_void_call_type_test.sh
	@echo "=== Running container-fresh-type test (two HashMap.new() are independent) ==="
	@WYN=./wyn bash tests/errors/run_container_fresh_type_test.sh
	@WYN=./wyn bash tests/errors/run_registry_reachable_test.sh
	@bash tests/errors/run_slim_header_parity_test.sh
	@echo "=== Running debug/--release output parity test ==="
	@WYN=./wyn bash tests/errors/run_release_output_parity_test.sh
	@echo "=== Running print(set) rendering test (#427) ==="
	@WYN=./wyn bash tests/errors/run_print_set_test.sh
	@echo "=== Running tuple-array rejection test (#415) ==="
	@WYN=./wyn bash tests/errors/run_tuple_array_test.sh
	@WYN=./wyn bash tests/errors/run_collection_return_type_test.sh
	@WYN=./wyn bash tests/errors/run_lambda_return_type_test.sh
	@WYN=./wyn bash tests/errors/run_value_call_diagnostics_test.sh
	@WYN=./wyn bash tests/errors/run_regex_contract_test.sh
	@WYN=./wyn bash tests/errors/run_json_handle_test.sh
	@WYN=./wyn bash tests/errors/run_option_combinator_test.sh
	@echo "=== Running Option/Result combinator API gate, debug half + the wyn-check arms (#392) ==="
	@WYN=./wyn bash tests/errors/run_option_combinator_api_debug_test.sh
	@echo "=== Running Option/Result combinator API gate, --release half + the slim header (#392) ==="
	@WYN=./wyn bash tests/errors/run_option_combinator_api_release_test.sh
	@echo "=== Running bug-batch-2 test ==="
	@WYN=./wyn bash tests/errors/run_bug_batch2_test.sh
	@echo "=== Running user test-runner test ==="
	@WYN=./wyn bash tests/errors/run_user_test_runner_test.sh
	@echo "=== Running wyn test one-summary gate ==="
	@WYN=./wyn bash tests/errors/run_test_summary_test.sh
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
	@echo "=== Running interpolation error-position gate (V-14) ==="
	@WYN=./wyn bash tests/errors/run_interp_error_line_test.sh
	@echo "=== Running UTF-8 padding test ==="
	@WYN=./wyn bash tests/errors/run_pad_utf8_test.sh
	@echo "=== Running Json one-model test ==="
	@WYN=./wyn bash tests/errors/run_json_model_test.sh
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
	@echo "=== Running bool-in-print authority test (V-30, every spelling) ==="
	@WYN=./wyn bash tests/errors/run_bool_in_print_test.sh
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
	@echo "=== Running map-literal value-type test (V-2) ==="
	@WYN=./wyn bash tests/errors/run_map_literal_value_type_test.sh
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
	@echo "=== Running sort_by comparator gate (V-19) ==="
	@WYN=./wyn bash tests/errors/run_sort_by_cmp_test.sh
	@echo "=== Running silent-wrong-answer test ==="
	@WYN=./wyn bash tests/errors/run_silent_wrong_test.sh
	@echo "=== Running regex shorthand-class gate (\\d \\w \\s) ==="
	@WYN=./wyn bash tests/errors/run_regex_escape_test.sh
	@echo "=== Running diagnostic-location + panic-path test ==="
	@WYN=./wyn bash tests/errors/run_diagnostic_location_test.sh
	@echo "=== Running checker-soundness gate (K5-K11) test ==="
	@WYN=./wyn bash tests/errors/run_checker_soundness_test.sh
	@echo "=== Running await_all element-typing gate ==="
	@WYN=./wyn bash tests/errors/run_await_all_type_test.sh
	@echo "=== Running Option/Result family completeness gate ==="
	@WYN=./wyn bash tests/errors/run_option_result_family_test.sh
	@echo "=== Running parallel{} synthesized-Expr initialisation gate ==="
	@WYN=./wyn bash tests/errors/run_parallel_synth_expr_init_test.sh
	@echo "=== Running parallel{} branch-overlap gate ==="
	@WYN=./wyn bash tests/errors/run_parallel_overlap_test.sh
	@echo "=== Running spawn-future array typing gate ==="
	@WYN=./wyn bash tests/errors/run_future_array_typing_test.sh
	@echo "=== Running crucible-P0 (fatal-by-default) test ==="
	@WYN=./wyn bash tests/errors/run_crucible_p0_test.sh
	@echo "=== Running checked string->number parse gate (V-18) ==="
	@WYN=./wyn bash tests/errors/run_parse_checked_test.sh
	@echo "=== Running CLI DX test ==="
	@WYN=./wyn bash tests/errors/run_cli_dx_test.sh
	@echo "=== Running wyn-run orphan-child test ==="
	@WYN=./wyn bash tests/errors/run_orphan_child_test.sh
	@echo "=== Running sqlite link-order gate ==="
	@WYN=./wyn bash tests/errors/run_sqlite_link_order_test.sh
	@echo "=== Running wyn ui coverage test ==="
	@WYN=./wyn bash tests/errors/run_ui_coverage_test.sh
	@echo "=== Running CLI flag-honesty gate (an unknown flag is an error) ==="
	@WYN=./wyn bash tests/errors/run_cli_flag_honesty_test.sh
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
	@echo "=== Running .len() O(1) length-cache gate ==="
	@WYN=./wyn bash tests/errors/run_len_cache_test.sh
	@echo "=== Running Task.select diagnostic gate ==="
	@WYN=./wyn bash tests/errors/run_task_select_diagnostic_test.sh
	@echo "=== Running stdlib error-channel gate ==="
	@WYN=./wyn bash tests/errors/run_error_channel_test.sh
	@echo "=== Running JSON nesting-depth gate ==="
	@WYN=./wyn bash tests/errors/run_json_depth_test.sh
	@echo "=== Running README code-block gate (builds and runs every wyn block) ==="
	@WYN=./wyn bash scripts/check_readme.sh
	@echo "=== Running packed-array leak gate (RSS bound) ==="
	@WYN=./wyn bash tests/errors/run_array_leak_test.sh
	@echo "=== Running --fast no-op gate ==="
	@WYN=./wyn bash tests/errors/run_fast_flag_test.sh
	@echo "=== Running HTTP server concurrent-load gate ==="
	@WYN=./wyn bash tests/errors/run_http_server_load_test.sh
	@echo "=== Running HTTP two-request + example gate ==="
	@WYN=./wyn bash tests/errors/run_http_two_requests_test.sh
	@echo "=== Running HTTP response-descriptor injection gate ==="
	@WYN=./wyn bash tests/errors/run_http_fd_injection_test.sh
	@echo "=== Running HTTP client status/error reachability gate ==="
	@WYN=./wyn bash tests/errors/run_http_client_status_test.sh
	@echo "=== Running test-port hygiene gate (no test may bind a fixed port) ==="
	@WYN=./wyn bash tests/errors/run_test_port_hygiene_test.sh
	@echo "=== Running v1.21 ACCEPTANCE gate ==="
	@# The release's own exit criterion: ONE realistic CLI tool that reads stdin,
	@# parses JSON, formats numbers, propagates errors across DIFFERENT Result
	@# families, passes structs across boundaries and uses a HashMap from a
	@# spawned handler - all at once. Every other suite here was green while the
	@# three defects this tool found were live, which is exactly §10's argument.
	@WYN=./wyn bash tests/acceptance/run_acceptance_test.sh
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

# NOTE: eight ARC-epic test targets used to live here - test_arc_runtime,
# test_arc_operations, test_weak_references, test_cycle_detection, test_memory_pool,
# test_escape_analysis, test_arc_insertion, test_weak_codegen - plus two empty
# section headers. Every one of them named a prerequisite that is not in the tree
# (tests/test_arc_*.c, src/arc_operations.c, src/weak_references.c,
# src/cycle_detection.c; all deleted long ago, still in git history), so they could
# only fail with "No rule to make target". None was in the `test:` roster and none is
# named anywhere outside this file. They were removed rather than re-pointed when
# src/arc_runtime.c left.

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
#
# src/wyn_tls.c + src/wyn_https.c are the native HTTPS transport. Until 2026-09
# wyn_tls.c was compiled by NOTHING - it existed, had a passing gate of its own, and
# was in no library, so nothing could call it. Both live here now, which is what makes
# `Http.get("https://...")` reach mbedTLS instead of `popen("... openssl s_client")`.
# Consequence to know: every link line that pulls a member referencing wyn_tls_*
# also needs $(MBEDTLS_LIB) - see wyn_tls_build_flags() in src/main.c, which is the
# one place that decides.
RT_SRCS = src/wyn_arena.c src/wyn_rc.c src/runtime_exports.c src/wyn_wrapper.c \
          src/wyn_tls.c src/wyn_https.c \
          src/wyn_interface.c src/coroutine.c src/spawn_fast.c src/spawn.c src/future.c \
          src/io.c src/io_loop.c src/optional.c src/result.c \
          src/concurrency.c src/async_runtime.c \
          src/safe_memory.c src/error.c src/string_runtime.c \
          src/hashmap.c src/hashset.c src/json.c \
          src/stdlib_runtime.c src/hashmap_runtime.c \
          src/stdlib_string.c src/stdlib_array.c src/stdlib_time.c \
          src/stdlib_crypto.c src/stdlib_math.c \
          src/net_advanced.c \
          src/test_runtime.c src/file_io_simple.c src/stdlib_enhanced.c

# The archive members, DERIVED from RT_SRCS rather than from a glob over the
# persistent runtime/obj* directories. See the note on the libwyn_rt.a rule.
RT_OBJS       = $(patsubst %.c,%.o,$(addprefix runtime/obj/,$(notdir $(RT_SRCS))))
RT_OBJS_ASAN  = $(patsubst %.c,%.o,$(addprefix runtime/obj_asan/,$(notdir $(RT_SRCS))))
RT_OBJS_TSAN  = $(patsubst %.c,%.o,$(addprefix runtime/obj_tsan/,$(notdir $(RT_SRCS))))

# The runtime library must be rebuilt whenever any runtime source (or a header
# they include, notably wyn_runtime.h/io_loop.h) changes - otherwise compiled
# programs silently link a stale libwyn_rt.a. Depend on the sources + headers so
# `make` detects the change instead of reporting "Nothing to be done".
runtime: runtime/libwyn_rt.a
runtime/libwyn_rt.a: $(RT_SRCS) $(wildcard src/*.h) | wyn$(EXE_EXT) $(MBEDTLS_LIB)
	@echo "Building runtime library..."
	@mkdir -p runtime/obj
	@# -DWYN_HAVE_TLS is what makes runtime_exports.c's https_get use the native
	@# transport rather than the "no TLS in this build" stub. It matters for
	@# `wyn run --release`, which emits wyn_runtime_slim.h (declarations only) and
	@# therefore takes https_get from THIS archive, not from the program's own TU.
	@set -e; for f in $(RT_SRCS); do \
		$(CC) -std=c11 -O2 -w -D_GNU_SOURCE -DWYN_HAVE_TLS \
		-I src -I vendor/minicoro -I $(MBEDTLS_DIR)/include \
		-c $$f -o runtime/obj/$$(basename $$f .c).o; \
	done
	@# `ar r` REPLACES members in an existing archive. If a stale runtime/obj/
	@# holds an .o from a previous build that this loop did not just recompile
	@# (or the archive holds a member whose source is gone), that stale code
	@# silently survives into the lib and every compiled program links it.
	@# This has already cost real debugging time: a runtime fix appeared to have
	@# no effect, and a perf regression looked unreproducible, because the lib
	@# still contained pre-fix objects. Build the archive from scratch instead -
	@# and name the members, DERIVED from RT_SRCS, rather than globbing
	@# runtime/obj/*.o. Deleting the archive alone did not fix the case the comment
	@# above describes: runtime/obj/ persists across builds, so an .o whose source
	@# is GONE was still swept into the fresh archive by the glob and kept shipping
	@# in every compiled program. RT_SRCS is the single source of truth for what
	@# belongs in this lib; a deleted source now cannot survive in it.
	@rm -f runtime/libwyn_rt.a
	@ar rcs runtime/libwyn_rt.a $(RT_OBJS)
	@echo "Built runtime/libwyn_rt.a ($$(du -h runtime/libwyn_rt.a | cut -f1))"

# ASan-instrumented runtime: compile RT_SRCS with -fsanitize=address into a
# separate lib, then build+run a set of representative tests against it. The
# RC/string/IO bugs live in the RUNTIME - `make debug-memory` only instruments
# the compiler, so this is the check that has caught every real UAF. Used by
# the sanitizer CI job; run locally with `make asan-runtime-test`.
runtime-asan: runtime/libwyn_rt_asan.a
runtime/libwyn_rt_asan.a: $(RT_SRCS) $(wildcard src/*.h) | $(MBEDTLS_LIB)
	@echo "Building ASan runtime library..."
	@mkdir -p runtime/obj_asan
	@# -I $(MBEDTLS_DIR)/include is REQUIRED, not optional: RT_SRCS now contains
	@# src/wyn_tls.c, which includes <mbedtls/ssl.h>. Same for the TSan lib below.
	@set -e; for f in $(RT_SRCS); do \
		$(CC) -std=c11 -O1 -g -w -fsanitize=address -fno-omit-frame-pointer \
		-D_GNU_SOURCE -DWYN_HAVE_TLS -I src -I vendor/minicoro -I $(MBEDTLS_DIR)/include \
		-c $$f -o runtime/obj_asan/$$(basename $$f .c).o; \
	done
	@rm -f runtime/libwyn_rt_asan.a
	@ar rcs runtime/libwyn_rt_asan.a $(RT_OBJS_ASAN)
	@echo "Built runtime/libwyn_rt_asan.a"

# Compile a representative test set's generated C against the ASan runtime
# and run each binary. Any ASan report (UAF, overflow, leak-at-exit is NOT
# checked - detect_leaks=0 keeps signal high) fails the target.
#
# The list is expect/ and regression/ plus ONE stdlib file. The stdlib suite was
# not covered here, and that is where the coverage mattered: uninstrumented,
# test_stdlib_expansion.wyn passes 275 runs in a row in both build modes and
# under MallocScribble/MallocGuardEdges; against the ASan runtime it reported a
# heap-buffer-overflow READ on the first run, every run. Runtime string
# constructors that returned a raw malloc'd buffer made the RC header probe read
# off the front of the block. Keep this file in the list - it exercises JSON,
# base64, crypto, uuid, datetime, regex, net and db string returns in one go.
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
             tests/regression/test_rc_stage2_reconcile.wyn \
             tests/regression/test_json_escaping.wyn \
             tests/regression/test_json_multiple_docs.wyn \
             tests/regression/test_json_parse_malformed.wyn \
             tests/stdlib/test_stdlib_expansion.wyn

asan-runtime-test: wyn$(EXE_EXT) runtime/libwyn_rt_asan.a $(MBEDTLS_LIB)
	@echo "=== ASan runtime test (representative set) ==="
	@set -e; for t in $(ASAN_TESTS); do \
		[ -f $$t ] || continue; \
		./wyn build $$t --debug >/dev/null 2>&1 || { echo "  skip (build) $$t"; continue; }; \
		$(CC) -std=c11 -O0 -g -w -fsanitize=address -fno-omit-frame-pointer \
			-I src -o $${t%.wyn}.asan $$t.c runtime/libwyn_rt_asan.a $(MBEDTLS_LIB) $(PLATFORM_LIBS); \
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
runtime/libwyn_rt_tsan.a: $(RT_SRCS) $(wildcard src/*.h) | $(MBEDTLS_LIB)
	@echo "Building TSan runtime library..."
	@mkdir -p runtime/obj_tsan
	@set -e; for f in $(RT_SRCS); do \
		$(CC) -std=c11 -O1 -g -w -fsanitize=thread -fno-omit-frame-pointer \
		-D_GNU_SOURCE -DWYN_HAVE_TLS -I src -I vendor/minicoro -I $(MBEDTLS_DIR)/include \
		-c $$f -o runtime/obj_tsan/$$(basename $$f .c).o; \
	done
	@rm -f runtime/libwyn_rt_tsan.a
	@ar rcs runtime/libwyn_rt_tsan.a $(RT_OBJS_TSAN)
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

tsan-runtime-test: wyn$(EXE_EXT) runtime/libwyn_rt_tsan.a $(MBEDTLS_LIB)
	@echo "=== TSan runtime test (both executor configs) ==="
	@set -e; for t in $(TSAN_TESTS); do \
		[ -f $$t ] || continue; \
		./wyn build $$t --debug >/dev/null 2>&1 || { echo "  skip (build) $$t"; continue; }; \
		$(CC) -std=c11 -O0 -g -w -fsanitize=thread -fno-omit-frame-pointer \
			-I src -o $${t%.wyn}.tsan $$t.c runtime/libwyn_rt_tsan.a $(MBEDTLS_LIB) $(PLATFORM_LIBS); \
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
# Depends on the MAKEFILE as well as the header: the pch must be rebuilt when the
# FLAGS change, not only when the header does. clang hard-errors if the pch's
# flags differ from the including file's ("signed integer overflow handling
# differs in precompiled file"), so when -fwrapv was added to both the compile
# line and this rule, every tree with an EXISTING pch kept using the old one and
# every build broke until the pch was deleted by hand. Anyone pulling that change
# would have hit it. Listing the Makefile makes the rebuild automatic.
runtime/wyn_runtime.pch: src/wyn_runtime.h Makefile
	@# -fwrapv must match the flags `wyn build` uses for the program that
	@# INCLUDES this pch. clang hard-errors on a mismatch ("signed integer
	@# overflow handling differs in precompiled file"), so adding -fwrapv to the
	@# build without adding it here breaks every macOS dev-loop build.
	@# -DWYN_HAVE_TLS is part of that flag set now: wyn_runtime.h's https_* bodies
	@# are behind it, so a pch built without it and included by a compile WITH it is
	@# a clang hard error ("definition of macro ... differs"). main.c only injects the
	@# pch on the path that also passes the define; see wyn_tls_build_flags.
	@$(CC) -x c-header -std=c11 -O0 -fwrapv -w -Wno-int-conversion -ffunction-sections -fdata-sections -DWYN_HAVE_TLS -I src \
		src/wyn_runtime.h -o runtime/wyn_runtime.pch 2>/dev/null && \
		echo "Built runtime/wyn_runtime.pch ($$(du -h runtime/wyn_runtime.pch | cut -f1))" || true
endif

# TCC runtime - excludes spawn.c, coroutine.c (can't compile macOS headers with TCC)
TCC_BIN = vendor/tcc/bin/tcc
TCC_RT_SRCS = src/wyn_arena.c src/wyn_rc.c src/io_loop.c src/runtime_exports.c src/wyn_wrapper.c src/wyn_interface.c src/optional.c src/result.c src/concurrency.c src/async_runtime.c src/safe_memory.c src/error.c src/string_runtime.c src/hashmap.c src/hashset.c src/json.c src/json_runtime.c src/stdlib_runtime.c src/hashmap_runtime.c src/stdlib_string.c src/stdlib_array.c src/stdlib_time.c src/stdlib_crypto.c src/stdlib_math.c src/test_runtime.c src/net_advanced.c src/file_io_simple.c src/stdlib_enhanced.c
runtime-tcc:
	@echo "Building TCC runtime library..."
	@mkdir -p /tmp/tcc_rt_obj
	@for f in $(TCC_RT_SRCS); do $(TCC_BIN) -c -I src -I vendor/minicoro -I vendor/tcc/tcc_include -w -DMCO_NO_MULTITHREAD -DMCO_USE_UCONTEXT -D_XOPEN_SOURCE=600 $$f -o /tmp/tcc_rt_obj/$$(basename $$f .c).o 2>/dev/null; done
	@ar rcs vendor/tcc/lib/libwyn_rt_tcc.a /tmp/tcc_rt_obj/*.o
	@rm -rf /tmp/tcc_rt_obj
	@echo "Built vendor/tcc/lib/libwyn_rt_tcc.a"

clean:
	rm -f wyn wyn.exe wyn-windows.exe wyn-linux wyn-macos tests/test_lexer tests/test_parser tests/test_checker tests/test_codegen tests/test_operators tests/test_default_parameters tests/test_function_overloading tests/test_generic_functions tests/test_parameter_validation tests/test_function_integration tests/test_syntax_design tests/test_system_integration tests/phase2_integration tests/phase2_integration_simple tests/test_wasm_support tests/test_self_compilation tests/test_documentation_system tests/test_container_support tests/test_lexer_rewrite tests/test_coroutine tools/formatter.wyn.out
	rm -rf temp runtime/obj runtime/libwyn_rt.a $(MBEDTLS_DIR)/obj $(MBEDTLS_DIR)/lib

.PHONY: all test test_bdd test-tls-seam test-https clean container-build container-test container-deploy container-all fmt-tool platform-info wyn-windows wyn-linux wyn-macos mbedtls

# valgrind-test defined earlier in file (line ~125)

test_t2_3_1_validation: tests/test_t2_3_1_validation.c $(SOURCES)
	$(CC) $(CFLAGS) -I src -o tests/test_t2_3_1_validation tests/test_t2_3_1_validation.c $(SOURCES) $(LDFLAGS)

# Runtime library
runtime/libwyn_runtime.a:
	$(MAKE) -C runtime
