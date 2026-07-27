#!/bin/bash
# Parallel BDD + Regression test runner
# Runs all .wyn tests concurrently using background jobs
set -uo pipefail

WYN="${WYN:-./wyn}"
TMPDIR=$(mktemp -d)
PASS=0
FAIL=0
TOTAL=0
ERRORS=""

# Per-command watchdog: wall-clock alarm + CPU rlimit. Stock macOS has no
# `timeout` binary, so use perl's alarm. A single looping/leaking test binary,
# multiplied across parallel shards, can exhaust host memory (this has
# kernel-panicked a dev machine) — never run one unbounded.
WYN_TEST_TIMEOUT="${WYN_TEST_TIMEOUT:-30}"
with_limits() {
    ( ulimit -t $((WYN_TEST_TIMEOUT * 2)) 2>/dev/null
      exec perl -e 'alarm shift; exec @ARGV or exit 127' "$WYN_TEST_TIMEOUT" "$@" )
}

run_test() {
    local file="$1"
    local name=$(basename "$file")
    local result_file="$TMPDIR/$name.result"

    local expected=$(grep '// EXPECT:' "$file" | sed 's|// EXPECT: ||')
    if [ -z "$expected" ]; then
        echo "SKIP" > "$result_file"
        return
    fi

    # --- Robust, deterministic build+run (fixes the macos-15 empty-output flake) ---
    # Root causes the old one-liner exposed on the macos-15 runner:
    #   (a) build+run+rm jammed in one subshell -> a slow/parallel `wyn build`
    #       could race the immediate exec, or the `rm` could delete the binary
    #       before it was executed / its stdout captured;
    #   (b) stdout captured before the child's buffers flushed -> empty output;
    #   (c) fixed binary path per source -> parallel shards clobbered each other;
    #   (d) the written binary not observed as present before exec.
    # Fix: unique artifact paths per invocation (defeats (c)); build as its own
    # step and verify the binary exists+executable before running (defeats (a)/(d)
    # and turns a real build failure into an explicit BUILD-FAIL instead of a
    # silent empty-output "wrong answer"); capture run output separately AFTER the
    # binary is confirmed present; retry an EMPTY-but-expected result up to 2x
    # (defeats (b) — a genuinely wrong answer is non-empty so it is never retried).

    # Unique per-invocation artifact directory so parallel shards never collide.
    local sandbox="$TMPDIR/run.$name.$$.$RANDOM"
    mkdir -p "$sandbox"
    local bin="$sandbox/$(basename "${file%.wyn}")"

    # Step 1: build to a unique output path. Keep the source tree clean.
    # A build that fails TRANSIENTLY is retried up to 5x with backoff. Transient =
    # either (i) no diagnostic text at all, or (ii) a host resource-exhaustion
    # error from the toolchain — on the macos-15 runner, launching the whole suite
    # in parallel starves the process table and clang dies with
    # "posix_spawn failed: Resource temporarily unavailable" / "unable to fork".
    # A build that fails with a REAL compiler diagnostic is a genuine error and is
    # reported immediately — never retried away.
    local build_err build_rc build_diag
    local battempt=0
    while [ "$battempt" -lt 5 ]; do
        build_err=$(with_limits "$WYN" build "$file" -o "$bin" 2>&1 >/dev/null)
        build_rc=$?
        rm -f "${file%.wyn}" "${file}.c" 2>/dev/null
        # Present + executable => build succeeded, proceed.
        [ "$build_rc" -eq 0 ] && [ -x "$bin" ] && break
        build_diag="$(echo "$build_err" | grep -v '^Building\|^Built\|^Compiled in\|Warning:' | head -3 | tr '\n' ' ')"
        # Host resource-exhaustion => transient, retry with backoff.
        if echo "$build_diag" | grep -qiE 'Resource temporarily unavailable|posix_spawn|unable to fork|Cannot allocate memory|too many open files'; then
            rm -f "$bin" 2>/dev/null
            battempt=$((battempt + 1))
            sleep "0.$((battempt * 3))"
            continue
        fi
        # Real diagnostic => genuine build error, stop and report.
        [ -n "$build_diag" ] && break
        # Empty diagnostic => transient flake, retry.
        rm -f "$bin" 2>/dev/null
        battempt=$((battempt + 1))
        sleep 0.2
    done

    # Step 2: verify the binary is present and executable before running it.
    if [ "$build_rc" -ne 0 ] || [ ! -x "$bin" ]; then
        printf "FAIL\n    BUILD FAILED (rc=%s): %s\n" "$build_rc" "$build_diag" \
            > "$result_file"
        rm -rf "$sandbox"
        return
    fi

    # Step 3: run & capture. Retry only when output is EMPTY but something was
    # expected (the flush/timing flake) — never masks a non-empty wrong answer.
    local output=""
    local attempt=0
    while [ "$attempt" -lt 3 ]; do
        output=$(with_limits "$bin" 2>&1)
        output=$(echo "$output" | grep -v "Building\|Built\|Compiled in\|Warning:")
        if [ -n "$output" ]; then
            break
        fi
        attempt=$((attempt + 1))
        sleep 0.2
    done

    # Step 4: clean up AFTER capture, so cleanup can never race the run.
    rm -rf "$sandbox"

    local failed=0
    local errs=""
    local i=1
    while IFS= read -r exp_line; do
        local actual_line=$(echo "$output" | sed -n "${i}p")
        if [ "$actual_line" != "$exp_line" ]; then
            failed=1
            errs="${errs}    expected: $exp_line\n    actual:   $actual_line\n"
        fi
        i=$((i + 1))
    done <<< "$expected"

    if [ "$failed" -eq 0 ]; then
        echo "PASS" > "$result_file"
    else
        printf "FAIL\n%b" "$errs" > "$result_file"
    fi
}

