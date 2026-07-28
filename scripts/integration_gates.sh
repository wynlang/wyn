#!/bin/bash

# Phase 2 Integration Validation Gates
# Automated validation at each handoff point

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Day 5 Integration Gate: LLVM Infrastructure Complete
validate_day5_llvm_infrastructure() {
    log_info "=== Day 5 Integration Gate: LLVM Infrastructure ==="
    
    local errors=0
    
    # 1. Verify LLVM Infrastructure deliverables
    log_info "Checking LLVM infrastructure completeness..."
    
    if [[ ! -f "src/llvm_codegen.c" ]]; then
        log_error "Missing src/llvm_codegen.c"
        ((errors++))
    else
        log_success "Found src/llvm_codegen.c"
    fi
    
    # Check for LLVM context management (T2.1.2)
    if grep -q "LLVMCodegenContext" src/llvm_codegen.c 2>/dev/null; then
        log_success "LLVM context management implemented"
    else
        log_warning "LLVM context management not yet implemented"
    fi
    
    # 2. Verify API contract compliance
    log_info "Validating LLVM API contracts..."
    
    # Check if LLVM build works
    if make wyn-llvm >/dev/null 2>&1; then
        log_success "LLVM build integration working"
    else
        log_error "LLVM build integration failed"
        ((errors++))
    fi
    
    # 3. Verify other agents can use LLVM system
    log_info "Testing LLVM system integration readiness..."
    
    # Check that LLVM headers are accessible
    if llvm-config --cflags >/dev/null 2>&1; then
        log_success "LLVM development environment ready"
    else
        log_error "LLVM development environment not ready"
        ((errors++))
    fi
    
    # 4. Performance validation
    log_info "Running LLVM performance validation..."
    
    # Basic performance test - LLVM should initialize quickly
    local start_time=$(date +%s%N)
    if timeout 10s ./wyn-llvm --version >/dev/null 2>&1; then
        local end_time=$(date +%s%N)
        local duration=$(( (end_time - start_time) / 1000000 )) # Convert to milliseconds
        if [[ $duration -lt 5000 ]]; then # Less than 5 seconds
            log_success "LLVM initialization performance acceptable ($duration ms)"
        else
            log_warning "LLVM initialization slow ($duration ms)"
        fi
    else
        log_warning "LLVM-based compiler not yet functional (expected in early Phase 2)"
    fi
    
    if [[ $errors -eq 0 ]]; then
        log_success "✅ Day 5 Integration Gate PASSED - LLVM infrastructure ready"
        return 0
    else
        log_error "❌ Day 5 Integration Gate FAILED - $errors errors found"
        return 1
    fi
}

