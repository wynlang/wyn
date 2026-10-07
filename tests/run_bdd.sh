#!/bin/bash
# Parallel BDD + Regression test runner
# Runs all .wyn tests concurrently using background jobs
#
# ---------------------------------------------------------------------------
# DIRECTIVES a test file may carry (all are `//` comments, anywhere in the file)
# ---------------------------------------------------------------------------
#   // EXPECT: <line>        The program must BUILD, RUN, and print <line> as
#                            the Nth line of its output (N = the Nth EXPECT).
#   // EXPECT_FAIL:          The program must NOT compile, in EITHER phase.
#                            `wyn check` (and, if check passes, `wyn build`)
#                            must exit non-zero with a status in [1,127]. A
#                            program that compiles is a test FAILURE, and so is
#                            a crash/timeout (rc >= 128) — "rejected" means a
#                            diagnostic, not a signal.
#   // EXPECT_CHECK_FAIL:    Stricter: rejection must come from `wyn check`.
#                            There is NO build fallback, so a check that exits 0
#                            is a FAILURE even if the build would have failed.
#                            Use this whenever the rule under test is a CHECKER
#                            rule — see "WHICH PHASE" below; EXPECT_FAIL alone
#                            cannot express it and silently accepts a rule that
#                            has regressed into the C backend.
#   // EXPECT_ERR: <text>    <text> must appear, as a LITERAL substring, in the
#                            compiler's output. Repeatable; ALL must match.
#                            Implies rejection mode on its own. A pattern that
#                            also occurs in the test's own code is REJECTED up
#                            front — see "SELF-MATCH" below.
#   // EXPECT_EXIT: <n>      The compiled program's exit status must be exactly
#                            <n>. DEFAULT IS 0 — every accept-mode test asserts
#                            a clean exit whether or not it spells this out, so
#                            this directive is only for a test that deliberately
#                            exits non-zero. Accept mode only (a rejection test
#                            never runs, so combining the two is reported as a
#                            directive conflict).
#
# WHY EXPECT_EXIT EXISTS: until it did, the accept path captured the program's
# stdout and threw its exit status away. A program that printed every expected
# line and then PANICKED was scored PASS — measured: a 3-line test printing its
# one EXPECT line and then dividing by zero (`panic ... division by zero`,
# rc=1) was reported "✓". Every tally this suite has ever printed was therefore
# asserting "the right text appeared somewhere in the output", not "the program
# ran to completion". tests/bdd_selftest/expect_exit_must_be_enforced.wyn is the
# negative control that keeps the status check from decaying back into that.
#
# These directives exist so that a rejection rule can be gated by a ~5 line
# auto-discovered .wyn file instead of a bespoke tests/errors/run_*.sh (123 of
# those exist, ~19,600 lines, median 129) plus a hand-edited Makefile roster
# line. Only PRESENCE of an expected string is ever asserted: asserting the
# ABSENCE of an error string passes spuriously the moment the error moves.
#
# The three rejection directives must START their comment (`^\s*// EXPECT_...:`).
# Do NOT spell one inside explanatory prose: directives are found by scanning
# text, so a sentence mentioning one used to BECOME one — a doc line quoting
# `EXPECT_ERR:` turned the remainder of that sentence into a required error
# pattern. Write directive names bare in prose.
#
# WHICH PHASE REJECTED is a property worth asserting, not an implementation
# detail. The historical defect shape these gates were written for is exactly
# "the rule used to fire in the C backend as a raw clang error, and was moved
# into the checker so the user gets a Wyn diagnostic". EXPECT_FAIL cannot tell
# those apart: it is satisfied by a non-zero exit from either phase. So a
# checker rule that regressed back into the backend would keep a green
# EXPECT_FAIL gate. EXPECT_CHECK_FAIL is the directive that pins the phase, and
# it is what the two tests/errors/ scripts mirrored here actually asserted
# (`wyn check` only, rc in [1,127]).
#
# COMPILE MODE: this runner only ever exercises ONE mode. The accept path builds
# with plain `wyn build` (debug) and there is no `--release` arm anywhere in this
# script; the second invocation CI makes (`ci.yml:167`) differs only by the
# WYN_ASYNC_CORO=1 environment variable, not by compile mode. The rejection path
# deliberately matches that: `wyn check`, then plain `wyn build`. If a release-mode
# arm is ever added to the accept path, add it here in the same shape.
#
# WHICH STREAM IS SEARCHED, and why: stdout and stderr COMBINED (`2>&1`).
# Not negotiable, and not an accident of convenience — the compiler does not
# route its diagnostics uniformly, and the asymmetry was MEASURED on this tree:
#   * `src/lexer.c`'s empty-radix error is emitted on BOTH streams (grep -c
#     "hex literal" = 2 on stdout alone and 2 on stderr alone), so either
#     stream alone would match it.
#   * `src/checker.c:6950`'s "struct 'S' has no field 'f'" is stderr-ONLY
#     (stdout count 0, stderr count 1), as are the C backend's own errors.
# That second case is the load-bearing one: matching stdout alone would miss
# every checker diagnostic, so the streams must be combined. Searching one
# stream would also make an EXPECT_ERR gate silently stop matching when a
# diagnostic is re-plumbed, which is a failure mode this repo has already been
# burned by.
#
# SELF-MATCH is the subtlest hazard here, and it is worse than "don't copy the
# source line". The compiler echoes the test's own text back in two places —
# `show_source_line()` (src/checker.c:21, one line) and the rich `--> file:l:c`
# renderer, which prints a WINDOW of SURROUNDING lines. A window a few lines wide
# reaches the directive block itself, so the output legitimately contains
# "  10 | // EXPECT_ERR: <pattern>" and EVERY pattern matches itself. Measured:
# with the raw output, the EXPECT_ERR negative control passed on nothing but that
# echo — the directive asserted literally nothing. Two defences, both needed:
#   1. run_rejection_test strips the echoed source context (gutter lines) out of
#      the text it searches, so a pattern must appear in a real diagnostic.
#   2. It also rejects, up front, any pattern that occurs in the test file's own
#      NON-DIRECTIVE lines — code AND comments, since the window echoes comments
#      too. Directive lines are necessarily exempt, which is why (1) exists.
# Write the pattern from the DIAGNOSTIC ("has no field 'namee'"), never from the
# program ("println(u.namee)").
#
# Mixing `// EXPECT:` with a rejection directive is itself reported as a test
# failure: a file cannot both run and fail to compile.
#
# NEGATIVE CONTROLS: tests/bdd_selftest/*.wyn are deliberately-wrong files whose
# directives their own source does NOT satisfy, so the runner is required to
# report FAIL for each. They are the ONLY mechanism that keeps the directives
# above non-vacuous: without them EXPECT_FAIL could degrade into a no-op and
# every rejection test in the suite would pass while asserting nothing. The
# invariant is ONE CONTROL PER DIRECTIVE/RULE, so the block below asserts a
# discovery FLOOR (a floor, not an equality, so adding a directive and its
# control does not red the suite) and FAILS — never skips — when it is unmet.
# A glob that matched nothing used to skip the whole block silently.
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