# Collect all test files
FILES=()
for f in tests/expect/*.wyn tests/regression/*.wyn; do
    [ -f "$f" ] && FILES+=("$f")
done

# Launch tests in parallel, but BOUND concurrency. Launching all ~180 at once
# means ~180 concurrent `wyn build`->clang processes; on a constrained runner
# (macos-15) that starves the process table and clang dies with
# "posix_spawn failed: Resource temporarily unavailable". Cap at a multiple of
# the CPU count so we still parallelize hard on big machines without fork-storming
# small ones. Override with WYN_TEST_JOBS.
if [ "${#FILES[@]}" -gt 0 ]; then
    ncpu=$( (getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4) )
    max_jobs="${WYN_TEST_JOBS:-$((ncpu * 2))}"
    [ "$max_jobs" -lt 1 ] 2>/dev/null && max_jobs=4
    running=0
    for f in "${FILES[@]}"; do
        run_test "$f" &
        running=$((running + 1))
        if [ "$running" -ge "$max_jobs" ]; then
            # Bash 3.2 (macOS default) has no `wait -n`; drain the batch. Each
            # test is short, so batch-draining keeps utilization high enough.
            wait
            running=0
        fi
    done
    wait
fi

# Collect results
echo "=== Expect Tests ==="
for f in tests/expect/*.wyn; do
    [ -f "$f" ] || continue
    name=$(basename "$f")
    rf="$TMPDIR/$name.result"
    [ -f "$rf" ] || continue
    status=$(head -1 "$rf")
    if [ "$status" = "SKIP" ]; then continue; fi
    TOTAL=$((TOTAL + 1))
    if [ "$status" = "PASS" ]; then
        PASS=$((PASS + 1))
        echo "  ✓ $name"
    else
        FAIL=$((FAIL + 1))
        echo "  ✗ $name"
        ERRORS="${ERRORS}\n  FAIL: $name\n$(tail -n +2 "$rf")"
    fi
done

echo ""
echo "=== Regression Tests ==="
for f in tests/regression/*.wyn; do
    [ -f "$f" ] || continue
    name=$(basename "$f")
    rf="$TMPDIR/$name.result"
    [ -f "$rf" ] || continue
    status=$(head -1 "$rf")
    if [ "$status" = "SKIP" ]; then continue; fi
    TOTAL=$((TOTAL + 1))
    if [ "$status" = "PASS" ]; then
        PASS=$((PASS + 1))
        echo "  ✓ $name"
    else
        FAIL=$((FAIL + 1))
        echo "  ✗ $name"
        ERRORS="${ERRORS}\n  FAIL: $name\n$(tail -n +2 "$rf")"
    fi
done

echo ""
echo "Results: $PASS pass, $FAIL fail"
if [ -n "$ERRORS" ]; then
    echo -e "\nFailures:$ERRORS"
fi
rm -rf "$TMPDIR"
exit $FAIL
