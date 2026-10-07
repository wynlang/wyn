#!/bin/sh
# wyn-container.sh — the persistent Linux build/test container for Wyn.
#
# WHY THIS EXISTS, in one measurement: on this macOS host CrowdStrike Falcon does a
# cloud hash lookup before every new executable is allowed to run. The first run of a
# novel binary cost 14,094 ms natively against 0.8 ms in this container, and a warm
# re-run still cost 96.7 ms native vs 0.8 ms. The Wyn suite builds and runs thousands
# of novel binaries, so the suite's wall clock was mostly Falcon, not the compiler.
# Measured: run_bdd.sh is ~62s here against ~240s natively on a GOOD Falcon day.
# The full measurement, with its caveats, is recorded in the development notes
# (tracked separately — this repository must not name private document paths).
#
# DESIGN, and the one rule that matters:
#   The workspace is mounted READ-ONLY at /src, and the container builds in its own
#   clone at /build/wyn. This is deliberate. The Makefile writes `wyn` at the repo
#   root and runtime/libwyn_rt.a beside it; building a Linux ELF through a writable
#   mount would overwrite the host's macOS binary and every subsequent native command
#   would silently run the wrong architecture. The project already has a recorded
#   incident of exactly that shape (the playground build clobbering repos/wyn/wyn).
#
# Usage:
#   ./wyn-container.sh up                 start it (idempotent; survives reboots)
#   ./wyn-container.sh sync [<branch>]    pull the host's current state into the clone
#   ./wyn-container.sh build              make, from scratch
#   ./wyn-container.sh bdd [<filter>]     run_bdd.sh, optionally WYN_TEST_FILTER
#   ./wyn-container.sh test               the full make test roster
#   ./wyn-container.sh sh  [<cmd>...]     anything else, inside /build/wyn
#   ./wyn-container.sh status             is it up, what commit, is it dirty
set -e

NAME=wyn-build
IMAGE=gcc:13-bookworm          # aarch64, and already local. gcc/cc/make/perl/python3/git/pkg-config
# Derived from this script's own location (scripts/ -> repos/wyn -> repos -> workspace),
# never hardcoded: a hardcoded path is why the measurement harness sat outside git and
# could not be reviewed or run in CI. Override with WYN_WS= if the layout differs.
# realpath first: this script is reached through a symlink at the workspace root, and
# `dirname "$0"` on a symlink gives the LINK's directory, which would mount two levels
# too high. Caught by invoking it both ways rather than by reading it.
SELF=$(python3 -c 'import os,sys;print(os.path.realpath(sys.argv[1]))' "$0")
HOST_WS=${WYN_WS:-$(cd "$(dirname "$SELF")/../../.." && pwd)}
CLONE=/build/wyn

inside() { docker exec -e TMPDIR=/tmp/wb -e WYN_ROOT="$CLONE" -w "$CLONE" "$NAME" bash -lc "$1"; }

case "${1:-status}" in
up)
    if docker ps --filter "name=^${NAME}$" --format '{{.Names}}' | grep -q .; then
        echo "already up"
    else
        docker rm -f "$NAME" >/dev/null 2>&1 || true
        # --restart unless-stopped so it comes back after a Docker or machine restart;
        # keeping it warm matters because the clone and the built artifacts persist.
        # --init is REQUIRED, not hygiene. Without an init as PID 1 nothing reaps
        # orphans, so a child SIGKILLed via PR_SET_PDEATHSIG lingers as a zombie and
        # `pgrep` still finds it: tests/errors/run_orphan_child_test.sh went
        # "4 pass, 3 fail" with `sleep infinity` as PID 1 and "7 pass, 0 fail" with
        # docker-init, on the same commit. That looked exactly like a Linux defect in
        # the compiler and was a defect in the container.
        docker run -d --name "$NAME" --init --platform linux/arm64 \
            -v "$HOST_WS":/src:ro -w /build --restart unless-stopped \
            "$IMAGE" sleep infinity >/dev/null
        docker exec "$NAME" bash -lc "mkdir -p /tmp/wb && git clone -q /src/repos/wyn $CLONE"
        echo "started, cloned"
    fi
    ;;
sync)
    # /src is read-only, which is fine: fetching READS the host repo and writes only
    # into the container's own clone.
    #
    # THE RESET IS LOAD-BEARING. `checkout -B` alone does NOT discard local
    # modifications to tracked files - it carries compatible ones forward - so a
    # clone that had been edited in place stayed edited across a sync while
    # reporting the new commit. That silently invalidated a mutation test: a nonce
    # check deleted by hand survived the sync, and the gate run afterwards was
    # reported as "green with the real fix" when it was green with the mutation
    # still applied. The comment here used to claim `--hard` while the command did
    # not do it. Untracked build output (wyn, runtime/) is deliberately kept - it is
    # not what drifts, and cleaning it would force a full rebuild on every sync.
    B="${2:-dev}"
    inside "git fetch -q /src/repos/wyn '$B' && git checkout -q -f -B '$B' FETCH_HEAD && git reset -q --hard FETCH_HEAD; if git status --porcelain -uno | grep -q .; then echo 'WARNING: clone still dirty after sync' >&2; fi; git log --oneline -1"
    ;;
# EVERY ONE OF THESE PIPES INTO `tail`, SO WITHOUT pipefail THE EXIT STATUS IS
# TAIL'S AND IS ALWAYS 0. That is not hypothetical: `test` reported exit 0 for a run
# whose log ended in `make: *** [Makefile:678: test] Error 1`, and the only reason it
# was noticed is that the failure happened to land inside the last 20 lines. A gate
# that cannot report failure through its exit status is the same defect suite.sh had
# three of. `build` was worse - it ended in `; true`, so it could never fail at all.
build)   inside "set -o pipefail; rm -f wyn && make 2>&1 | tail -3" ;;
bdd)     if [ -n "${2:-}" ]; then inside "set -o pipefail; WYN_TEST_FILTER='$2' bash tests/run_bdd.sh | tail -3"
         else inside "set -o pipefail; bash tests/run_bdd.sh | tail -3"; fi ;;
test)    inside "set -o pipefail; make test 2>&1 | tail -20" ;;
sh)      shift; inside "$*" ;;
status)
    docker ps -a --filter "name=^${NAME}$" --format '  container: {{.Names}} {{.Status}} ({{.Image}})'
    inside "echo -n '  clone:     '; git log --oneline -1; echo -n '  dirty:     '; git status --short | wc -l; echo -n '  binary:    '; (file wyn 2>/dev/null | cut -d, -f1-2) || echo 'not built'" 2>/dev/null || echo "  (not running — ./wyn-container.sh up)"
    ;;
*) echo "usage: $0 {up|sync [branch]|build|bdd [filter]|test|sh <cmd>|status}" >&2; exit 2 ;;
esac
