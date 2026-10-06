#!/bin/bash
# The Option/Result COMBINATOR API (#392) - the --release half.
#
# Runs every `both` and `expect_panic` arm under `wyn build --release FILE` (flags BEFORE
# the path; the other order silently compiles non-release and hands `--release` to the
# program, which would make this half a second debug run), plus the one slim-header arm,
# which uses `wyn run --release FILE`. 64 assertions.
#
# WHY THIS HALF EXISTS AT ALL - it is not a duplicate of the debug half. `wyn build
# --release` optimises and re-emits, so a lowering that is only wrong under -O2 shows up
# here; and `wyn run --release` is the ONLY command that compiles wyn_runtime_slim.h, the
# hand-maintained declarations-only header, so a combinator naming something absent from it
# is green in debug and red here. That is why the release half is a half and not a deletion.
#
# Skips the 7 mode-independent `reject` arms (OCA_CHECK_ARMS=0): they are `wyn check` only,
# `wyn check` has no --release, and the debug half runs them.
#
# THIS HALF IS THE EXPENSIVE ONE, AND IT IS LESS LOPSIDED THAN THE BUILD COST SUGGESTS.
# Measured per arm on this gate's programs, 6 timings each, first discarded (macos-arm64,
# 2026-10-05, on a CONTENDED box - treat the absolute numbers as upper bounds and re-time
# before quoting them anywhere):
#
#   wyn build            ~0.37s      wyn build --release   ~1.45s     (~3.9x)
#   first run of the freshly built binary   ~1.2-1.6s   IN BOTH MODES
#   second run of the same binary           ~0.05s
#
# That middle row is the one that decides the shape of the split: every arm builds a NEW
# binary and runs it exactly ONCE, so every arm pays the macOS first-execution scan, and
# that cost is identical in both halves. Per arm the real totals are therefore ~1.7s debug
# and ~2.8s release - this half is dearer by about 1.6x, not by the 3.9x the build ratio on
# its own implies. A parallel runner pays the MAX of the two halves, so that 1.6x is what
# caps the win: measured 262s for the single script against 125s / 142s for the two halves
# run concurrently, i.e. ~1.8x, not the 2x an even split would give.
#
# Two consequences worth recording. (1) Balancing further means splitting the release arms
# again; the natural cut is by receiver family, Option arms vs Result arms, ~30 each, which
# the arm list is already grouped for. Deliberately NOT done here: one concern per PR.
# (2) The per-arm fixed cost, not the build mode, is the bigger lever on this gate - no mode
# split can remove ~1.3s x 126 arm-executions. Consolidating several assertions into ONE
# program would, and that is a change to the arms, not to the drivers.
#
# The arms themselves live in option_combinator_api_arms.bash, sourced by this script and
# by run_option_combinator_api_debug_test.sh. One list, two drivers - see that file's
# header for why. The two halves share no state: each gets its own `gate_tmpdir` sandbox and
# its own alarm budgets, so `make test` can run them concurrently.
#
# Deliberately does NOT cd: `make test` invokes this as `WYN=./wyn bash tests/errors/...`
# from the repo root and the arms file resolves that relative WYN against the cwd.
set -uo pipefail
# shellcheck source=tests/errors/split_gate_lib.bash
. "$(dirname "$0")/split_gate_lib.bash"

# FLOOR, not an equality: 60 `both` + 3 `expect_panic` + 1 slim-header arm = 64 arms run
# here; the debug half runs the other 70, and 70 + 64 = 134 is what the single script
# asserted. Re-derive the parts, do not trust this comment:
#   grep -cE '^both "'          tests/errors/option_combinator_api_arms.bash
#   grep -cE '^expect_panic "'  tests/errors/option_combinator_api_arms.bash
# plus the single arm guarded by OCA_SLIM_ARM. Adding an arm needs no edit here; see
# `gate_verdict` in split_gate_lib.bash for what the floor does and does not cover. In
# particular this floor is what reds if OCA_SLIM_ARM is ever left at 0 here, which would
# otherwise drop the only assertion in the whole suite that compiles the slim header.
# gate_begin must come BEFORE the arms are sourced: it installs the EXIT trap that catches
# an arms file exiting before the verdict is reached.
gate_begin "option-combinator-api[release]" 64
export OCA_MODE=release
export OCA_CHECK_ARMS=0
export OCA_SLIM_ARM=1
export OCA_BUILD_ALARM=180
export OCA_EXEC_ALARM=60
export OCA_SLIM_ALARM=240
# shellcheck source=tests/errors/option_combinator_api_arms.bash
. "$(dirname "$0")/option_combinator_api_arms.bash"
gate_verdict
