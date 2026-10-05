#!/bin/bash
# V-38 (#391) typed HashSet - the DEBUG half.
#
# Runs every `expect` arm in DEBUG mode plus all the MODE-INDEPENDENT arms (`reject`,
# `check_only`, the type-printer block), which only ever call `wyn check` and so would be
# identical work in the release half. 88 assertions.
#
# The arms themselves live in typed_set_arms.bash, sourced by this script and by
# run_typed_set_release_test.sh. One list, two drivers - see that file's header for why.
# The two halves share no state: each `mktemp -d`s its own sandbox and owns its own alarm
# budget, so `make test` can run them concurrently.
#
# Deliberately does NOT cd: `make test` invokes this as `WYN=./wyn bash tests/errors/...`
# from the repo root and the arms file resolves that relative WYN against the cwd.
set -uo pipefail
export TS_MODE=debug
export TS_CHECK_ARMS=1
export TS_RUN_ALARM=90
export TS_CHK_ALARM=20
# shellcheck source=tests/errors/typed_set_arms.bash
. "$(dirname "$0")/typed_set_arms.bash"
# FLOOR = every arm in the file, because TS_CHECK_ARMS=1 runs all of them:
#   50 `expect` + 31 `reject` + 2 `check_only` + 5 in the type-printer block = 88.
# Re-derive it, do not trust this comment:
#   grep -cE '^expect '  tests/errors/typed_set_arms.bash
#   grep -cE '^reject '  tests/errors/typed_set_arms.bash
#   grep -cE '^check_only ' tests/errors/typed_set_arms.bash   # + 4 loop pairs + 1 open set
# It is a FLOOR, so adding an arm needs no edit here; see `verdict` in the arms file.
verdict debug 88
