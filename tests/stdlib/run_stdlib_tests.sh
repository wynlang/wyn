#!/bin/bash
# Stdlib suite runner.
#
# tests/stdlib/*.wyn (68 files) were run by NOTHING before this script existed:
# run_bdd.sh only walks tests/expect/ + tests/regression/, run_tests_parallel.sh
# reads a tests/test_list.txt that isn't in the tree (so it runs 0 tests), and
# the Makefile `test:` target never mentioned stdlib either. That is how a
# batch of stdlib regressions shipped unnoticed.
#
# These files are NOT `// EXPECT:` tests - they call Test.init/Test.assert_* and
# rely on wyn_test_exit_code() (src/test_runtime.c) to turn a failed assertion
# into a nonzero process exit. So the contract here is purely exit-code based:
#
#   PASS  build rc == 0  AND  run rc == 0
#   FAIL  build failed, or run exited nonzero, or run died on a fault signal
#         (SIGSEGV/SIGABRT/SIGBUS/SIGFPE -> rc 134/136/138/139)
#   HANG  build or run killed by the WATCHDOG specifically - SIGALRM (rc 142),
#         SIGXCPU (rc 152) or SIGKILL (rc 137)
#
# Note the signal distinction: rc >= 128 alone does NOT mean "hang". `panic:
# abort` raises SIGABRT and shows up as rc 134 in ~1s; calling that a timeout
# would hide a hard crash behind a "flaky/slow" label. Only the watchdog's own
# signals count as HANG.
#
# The suite is NOT clean today, so it cannot be a plain hard gate. Instead it
# enforces a known-failure allowlist (tests/stdlib/known_failures.txt):
#
#   * a test that is broken AND listed        -> reported, does not fail the run
#   * a test that is broken and NOT listed    -> NEW BREAKAGE, fails the run
#   * a listed test that now passes           -> reported as FIXED; remove it
#                                                from the allowlist. Advisory by
#                                                default (some listed entries are
#                                                timing-sensitive hangs and would
#                                                otherwise flake CI red); set
#                                                WYN_STDLIB_STRICT=1 to make it
#                                                fatal.
#
# The allowlist is deliberately a visible, shrinking debt list - not a mute
# button. Every line in it is a bug someone still has to fix.
#
# Watchdog: stock macOS has no `timeout` binary, so use the same
# `perl -e 'alarm ...; exec @ARGV'` idiom as tests/errors/*.sh and run_bdd.sh,
# plus a CPU rlimit. Five of these tests hang; unbounded they would wedge CI and
# (multiplied across shards) can exhaust host memory - that has kernel-panicked a
# dev machine before. Never run one unbounded.
#
# Usage:
#   WYN=./wyn bash tests/stdlib/run_stdlib_tests.sh
#   WYN=./wyn bash tests/stdlib/run_stdlib_tests.sh --update-allowlist
#   WYN_STDLIB_STRICT=1 ... (also fail on unexpected passes)
#
# Env: WYN, WYN_STDLIB_TIMEOUT (default 60s run / 90s build), WYN_STDLIB_JOBS.
set -uo pipefail

WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SUITE_DIR="$ROOT/tests/stdlib"
ALLOWLIST="$SUITE_DIR/known_failures.txt"

UPDATE=0
[ "${1:-}" = "--update-allowlist" ] && UPDATE=1

RUN_TIMEOUT="${WYN_STDLIB_TIMEOUT:-60}"
BUILD_TIMEOUT=$((RUN_TIMEOUT + 30))

TMPDIR_=$(mktemp -d)
trap 'rm -rf "$TMPDIR_"' EXIT

# Wall-clock alarm + CPU rlimit. Stock macOS has no `timeout`; this is the same
# perl-alarm idiom tests/errors/*.sh and run_bdd.sh already use.
with_limits() {
    local secs="$1"; shift
    ( ulimit -t $((secs * 2)) 2>/dev/null
      exec perl -e 'alarm shift; exec @ARGV or exit 127' "$secs" "$@" )
}