# Rejection mode: `// EXPECT_FAIL:` / `// EXPECT_CHECK_FAIL:` / `// EXPECT_ERR:`
# (see the header block). Writes PASS/FAIL to $result_file exactly like the
# accept path does. $check_only=1 selects EXPECT_CHECK_FAIL semantics: the
# rejection must come from `wyn check` and there is no build fallback.
run_rejection_test() {
    local file="$1"; local result_file="$2"; local want_err="$3"; local check_only="$4"
    local sandbox="$TMPDIR/rej.$(basename "$file").$$.$RANDOM"

    # SELF-MATCH GUARD (see header). The compiler echoes the test's own source
    # back, so a pattern lifted out of the file under test would match even with
    # the rule deleted. Checked BEFORE compiling: that is a defect in the TEST,
    # not in the compiler, and it deserves its own message rather than a
    # confusing "not found" later. Comment lines are scanned too, not just code:
    # the diagnostic renderer prints a WINDOW of surrounding lines, so a comment
    # near the error is echoed just as readily as the offending statement. Only
    # the directive lines themselves are exempt (they necessarily carry the
    # pattern), which is why the echo strip below is also needed.
    local selfmatch="" nondirective pat
    nondirective=$(grep -v '^[[:space:]]*// EXPECT' "$file")
    while IFS= read -r pat; do
        [ -n "$pat" ] || continue
        if printf '%s' "$nondirective" | grep -qF -- "$pat"; then
            selfmatch="${selfmatch}    EXPECT_ERR pattern occurs in this test's own source, so the compiler's echo of that line would satisfy it even with the rule deleted; assert the DIAGNOSTIC text instead: $pat\n"
        fi
    done <<< "$want_err"
    if [ -n "$selfmatch" ]; then
        printf "FAIL\n%b" "$selfmatch" > "$result_file"
        return
    fi

    mkdir -p "$sandbox"

    # `wyn check` first: most rules reject there and it writes no artifacts.
    local out rc check_rc
    out=$(with_limits "$WYN" check "$file" 2>&1); rc=$?; check_rc=$rc
    if [ "$rc" -eq 0 ] && [ "$check_only" != "1" ]; then
        # Checked clean, so the rule may only fire in codegen or in the C
        # backend; the full build decides. BOTH phases' output is searched.
        local bout
        bout=$(with_limits "$WYN" build "$file" -o "$sandbox/out" 2>&1); rc=$?
        out="$out
$bout"
        rm -f "${file%.wyn}" "${file}.c" 2>/dev/null
    fi
    rm -rf "$sandbox"

    local errs=""
    if [ "$rc" -eq 0 ]; then
        if [ "$check_only" = "1" ]; then
            errs="    EXPECT_CHECK_FAIL: expected 'wyn check' to REJECT this program, but check exited 0. This directive has no build fallback on purpose: a rule that only fires later (codegen or the C backend) does not satisfy it.\n"
        else
            errs="    expected compilation to FAIL, but it succeeded (rc=0)\n"
        fi
    elif [ "$rc" -gt 127 ]; then
        errs="    expected a clean diagnostic, got signal/timeout rc=$rc\n"
    fi
    # The text EXPECT_ERR searches is the compiler's output with (a) ANSI colour
    # codes and (b) its ECHOED SOURCE CONTEXT removed. (b) is load-bearing, not
    # cosmetic: the diagnostic renderer prints a WINDOW of lines around the error
    # ("  10 | // EXPECT_ERR: <pattern>"), so the DIRECTIVE LINE ITSELF lands in
    # the output and every pattern would match itself. Measured on this tree: with
    # the raw output, tests/bdd_selftest/expect_err_must_be_enforced.wyn was
    # satisfied purely by the echo of its own directive and reported PASS — i.e.
    # EXPECT_ERR asserted nothing at all. Echo lines are `<spaces><digits> | ...`
    # plus the bare gutter/caret rules; real diagnostics never start that way.
    # If the renderer's format changes so this strip stops matching, that control
    # reports PASS and the suite fails — which is exactly its job.
    local searchable
    searchable=$(printf '%s' "$out" | sed $'s/\033\[[0-9;]*m//g' \
        | grep -v '^[[:space:]]*[0-9]*[[:space:]]*|')
    while IFS= read -r pat; do
        [ -n "$pat" ] || continue
        if ! printf '%s' "$searchable" | grep -qF -- "$pat"; then
            errs="${errs}    expected error text not found: $pat\n"
        fi
    done <<< "$want_err"

    if [ -z "$errs" ]; then
        echo "PASS" > "$result_file"
    else
        printf "FAIL\n%b    check rc=%s, final rc=%s, actual: %s\n" "$errs" "$check_rc" "$rc" \
            "$(printf '%s' "$out" | tr '\n' ' ' | cut -c1-300)" > "$result_file"
    fi
}