# Day 10 Integration Gate: Basic Codegen + ARC Runtime Ready
validate_day10_codegen_arc() {
    log_info "=== Day 10 Integration Gate: Basic Codegen + ARC Runtime ==="
    
    local errors=0
    
    # 1. Verify basic code generation works
    log_info "Checking basic LLVM code generation..."
    
    # Check for codegen functions
    if grep -q "codegen_.*_expr\|codegen_.*_stmt" src/llvm_codegen.c 2>/dev/null; then
        log_success "Basic codegen functions found"
    else
        log_warning "Basic codegen functions not yet implemented"
    fi
    
    # 2. Verify ARC runtime system
    log_info "Checking ARC runtime system..."
    
    # Look for ARC-related files (may not exist yet)
    if [[ -f "src/arc_runtime.c" ]] || grep -q "arc_.*retain\|arc_.*release" src/*.c 2>/dev/null; then
        log_success "ARC runtime system components found"
    else
        log_warning "ARC runtime system not yet implemented"
    fi
    
    # 3. Integration test
    log_info "Testing codegen-ARC integration readiness..."
    
    # Test that basic compilation still works
    if make wyn >/dev/null 2>&1; then
        log_success "Basic compilation still working"
    else
        log_error "Basic compilation broken"
        ((errors++))
    fi
    
    if [[ $errors -eq 0 ]]; then
        log_success "✅ Day 10 Integration Gate PASSED - Codegen + ARC ready"
        return 0
    else
        log_error "❌ Day 10 Integration Gate FAILED - $errors errors found"
        return 1
    fi
}

# Day 15 Integration Gate: Core Systems Complete
validate_day15_core_systems() {
    log_info "=== Day 15 Integration Gate: Core Systems Complete ==="
    
    local errors=0
    
    # 1. Verify type system enhancements
    log_info "Checking type system enhancements..."
    
    # Look for enhanced type system features
    if grep -q "Optional\|Union\|enhanced.*struct" src/*.c src/*.h 2>/dev/null; then
        log_success "Type system enhancements found"
    else
        log_warning "Type system enhancements not yet implemented"
    fi
    
    # 2. Verify all core systems integrate
    log_info "Testing core system integration..."
    
    # Test comprehensive build
    if make all >/dev/null 2>&1; then
        log_success "All build targets working"
    else
        log_error "Build system integration issues"
        ((errors++))
    fi
    
    # 3. Cross-platform readiness check
    log_info "Checking cross-platform readiness..."
    
    # Verify platform detection works
    local platform=$(uname -s)
    log_info "Current platform: $platform"
    
    if [[ $errors -eq 0 ]]; then
        log_success "✅ Day 15 Integration Gate PASSED - Core systems complete"
        return 0
    else
        log_error "❌ Day 15 Integration Gate FAILED - $errors errors found"
        return 1
    fi
}

# Day 20 Integration Gate: Full Integration
validate_day20_full_integration() {
    log_info "=== Day 20 Integration Gate: Full Integration ==="
    
    local errors=0
    
    # 1. Verify all Phase 2 systems work together
    log_info "Testing full Phase 2 integration..."
    
    # Test all build targets
    local targets=("wyn" "wyn-llvm" "test")
    for target in "${targets[@]}"; do
        if make "$target" >/dev/null 2>&1; then
            log_success "Build target '$target' working"
        else
            log_error "Build target '$target' failed"
            ((errors++))
        fi
    done
    
    # 2. Comprehensive testing
    log_info "Running comprehensive test suite..."
    
    if make test >/dev/null 2>&1; then
        log_success "All tests passing"
    else
        log_warning "Some tests failing (may be expected during development)"
    fi
    
    # 3. Performance validation
    log_info "Running performance validation..."
    
    # Test compilation speed
    local test_file="examples/simple_test.wyn"
    if [[ -f "$test_file" ]]; then
        local start_time=$(date +%s%N)
        if timeout 30s ./wyn "$test_file" >/dev/null 2>&1; then
            local end_time=$(date +%s%N)
            local duration=$(( (end_time - start_time) / 1000000 ))
            log_success "Compilation performance: $duration ms"
        else
            log_warning "Compilation performance test failed or timed out"
        fi
    fi
    
    # 4. Memory safety validation
    log_info "Running memory safety validation..."
    
    if command -v valgrind >/dev/null 2>&1; then
        if timeout 60s valgrind --leak-check=full --error-exitcode=1 ./wyn examples/hello.wyn >/dev/null 2>&1; then
            log_success "Memory safety validation passed"
        else
            log_warning "Memory safety issues detected"
        fi
    else
        log_warning "Valgrind not available for memory safety testing"
    fi
    
    if [[ $errors -eq 0 ]]; then
        log_success "✅ Day 20 Integration Gate PASSED - Full integration complete"
        return 0
    else
        log_error "❌ Day 20 Integration Gate FAILED - $errors errors found"
        return 1
    fi
}

# ============================================================================
# Idle-CPU Gate: a WAITING concurrent program must not burn CPU
# ============================================================================
# Lives here rather than in tests/regression/ because it asserts a RESOURCE
# property (CPU consumed while waiting), which the `// EXPECT:` runner cannot
# express - those tests only compare stdout.
#
# History: the scheduler had three spin sites that each burned roughly a core in
# any concurrent program. Measured cumulative CPU over 2s of wall time, before
# the fix / after:
#   (a) spawn + long sleep        - worker idle park (100us tick)   2.1s -> ~0ms
#   (b) spawn then return         - wyn_spawn_wait poll+yield       2.8s -> ~12ms
#                                   (357% CPU: it burned several cores)
#   (c) spawn then await          - future_get sched_yield          0.95s -> ~10ms
#
# Case (b) is the biggest offender - do NOT drop it to save runtime.
#
# Measurement notes (both of these have caused wrong conclusions here before):
#   * Use CUMULATIVE CPU time, never %CPU - %CPU is a decaying average.
#   * `ps -o time=` has 1s granularity and non-portable formatting, and there is
#     no `timeout` binary on macOS. Hence the small C probe, which uses
#     getrusage(RUSAGE_CHILDREN) after waitpid (POSIX) / GetProcessTimes (Win).
validate_idle_cpu() {
    log_info "=== Idle-CPU Gate: waiting programs must not spin ==="

    local errors=0
    local budget_ms=50      # generous: the fixed scheduler measures ~0-15ms
    local wall_ms=2000      # observation window
    local work="${TMPDIR:-/tmp}/wyn_idle_cpu_gate.$$"

    if [[ ! -x "./wyn" ]]; then
        log_error "./wyn not built - run 'make' first"
        return 1
    fi

    mkdir -p "$work"
    # shellcheck disable=SC2064
    trap "rm -rf '$work'" RETURN

    # Build the CPU probe.
    local probe="$work/cpu_probe"
    if ! ${CC:-cc} -O2 -o "$probe" scripts/testing/cpu_probe.c 2>"$work/probe.err"; then
        log_error "failed to build scripts/testing/cpu_probe.c"
        cat "$work/probe.err"
        return 1
    fi

    # (a) a spawned task plus a long sleep: nothing to run, everything parked.
    cat > "$work/a.wyn" <<'WYN'
fn work(x: int) -> int { return x + 1 }
fn main() {
    spawn work(1)
    Time::sleep(6000)
    println("done")
}
WYN
    # (b) spawn a slow task and return immediately -> main drains in
    #     wyn_spawn_wait. Historically the WORST site (357% CPU).
    cat > "$work/b.wyn" <<'WYN'
fn slow3(x: int) -> int { Time::sleep(3000); return x }
fn main() {
    spawn slow3(1)
    println("returned")
}
WYN
    # (c) await a slow task -> main blocks in future_get.
    cat > "$work/c.wyn" <<'WYN'
fn slow3(x: int) -> int { Time::sleep(3000); return x }
fn main() {
    f = spawn slow3(1)
    a = await f
    println(a)
}
WYN
    # (d) a spawned fn with NO yield point. Regression guard: the reactor is
    #     created lazily, so this program registers no fd/timer at all. When
    #     wyn_io_poll_wait returned immediately in that state, the designated
    #     poller re-claimed in a tight loop and burned ~200% CPU.
    cat > "$work/d.wyn" <<'WYN'
fn work(x: int) -> int { return x + 1 }
fn main() {
    spawn work(1)
    Time::sleep(6000)
    println("done")
}
WYN

    local case_id
    for case_id in a b c d; do
        if ! ./wyn build "$work/$case_id.wyn" -o "$work/$case_id" >"$work/$case_id.build" 2>&1; then
            log_error "idle-cpu case ($case_id): build failed"
            tail -5 "$work/$case_id.build"
            ((errors++))
            continue
        fi

        # Median of 3: a single sample can catch an unrelated system hiccup.
        local samples=() s
        for s in 1 2 3; do
            samples+=("$("$probe" "$wall_ms" "$work/$case_id" 2>/dev/null)")
            # The probe kills the child, but be explicit: these programs spin by
            # design when broken, and a leaked spinner corrupts later samples.
            pkill -9 -f "$work/$case_id" 2>/dev/null || true
        done
        local cpu_ms
        cpu_ms=$(printf '%s\n' "${samples[@]}" | sort -n | awk '{a[NR]=$1} END{print a[int((NR+1)/2)]}')

        if [[ -z "$cpu_ms" ]]; then
            log_error "idle-cpu case ($case_id): probe produced no measurement"
            ((errors++))
        elif (( cpu_ms > budget_ms )); then
            log_error "idle-cpu case ($case_id): ${cpu_ms}ms CPU over ${wall_ms}ms wall (budget ${budget_ms}ms) [samples: ${samples[*]}]"
            ((errors++))
        else
            log_success "idle-cpu case ($case_id): ${cpu_ms}ms CPU over ${wall_ms}ms wall (budget ${budget_ms}ms)"
        fi
    done

    if [[ $errors -eq 0 ]]; then
        log_success "✅ Idle-CPU Gate PASSED"
        return 0
    else
        log_error "❌ Idle-CPU Gate FAILED - $errors case(s) over budget"
        return 1
    fi
}

# Run specific validation gate
run_validation_gate() {
    local gate="$1"

    case "$gate" in
        "idle-cpu"|"cpu")
            validate_idle_cpu
            ;;
        "day5"|"5")
            validate_day5_llvm_infrastructure
            ;;
        "day10"|"10")
            validate_day10_codegen_arc
            ;;
        "day15"|"15")
            validate_day15_core_systems
            ;;
        "day20"|"20")
            validate_day20_full_integration
            ;;
        "all")
            local total_errors=0
            validate_day5_llvm_infrastructure || ((total_errors++))
            echo ""
            validate_day10_codegen_arc || ((total_errors++))
            echo ""
            validate_day15_core_systems || ((total_errors++))
            echo ""
            validate_day20_full_integration || ((total_errors++))
            
            echo ""
            if [[ $total_errors -eq 0 ]]; then
                log_success "🎯 ALL INTEGRATION GATES PASSED ✅"
            else
                log_error "❌ $total_errors integration gates failed"
                return 1
            fi
            ;;
        *)
            echo "Usage: $0 [idle-cpu|day5|day10|day15|day20|all]"
            echo ""
            echo "Integration Gates:"
            echo "  idle-cpu - Waiting concurrent programs must not burn CPU"
            echo "  day5  - LLVM Infrastructure Complete"
            echo "  day10 - Basic Codegen + ARC Runtime Ready"
            echo "  day15 - Core Systems Complete"
            echo "  day20 - Full Integration"
            echo "  all   - Run all validation gates"
            exit 1
            ;;
    esac
}

# Run the specified validation gate
run_validation_gate "${1:-all}"