# True only for the watchdog's own kill signals: SIGALRM (142) from perl's
# alarm, SIGXCPU (152) from the CPU rlimit, SIGKILL (137) from an OOM killer.
# Deliberately excludes 134/136/138/139 (ABRT/FPE/BUS/SEGV) - those are crashes.
is_timeout_rc() {
    case "$1" in 142|152|137) return 0 ;; esac
    return 1
}

# Classify one test file. Writes "<STATUS>\t<detail>" to $TMPDIR_/<name>.result.
run_one() {
    local file="$1"
    local name; name=$(basename "$file")
    local result_file="$TMPDIR_/$name.result"

    local sandbox="$TMPDIR_/run.$name.$$.$RANDOM"
    mkdir -p "$sandbox"
    local bin="$sandbox/$(basename "${file%.wyn}")"

    local build_out build_rc
    build_out=$(with_limits "$BUILD_TIMEOUT" "$WYN" build "$file" -o "$bin" 2>&1 >/dev/null)
    build_rc=$?
    # `wyn build` can drop side artifacts next to the source; keep the tree clean.
    rm -f "${file%.wyn}" "${file}.c" 2>/dev/null

    if is_timeout_rc "$build_rc"; then
        printf 'HANG\tbuild timed out after %ss (rc=%s)\n' "$BUILD_TIMEOUT" "$build_rc" > "$result_file"
        rm -rf "$sandbox"; return
    fi
    if [ "$build_rc" -ne 0 ] || [ ! -x "$bin" ]; then
        printf 'FAIL\tbuild failed (rc=%s): %s\n' "$build_rc" \
            "$(echo "$build_out" | grep -v '^Building\|^Built\|^Compiled in' | head -2 | tr '\n' ' ')" \
            > "$result_file"
        rm -rf "$sandbox"; return
    fi

    local run_out run_rc t0 t1
    t0=$(date +%s)
    run_out=$(with_limits "$RUN_TIMEOUT" "$bin" 2>&1)
    run_rc=$?
    t1=$(date +%s)
    rm -rf "$sandbox"

    if is_timeout_rc "$run_rc"; then
        printf 'HANG\trun killed by watchdog after %ss (rc=%s)\n' "$((t1 - t0))" "$run_rc" > "$result_file"
    elif [ "$run_rc" -ne 0 ]; then
        printf 'FAIL\trun rc=%s after %ss: %s\n' "$run_rc" "$((t1 - t0))" \
            "$(echo "$run_out" | grep -iE 'FAILED|✗|Segmentation|Abort|panic|error' | head -2 | tr '\n' ' ')" \
            > "$result_file"
    else
        printf 'PASS\t-\n' > "$result_file"
    fi
}