run_test() {
    local file="$1"
    local name=$(basename "$file")
    local result_file="$TMPDIR/$name.result"

    local expected=$(grep '// EXPECT:' "$file" | sed 's|// EXPECT: ||')
    local want_fail want_check_fail want_err check_only
    # The rejection directives are ANCHORED to the start of their comment, unlike
    # the older unanchored `// EXPECT:` above. Reason: these greps match a
    # directive anywhere on a line, so a comment that merely MENTIONS one in prose
    # silently becomes one -- a doc line reading `// see // EXPECT_ERR: ...` turned
    # the rest of that sentence into a required error pattern. Anchoring was
    # measured to change nothing in the existing corpus (0 of the scanned
    # tests/expect + tests/regression files put a directive mid-line).
    # Note `// EXPECT_FAIL:` is not a substring of `// EXPECT_CHECK_FAIL:`, so
    # these two greps do not alias each other either.
    want_fail=$(grep -c '^[[:space:]]*// EXPECT_FAIL:' "$file")
    want_check_fail=$(grep -c '^[[:space:]]*// EXPECT_CHECK_FAIL:' "$file")
    want_err=$(grep '^[[:space:]]*// EXPECT_ERR:' "$file" | sed 's|^.*// EXPECT_ERR:[[:space:]]*||')
    # Expected process exit status, accept mode only. Anchored like the rejection
    # directives, for the same reason (a directive named in prose used to become
    # one). Absent => 0: the assertion is ON by default, so no existing test has
    # to be edited to acquire it and no new test can forget it.
    local want_exit_raw want_exit
    want_exit_raw=$(grep '^[[:space:]]*// EXPECT_EXIT:' "$file" | tail -1 | sed 's|^.*// EXPECT_EXIT:[[:space:]]*||' | tr -d '[:space:]')
    if [ "$want_fail" -gt 0 ] || [ "$want_check_fail" -gt 0 ] || [ -n "$want_err" ]; then
        if [ -n "$expected" ] || [ -n "$want_exit_raw" ]; then
            printf "FAIL\n    directive conflict: // EXPECT: / // EXPECT_EXIT: cannot be combined with // EXPECT_FAIL: / // EXPECT_CHECK_FAIL: / // EXPECT_ERR:\n" \
                > "$result_file"
            return
        fi
        check_only=0
        [ "$want_check_fail" -gt 0 ] && check_only=1
        run_rejection_test "$file" "$result_file" "$want_err" "$check_only"
        return
    fi
    # Validate EXPECT_EXIT before it can silently mean something else. A typo'd
    # or non-numeric value must be a loud FAIL, not a directive that quietly
    # degrades to "0" (or to "any status"), which is how a gate goes vacuous.
    want_exit=0
    if [ -n "$want_exit_raw" ]; then
        case "$want_exit_raw" in
            ''|*[!0-9]*)
                printf "FAIL\n    // EXPECT_EXIT: expects a decimal exit status 0-255, got '%s'\n" "$want_exit_raw" \
                    > "$result_file"
                return ;;
        esac
        if [ "$want_exit_raw" -gt 255 ]; then
            printf "FAIL\n    // EXPECT_EXIT: expects a decimal exit status 0-255, got '%s'\n" "$want_exit_raw" \
                > "$result_file"
            return
        fi
        want_exit="$want_exit_raw"
    fi
    if [ -z "$expected" ]; then
        if [ -n "$want_exit_raw" ]; then
            # EXPECT_EXIT on its own would otherwise fall into the SKIP below and
            # assert nothing at all — exactly the silent-no-op shape this harness
            # has been burned by. Say so instead.
            printf "FAIL\n    // EXPECT_EXIT: requires at least one // EXPECT: line; on its own it would be skipped and assert nothing\n" \
                > "$result_file"
            return
        fi
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
    local run_rc=0
    while [ "$attempt" -lt 3 ]; do
        output=$(with_limits "$bin" 2>&1)
        # Captured on the SAME line as the run. `output=$(...)` above is the last
        # command whose status is still in $?, so this must stay adjacent to it —
        # the filtering pipeline below would otherwise overwrite it with grep's.
        run_rc=$?
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

    # Step 5: the program must also have EXITED as expected. Printing the right
    # text is not the same as running to completion: without this, a test whose
    # program panicked, aborted or was killed by a signal after its last EXPECT
    # line was scored PASS. 128+N means a signal (139 = SIGSEGV, 134 = SIGABRT),
    # which with_limits' `perl ... alarm` also uses for a timeout kill, so it is
    # named separately — "crashed" and "printed a wrong line" are different bugs
    # and the message must not conflate them.
    if [ "$run_rc" -ne "$want_exit" ]; then
        failed=1
        if [ "$run_rc" -ge 128 ]; then
            errs="${errs}    program died on signal $((run_rc - 128)) (rc=$run_rc) after producing its output; expected exit $want_exit\n"
        else
            errs="${errs}    program exit status: expected $want_exit, got $run_rc\n"
        fi
    fi

    if [ "$failed" -eq 0 ]; then
        echo "PASS" > "$result_file"
    else
        printf "FAIL\n%b" "$errs" > "$result_file"
    fi
}

