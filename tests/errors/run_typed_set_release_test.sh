#!/bin/bash
# V-38 (#391) typed HashSet - the --release half.
#
# Runs every `expect` arm under `wyn run --release FILE` (flags BEFORE the path; the other
# order silently compiles non-release and hands `--release` to the program, which would
# make this half a second debug run). That is the half that matters for the slim runtime
# header: `--release` emits wyn_runtime_slim.h and links libwyn_rt.a, so a runtime
# function declared in only one header breaks release alone. 50 assertions.
#
# Skips the mode-independent arms (TS_CHECK_ARMS=0): they are `wyn check` only, `wyn check`
# has no --release, and the debug half runs them.
#
# The arms themselves live in typed_set_arms.bash, sourced by this script and by
# run_typed_set_debug_test.sh. One list, two drivers - see that file's header for why.
# The two halves share no state: each `mktemp -d`s its own sandbox and owns its own alarm
# budget, so `make test` can run them concurrently.
#
# Deliberately does NOT cd: `make test` invokes this as `WYN=./wyn bash tests/errors/...`
# from the repo root and the arms file resolves that relative WYN against the cwd.
set -uo pipefail
# shellcheck source=tests/errors/split_gate_lib.bash
. "$(dirname "$0")/split_gate_lib.bash"
# FLOOR = the 50 `expect` arms, the only ones this half runs (TS_CHECK_ARMS=0 skips the 38
# `wyn check`-only arms, which the debug half owns):
#   grep -cE '^expect ' tests/errors/typed_set_arms.bash
# 88 + 50 = 138, the tally the single pre-split script printed.
# It is a FLOOR, so adding an arm needs no edit here; see `gate_verdict` in
# split_gate_lib.bash. gate_begin must precede the arms: it installs the EXIT trap that
# catches an arms file that exits before the verdict is reached.
gate_begin "typed-set[release]" 50
export TS_MODE=release
export TS_CHECK_ARMS=0
export TS_RUN_ALARM=90
export TS_CHK_ALARM=20
# shellcheck source=tests/errors/typed_set_arms.bash
. "$(dirname "$0")/typed_set_arms.bash"
gate_verdict
