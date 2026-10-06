#!/bin/bash
# THE SPLIT-GATE HARNESS: counters, the per-half tally, and the FLOOR.
#
# THIS FILE IS SOURCED, NOT RUN. It deliberately does not end in `.sh` and is not
# executable, so a roster derived from `tests/**/*.sh` cannot mistake it for a gate.
#
# WHAT A "SPLIT GATE" IS. One arm list in an `*_arms.bash` file, sourced by TWO driver
# scripts that `make test` can run concurrently - typically a debug half and a `--release`
# half. Copying the arms into two scripts would make two lists of the same thing that have
# to agree, which is a defect shape this repo keeps paying for; so the arms live once and
# the drivers set switches.
#
# WHY THE FLOOR LIVES HERE AND NOT IN EACH DRIVER. Splitting introduces a failure mode the
# single script did not have: a half can SKIP arms - a skip switch left on, a dropped
# section, a helper that returns early - and still print "0 pass, 0 fail" and exit 0. A
# fully green gate that proved nothing. That is not hypothetical in this tree:
# .github/workflows/ci.yml carries a comment about tests/run_tests_parallel.sh, which sat
# in CI reporting exactly "0 pass, 0 fail" with exit 0 on every run because its test list
# was missing. The medicine is to REFUSE rather than report a vacuous success - and to have
# ONE implementation of that refusal, because a second copy of a floor check is the same
# two-lists defect one level up.
#
# USE, from a driver:
#
#     set -uo pipefail
#     . "$(dirname "$0")/split_gate_lib.bash"
#     gate_begin "typed-set[debug]" 88        # BEFORE the arms: installs the EXIT trap
#     export TS_MODE=debug TS_CHECK_ARMS=1    # the half's switches
#     . "$(dirname "$0")/typed_set_arms.bash"
#     gate_verdict                            # last command of the driver
#
# and from an arms file: `TMP=$(gate_tmpdir)` for its sandbox, `section`/`ok`/`bad` to
# report. The arms file sets no trap and owns no counters.
#
# THE FLOOR IS A FLOOR, NEVER AN EQUALITY. `-lt`, so thinning the list reds while correctly
# ADDING an arm stays green and needs no edit to the number. It counts arms that REPORTED
# (PASS+FAIL), not PASS: on PASS alone a single legitimately-failing arm would trip the
# floor too and blame thinning for a plain regression. The FAIL check already owns that
# case, and names the arm.

GATE_LABEL="(unnamed gate)"
GATE_FLOOR=0
GATE_BEGUN=0
GATE_VERDICT_RAN=0
PASS=0
FAIL=0

# gate_begin <label> <floor>   -- call BEFORE sourcing the arms file.
#
# Installing the EXIT trap here, ahead of the arms, is the whole point of the ordering: an
# `exit` ANYWHERE inside a sourced arms file terminates the driver on the spot, so a check
# written after the `.` source line - which is where it naturally goes - never runs. An
# `exit 0` mid-arms would then be a silent green. The trap is the only hook that still
# fires, which is why the completion check lives in it and not next to `gate_verdict`.
gate_begin(){
  GATE_LABEL=$1
  GATE_FLOOR=$2
  GATE_BEGUN=1
  GATE_VERDICT_RAN=0
  # The sandbox ROOT is created here, in the PARENT shell, because gate_tmpdir
  # cannot create it: callers write `d=$(gate_tmpdir)`, so that function's body
  # runs in a command-substitution SUBSHELL and every variable it assigns is
  # discarded when the subshell exits. An earlier version registered each
  # sandbox from inside gate_tmpdir, so GATE_TMPDIRS was always empty in the
  # trap and NO split gate ever cleaned up after itself. One leak per gate per
  # run is how a /tmp fills up quietly.
  GATE_TMPROOT=$(mktemp -d) || exit 2
  trap gate_on_exit EXIT
}

# gate_tmpdir  -- a sandbox for the caller, cleaned up by the EXIT trap.
#
# The arms file must NOT set its own `trap ... EXIT`: that would replace the completion
# trap and reopen the early-`exit` hole above. Refusing when gate_begin has not run keeps
# that mistake from being silent.
gate_tmpdir(){
  if [ "$GATE_BEGUN" != 1 ]; then
    echo "split_gate_lib: gate_begin must be called before the arms file is sourced" >&2
    exit 2
  fi
  # Carve the sandbox out of the root gate_begin made. Nothing is registered
  # here on purpose: this body is a subshell (see gate_begin), so a registration
  # would be silently lost. Cleanup removes the root, which the parent knows,
  # and that works for any number of calls.
  d=$(mktemp -d "$GATE_TMPROOT/sandbox.XXXXXX") || exit 2
  printf '%s\n' "$d"
}

# gate_on_exit  -- EXIT trap. Cleans up, then converts "exited 0 without a verdict" into a
# failure. A status that is ALREADY non-zero is preserved rather than flattened to 1, so
# `exit 2` from an arms-file argument check still reads as 2.
gate_on_exit(){
  status=$?
  [ -n "$GATE_TMPROOT" ] && rm -rf "$GATE_TMPROOT"
  if [ "$GATE_VERDICT_RAN" != 1 ]; then
    echo "" >&2
    echo "  FAIL  $GATE_LABEL exited with status $status WITHOUT reaching gate_verdict." >&2
    echo "        $PASS pass, $FAIL fail so far - a partial run, not a result. An \`exit\`" >&2
    echo "        inside the sourced arms file skips the driver's remaining lines, so the" >&2
    echo "        tally and the floor were never checked." >&2
    [ "$status" -ne 0 ] && exit "$status"
    exit 1
  fi
  exit "$status"
}

# A section header prints only when an arm under it actually runs, so a half that skips the
# mode-independent arms does not emit bare headings.
GATE_PENDING=""
section(){ GATE_PENDING="$1"; }
flush(){ if [ -n "$GATE_PENDING" ]; then echo "$GATE_PENDING"; GATE_PENDING=""; fi; }
ok(){ flush; echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ flush; echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# gate_verdict  -- the gate. The driver's LAST command; its status is the gate's status.
# TWO assertions, because `FAIL -eq 0` alone is not a gate: it says nothing about how many
# arms ran.
gate_verdict(){
  GATE_VERDICT_RAN=1
  ran=$((PASS + FAIL))
  echo ""
  echo "$GATE_LABEL: $PASS pass, $FAIL fail ($ran assertions ran, floor $GATE_FLOOR)"
  if [ "$ran" -lt "$GATE_FLOOR" ]; then
    echo "  FAIL  $GATE_LABEL ran only $ran assertions, below its floor of $GATE_FLOOR." >&2
    echo "        The arm list was thinned - a skip switch left on, an early return in a" >&2
    echo "        helper, or a dropped section. Restore the arms; only lower the floor" >&2
    echo "        deliberately." >&2
    return 1
  fi
  [ "$FAIL" -eq 0 ]
}