# Collect all test files.
#
# SUBSET FILTER — `WYN_TEST_FILTER` is an EDIT-LOOP tool and never a gate. It is
# a shell case-glob matched against the path, e.g.
#     WYN_TEST_FILTER='tests/regression/*rejected*' bash tests/run_bdd.sh
#
# WHY IT EXISTS: this script had no filter, so the only way to verify ANY change
# to it was a full ~321-program pass at ~3 minutes a time. Verifying one change
# to the runner means a before/after tally plus a red/green cycle per mutation,
# each needing its own pass, and a rebuild between — which put a single-afternoon
# change to this file into the many-hours range. Measured, not supposed.
#
# TWO GUARDS, because a subset runner is a thinning mechanism and this repo has
# been burned by gates that pass because they matched less than they used to:
#   1. A filter that selects NOTHING is a FAILURE, not an empty green run. A
#      typo'd glob must never report "0 pass, 0 fail" and exit 0.
#   2. A filtered run SAYS SO, in the banner and in the final line, so no log of
#      a partial run can ever be read as evidence of a full one. The harness
#      self-test below still runs either way: it is four programs, and it is the
#      thing that keeps the rejection directives non-vacuous.
FILES=()
for f in tests/expect/*.wyn tests/regression/*.wyn; do
    [ -f "$f" ] || continue
    if [ -n "${WYN_TEST_FILTER:-}" ]; then
        case "$f" in
            $WYN_TEST_FILTER) ;;
            *) continue ;;
        esac
    fi
    FILES+=("$f")
done

if [ -n "${WYN_TEST_FILTER:-}" ]; then
    echo "!!! FILTERED RUN — WYN_TEST_FILTER='$WYN_TEST_FILTER' selected ${#FILES[@]} of the corpus."
    echo "!!! This is NOT a full suite run and must not be reported as one."
    if [ "${#FILES[@]}" -eq 0 ]; then
        echo "FAIL: WYN_TEST_FILTER='$WYN_TEST_FILTER' matched no test file. A filter that"
        echo "      selects nothing is a failure, not an empty pass — otherwise a typo'd"
        echo "      glob reports a green suite that ran nothing."
        exit 1
    fi
fi

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
    # SLIDING WINDOW, not batch-drain. The previous version launched max_jobs tests
    # and then `wait`ed for ALL of them before launching any more, because bash 3.2
    # (still the macOS default) has no `wait -n`. The comment claimed "each test is
    # short, so batch-draining keeps utilization high enough" - measured, it does
    # not: per-test wall time ranges 0.5-4.4s, so every batch runs at the speed of
    # its slowest member while the other slots sit idle. Total %CPU across the whole
    # box mid-run was 439% of a possible 1200% on this 12-core machine, i.e. ~8
    # cores idle for most of a 234-second suite.
    #
    # `jobs -pr` (running jobs only) works on bash 3.2, so poll it and launch a
    # replacement as soon as any slot frees. Same max_jobs cap, same fork-storm
    # protection - just no barrier. The 0.05s poll is far below the 0.5s floor of
    # the fastest test, so polling overhead is noise.
    for f in "${FILES[@]}"; do
        while [ "$(jobs -pr | wc -l | tr -d ' ')" -ge "$max_jobs" ]; do
            sleep 0.05
        done
        run_test "$f" &
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

# --- Negative controls for the directives themselves -----------------------
# tests/bdd_selftest/*.wyn are deliberately WRONG: each carries a rejection
# directive its own source does not satisfy. They are NOT in the scanned
# directories above, so they are run here (serially; there are a handful) and
# the expected verdict is "FAIL". If EXPECT_FAIL ever became a no-op, every
# ported rejection test would pass vacuously and only this block would notice.
#
# DISCOVERY FLOOR: this block must never silently do nothing. A bare
# `if ls tests/bdd_selftest/*.wyn` skipped the entire self-test when the glob
# matched nothing, so deleting or renaming the directory restored full vacuity
# with a green suite — the "two empty sets compared equal" failure this repo has
# been burned by. So COUNT what was discovered and fail if the count is short.
# A FLOOR, not an equality: one control per directive/rule is the invariant, and
# adding a directive plus its control must not red the suite. Raise it when you
# add one. Current: EXPECT_FAIL, EXPECT_CHECK_FAIL, EXPECT_ERR, the
# EXPECT:-vs-rejection directive conflict, and the program-exit-status check.
SELFTEST_FLOOR=5
echo ""
echo "=== Harness self-test (negative controls) ==="
selftest_n=$(ls tests/bdd_selftest/*.wyn 2>/dev/null | wc -l | tr -d ' ')
echo "  discovered $selftest_n negative control(s) in tests/bdd_selftest/ (floor: $SELFTEST_FLOOR)"
if [ "$selftest_n" -lt "$SELFTEST_FLOOR" ]; then
    TOTAL=$((TOTAL + 1))
    FAIL=$((FAIL + 1))
    echo "  ✗ discovery floor not met"
    ERRORS="${ERRORS}\n  FAIL: harness self-test discovery: found $selftest_n control(s) in tests/bdd_selftest/, floor is $SELFTEST_FLOOR (one per rejection directive/rule). These controls are the ONLY thing keeping EXPECT_FAIL / EXPECT_CHECK_FAIL / EXPECT_ERR non-vacuous, so a missing one is a suite FAILURE, not a skip.\n"
fi
for f in tests/bdd_selftest/*.wyn; do
    [ -f "$f" ] || continue
    name=$(basename "$f")
    run_test "$f"
    status=$(head -1 "$TMPDIR/$name.result" 2>/dev/null)
    TOTAL=$((TOTAL + 1))
    if [ "$status" = "FAIL" ]; then
        PASS=$((PASS + 1))
        echo "  ✓ $name (directive correctly reported FAIL)"
    else
        FAIL=$((FAIL + 1))
        echo "  ✗ $name"
        ERRORS="${ERRORS}\n  FAIL: harness self-test $name: expected the runner to report FAIL, got '$status' - the directive this file controls is not being enforced\n"
    fi
done

echo ""
if [ -n "${WYN_TEST_FILTER:-}" ]; then
    # The tally line is what every caller greps, including `make test` and CI, so
    # the FILTERED marker goes ON that line rather than near it.
    echo "Results: $PASS pass, $FAIL fail  [FILTERED RUN: WYN_TEST_FILTER='$WYN_TEST_FILTER' — NOT the full corpus]"
else
    echo "Results: $PASS pass, $FAIL fail"
fi
if [ -n "$ERRORS" ]; then
    echo -e "\nFailures:$ERRORS"
fi
rm -rf "$TMPDIR"
exit $FAIL