FILES=()
for f in "$SUITE_DIR"/*.wyn; do
    [ -f "$f" ] && FILES+=("$f")
done
if [ "${#FILES[@]}" -eq 0 ]; then
    echo "stdlib: no test files found under $SUITE_DIR" >&2
    exit 1
fi

# Bounded parallelism. Each hang parks a slot for the whole timeout, so cap at
# the CPU count (not a multiple of it) to keep peak memory sane.
ncpu=$( (getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4) )
max_jobs="${WYN_STDLIB_JOBS:-$ncpu}"
[ "$max_jobs" -lt 1 ] 2>/dev/null && max_jobs=4

running=0
for f in "${FILES[@]}"; do
    run_one "$f" &
    running=$((running + 1))
    if [ "$running" -ge "$max_jobs" ]; then
        # Bash 3.2 (macOS default) has no `wait -n`; drain the batch.
        wait
        running=0
    fi
done
wait

# --- Load the allowlist: one bare filename per line, '#' comments, optional
# --- trailing status word that is informational only (matching is by name).
ALLOWED_NAMES=""
if [ -f "$ALLOWLIST" ]; then
    while IFS= read -r line; do
        line="${line%%#*}"
        set -- $line
        [ "$#" -eq 0 ] && continue
        ALLOWED_NAMES="$ALLOWED_NAMES $1"
    done < "$ALLOWLIST"
fi
is_allowed() {
    case " $ALLOWED_NAMES " in *" $1 "*) return 0 ;; esac
    return 1
}

PASS=0; FAILN=0; HANGN=0
NEW_BREAK=""
FIXED=""
KNOWN=""
declare_lines=""

echo "=== Stdlib Suite (${#FILES[@]} files, ${max_jobs} jobs, ${RUN_TIMEOUT}s run watchdog) ==="
for f in "${FILES[@]}"; do
    name=$(basename "$f")
    rf="$TMPDIR_/$name.result"
    if [ ! -f "$rf" ]; then
        status="FAIL"; detail="runner produced no result"
    else
        status=$(cut -f1 "$rf")
        detail=$(cut -f2- "$rf")
    fi

    case "$status" in
        PASS)
            PASS=$((PASS + 1))
            if is_allowed "$name"; then
                echo "  ✓ $name  (was allowlisted - FIXED, remove from known_failures.txt)"
                FIXED="${FIXED}    $name\n"
            else
                echo "  ✓ $name"
            fi
            ;;
        HANG)
            HANGN=$((HANGN + 1))
            declare_lines="${declare_lines}${name} hang\n"
            if is_allowed "$name"; then
                echo "  ⏱ $name  (known hang) $detail"
                KNOWN="${KNOWN}    hang $name\n"
            else
                echo "  ⏱ $name  NEW HANG: $detail"
                NEW_BREAK="${NEW_BREAK}    HANG $name - $detail\n"
            fi
            ;;
        *)
            FAILN=$((FAILN + 1))
            declare_lines="${declare_lines}${name} fail\n"
            if is_allowed "$name"; then
                echo "  ✗ $name  (known failure) $detail"
                KNOWN="${KNOWN}    fail $name\n"
            else
                echo "  ✗ $name  NEW FAILURE: $detail"
                NEW_BREAK="${NEW_BREAK}    FAIL $name - $detail\n"
            fi
            ;;
    esac
done

echo ""
echo "Stdlib results: $PASS pass, $FAILN fail, $HANGN hang (of ${#FILES[@]})"

if [ "$UPDATE" -eq 1 ]; then
    {
        echo "# tests/stdlib known-failure allowlist  (generated: $(date -u +%Y-%m-%d))"
        echo "#"
        echo "# Every line here is an OPEN BUG in the stdlib suite, not a waiver."
        echo "# tests/stdlib/run_stdlib_tests.sh fails CI on any breakage NOT listed"
        echo "# here, so this file is the debt list: it must only ever shrink."
        echo "#"
        echo "# Format: <filename> <fail|hang>   (# starts a comment)"
        echo "#   fail = builds but exits nonzero (failed Test.assert_* / crash)"
        echo "#   hang = never terminates; killed by the runner watchdog"
        echo "#"
        echo "# Regenerate with:"
        echo "#   WYN=./wyn bash tests/stdlib/run_stdlib_tests.sh --update-allowlist"
        echo ""
        printf "%b" "$declare_lines" | sort
    } > "$ALLOWLIST"
    echo "Wrote $ALLOWLIST ($(grep -cve '^\s*#' -e '^\s*$' "$ALLOWLIST") entries)"
    exit 0
fi

rc=0
if [ -n "$NEW_BREAK" ]; then
    echo ""
    echo "NEW stdlib breakage (not in $(basename "$ALLOWLIST")):"
    printf "%b" "$NEW_BREAK"
    echo "  -> This is a regression. Fix it, or (only if it is genuinely"
    echo "     pre-existing) add it to tests/stdlib/known_failures.txt."
    rc=1
fi
if [ -n "$FIXED" ]; then
    echo ""
    echo "Allowlisted tests that now PASS - shrink the debt list:"
    printf "%b" "$FIXED"
    if [ "${WYN_STDLIB_STRICT:-0}" = "1" ]; then
        echo "  -> WYN_STDLIB_STRICT=1: treating as an error."
        rc=1
    fi
fi
if [ -n "$KNOWN" ]; then
    echo ""
    echo "Known (allowlisted) stdlib debt still outstanding:"
    printf "%b" "$KNOWN"
fi

if [ "$rc" -eq 0 ]; then
    echo ""
    echo "stdlib: no new breakage"
fi
exit "$rc"
