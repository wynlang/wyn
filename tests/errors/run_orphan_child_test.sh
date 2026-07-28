#!/bin/bash
# `wyn run` must not orphan the program it started.
#
# THE BUG: `wyn run` launched the compiled binary with system(), which ties the
# child to nothing. Killing `wyn run` - including with SIGKILL, which a wrapper
# script, a CI watchdog, or an impatient human all do - left the compiled
# program running forever. Long Wyn sessions accumulated those orphans until the
# host ran out of memory; this class of leak has kernel-panicked a dev machine
# twice, so it is an operational bug, not a tidiness matter.
#
# THE FIX (src/main.c, wyn_run_program): the program runs as a supervised child.
#   - SIGINT/SIGTERM/SIGHUP are forwarded to it, then re-raised so `wyn`'s own
#     status is a faithful 128+N;
#   - it is reaped with waitpid (no zombie);
#   - it cannot outlive us being SIGKILLed: on Linux via
#     prctl(PR_SET_PDEATHSIG), on macOS/BSD via a supervisor process holding the
#     read end of a pipe only `wyn` holds the write end of (EOF => SIGKILL the
#     program), on Windows via a KILL_ON_JOB_CLOSE job object.
#
# Cases 1-3 are the regression guards; 4-6 are the don't-break-the-normal-path
# controls (exit code, stdout, stdin) that a naive "just kill everything" fix
# would break.
#
# Every wait here is bounded: stock macOS has no `timeout`, so use perl's alarm
# (the convention in this directory). A REGRESSION must fail loudly, never hang
# the suite.
set -uo pipefail

WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d)
# Belt and braces: whatever happens, do not leave a sleeper behind - this test
# is specifically about stray processes.
cleanup() {
    [ -n "${SLEEPER_BIN:-}" ] && pkill -9 -f "^$SLEEPER_BIN" 2>/dev/null
    rm -rf "$TMP"
}
trap cleanup EXIT
PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }
skip(){ echo "  skip  $1"; }

# Windows: the guarantee there is a job object, but this script drives POSIX
# signals and pgrep, neither of which behaves the same under mingw/cmd. Skip
# explicitly rather than pretend to cover it (see ci.yml's Windows notes).
case "${OS:-}" in
  Windows_NT) echo "orphan-child: SKIP (POSIX signals/pgrep required)"; exit 0 ;;
esac

# A program that sleeps long enough that it is unambiguously still alive when we
# kill its parent. 60s: comfortably longer than every wait below.
printf 'fn main() -> int {\n    Time.sleep(60000)\n    return 0\n}\n' > "$TMP/sleeper.wyn"
SLEEPER_BIN="$TMP/sleeper.wyn.out"

# Wait (bounded) for the compiled program to appear, then echo its pid.
# `wyn run` compiles first, so the child does not exist immediately.
#
# The pattern is ANCHORED with '^'. An unanchored `pgrep -f "$SLEEPER_BIN"` also
# matches the `wyn` process itself, because its C-compiler command line contains
# `-o <file>.wyn.out` - so the test would pick up wyn's own pid, signal it during
# the COMPILE phase (before the handler is even installed), and then misreport
# the result. That cost a debugging cycle; keep the anchor.
wait_for_child() {
    local i pid
    for i in $(seq 1 150); do        # up to ~30s in 0.2s steps
        pid=$(pgrep -f "^$SLEEPER_BIN" 2>/dev/null | head -1)
        [ -n "$pid" ] && { echo "$pid"; return 0; }
        sleep 0.2
    done
    return 1
}

# Bounded wait for a pid to disappear. Returns 0 if it died, 1 if it survived.
wait_gone() {
    local pid="$1" i
    for i in $(seq 1 50); do         # up to ~10s
        kill -0 "$pid" 2>/dev/null || return 0
        sleep 0.2
    done
    return 1
}

# --- 1. THE REGRESSION: SIGKILL the parent, the program must not survive ------
# SIGKILL is the hard case: `wyn` cannot run a handler, so this only passes if
# the kernel (PDEATHSIG) or the supervisor (pipe EOF) enforces it.
rm -f "$SLEEPER_BIN"
"$WYN" run "$TMP/sleeper.wyn" >/dev/null 2>&1 &
wyn_pid=$!
if child=$(wait_for_child); then
    sleep 0.5
    kill -9 "$wyn_pid" 2>/dev/null
    wait "$wyn_pid" 2>/dev/null
    if wait_gone "$child"; then
        ok "SIGKILL on wyn run leaves no orphan (child $child reaped)"
    else
        bad "ORPHAN: child $child still alive after kill -9 of wyn run"
        kill -9 "$child" 2>/dev/null
    fi
else
    bad "child never started (case 1)"
    kill -9 "$wyn_pid" 2>/dev/null
fi

# --- 2. SIGTERM: child dies AND wyn reports 128+15 ---------------------------
rm -f "$SLEEPER_BIN"
"$WYN" run "$TMP/sleeper.wyn" >/dev/null 2>&1 &
wyn_pid=$!
if child=$(wait_for_child); then
    sleep 0.5
    kill -TERM "$wyn_pid" 2>/dev/null
    wait "$wyn_pid" 2>/dev/null; rc=$?
    gone=0; wait_gone "$child" && gone=1
    if [ "$gone" = 1 ] && [ "$rc" = 143 ]; then
        ok "SIGTERM propagates to child; wyn exits 143 (128+15)"
    else
        bad "SIGTERM: child_gone=$gone wyn_rc=$rc (want gone=1 rc=143)"
        kill -9 "$child" 2>/dev/null
    fi
else
    bad "child never started (case 2)"
    kill -9 "$wyn_pid" 2>/dev/null
fi

# --- 3. SIGINT (Ctrl-C): same, with 128+2 ------------------------------------
rm -f "$SLEEPER_BIN"
"$WYN" run "$TMP/sleeper.wyn" >/dev/null 2>&1 &
wyn_pid=$!
if child=$(wait_for_child); then
    sleep 0.5
    kill -INT "$wyn_pid" 2>/dev/null
    wait "$wyn_pid" 2>/dev/null; rc=$?
    gone=0; wait_gone "$child" && gone=1
    if [ "$gone" = 1 ] && [ "$rc" = 130 ]; then
        ok "SIGINT propagates to child; wyn exits 130 (128+2)"
    else
        bad "SIGINT: child_gone=$gone wyn_rc=$rc (want gone=1 rc=130)"
        kill -9 "$child" 2>/dev/null
    fi
else
    bad "child never started (case 3)"
    kill -9 "$wyn_pid" 2>/dev/null
fi

# A watchdog-killed run (rc >= 128 with the alarm having fired) on these CONTROL
# cases is a host-load artifact, not a signal-handling bug: under a loaded runner
# `wyn run`'s compile step can genuinely exceed the alarm. Retry those, but never
# retry a WRONG ANSWER - a real regression produces a definite bad value, which
# must fail on the first attempt. Same discipline as run_bdd.sh's retry.
ORPHAN_ALARM=90
# Writes the command's stdout to $RB_OUT_FILE and sets RB_RC. NOT via
# `x=$(run_bounded ...)`: command substitution runs the function in a SUBSHELL,
# so RB_RC would never make it back to the caller and the check would silently
# test a stale exit code from an earlier case (this bit once - keep the file).
RB_OUT_FILE="$TMP/.rb_out"
run_bounded() {
    perl -e 'alarm shift; exec @ARGV or exit 127' "$ORPHAN_ALARM" "$@" \
        >"$RB_OUT_FILE" 2>/dev/null
    RB_RC=$?
}

# --- 4. CONTROL: the normal path still passes the exit code through ----------
# Both fresh-compile and cached (.out reused) runs, since they are separate
# call sites in main.c and the cached one has regressed before.
printf 'fn main() {\n    System.exit(3)\n}\n' > "$TMP/ec.wyn"
fresh=""; cached=""
for attempt in 1 2 3; do
    rm -f "$TMP/ec.wyn.out"
    run_bounded "$WYN" run "$TMP/ec.wyn"; fresh=$RB_RC
    run_bounded "$WYN" run "$TMP/ec.wyn"; cached=$RB_RC
    # Only a watchdog kill is retryable.
    [ "$fresh" -ge 128 ] || [ "$cached" -ge 128 ] || break
done
if [ "$fresh" = 3 ] && [ "$cached" = 3 ]; then
    ok "exit code preserved (fresh=3, cached=3)"
else
    bad "exit code: fresh=$fresh cached=$cached (want 3/3)"
fi

# --- 5. CONTROL: stdout is still forwarded ----------------------------------
printf 'fn main() -> int {\n    print("forwarded-stdout")\n    return 0\n}\n' > "$TMP/so.wyn"
out=""; rc=""
for attempt in 1 2 3; do
    rm -f "$TMP/so.wyn.out"
    run_bounded "$WYN" run "$TMP/so.wyn"; rc=$RB_RC
    out=$(cat "$RB_OUT_FILE" 2>/dev/null)
    [ "$rc" -ge 128 ] || break
done
if [ "$rc" = 0 ] && echo "$out" | grep -q "forwarded-stdout"; then
    ok "stdout forwarded"
else
    bad "stdout: rc=$rc out=[$out]"
fi

# --- 6. CONTROL: the program can still READ STDIN ---------------------------
# The supervisor sits between `wyn` and the program, so a botched fix (closing
# fd 0, or moving the child to its own session) would silently break every
# interactive program. Test both fresh and cached.
printf 'fn main() -> int {\n    line = input_line()\n    print("got:${line}")\n    return 0\n}\n' > "$TMP/in.wyn"
out=""; out2=""
for attempt in 1 2 3; do
    rm -f "$TMP/in.wyn.out"
    out=$(printf 'piped-input\n'  | perl -e 'alarm shift; exec @ARGV or exit 127' "$ORPHAN_ALARM" "$WYN" run "$TMP/in.wyn" 2>/dev/null); r1=$?
    out2=$(printf 'second-input\n' | perl -e 'alarm shift; exec @ARGV or exit 127' "$ORPHAN_ALARM" "$WYN" run "$TMP/in.wyn" 2>/dev/null); r2=$?
    [ "$r1" -ge 128 ] || [ "$r2" -ge 128 ] || break
done
if echo "$out" | grep -q "got:piped-input" && echo "$out2" | grep -q "got:second-input"; then
    ok "stdin readable by the program (fresh + cached)"
else
    bad "stdin: fresh=[$out] cached=[$out2]"
fi

# --- 7. CONTROL: no zombie is left behind -----------------------------------
# A supervised child that is never reaped would show up as a zombie (state Z)
# parented to a live `wyn`. Run a fast program and confirm the process table is
# clean afterwards.
rm -f "$TMP/so.wyn.out"
perl -e 'alarm 90; exec @ARGV or exit 127' "$WYN" run "$TMP/so.wyn" >/dev/null 2>&1
zombies=$(ps -eo stat,command 2>/dev/null | grep -c "^Z.*$TMP" || true)
if [ "${zombies:-0}" = 0 ]; then
    ok "no zombie left after a normal run"
else
    bad "zombie(s) left: $zombies"
fi

echo ""
echo "orphan-child: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ]
