#!/bin/bash
# The Option/Result COMBINATOR API (#392) - the DEBUG half.
#
# Runs every `both` and `expect_panic` arm in DEBUG mode (`wyn build FILE`, no flag) plus
# all 7 MODE-INDEPENDENT `reject` arms, which only ever call `wyn check` and so would be
# identical work in the release half. 70 assertions.
#
# The arms themselves live in option_combinator_api_arms.bash, sourced by this script and
# by run_option_combinator_api_release_test.sh. One list, two drivers - see that file's
# header for why (it was the slowest gate in the project, 11-15% of `make test` alone).
# The two halves share no state: each gets its own `gate_tmpdir` sandbox and its own alarm
# budgets, so `make test` can run them concurrently.
#
# Deliberately does NOT cd: `make test` invokes this as `WYN=./wyn bash tests/errors/...`
# from the repo root and the arms file resolves that relative WYN against the cwd.
set -uo pipefail
# shellcheck source=tests/errors/split_gate_lib.bash
. "$(dirname "$0")/split_gate_lib.bash"

# FLOOR, not an equality: 60 `both` + 3 `expect_panic` + 7 `reject` = 70 arms run here; the
# release half runs the other 64, and 70 + 64 = 134 is what the single script asserted.
# Re-derive the parts, do not trust this comment:
#   grep -cE '^both "'          tests/errors/option_combinator_api_arms.bash
#   grep -cE '^expect_panic "'  tests/errors/option_combinator_api_arms.bash
#   grep -cE '^reject "'        tests/errors/option_combinator_api_arms.bash
# Adding an arm needs no edit here; see `gate_verdict` in split_gate_lib.bash for what the
# floor does and does not cover. gate_begin must come BEFORE the arms are sourced: it
# installs the EXIT trap that catches an arms file exiting before the verdict is reached.
gate_begin "option-combinator-api[debug]" 70
export OCA_MODE=debug
export OCA_CHECK_ARMS=1
export OCA_SLIM_ARM=0
export OCA_BUILD_ALARM=180
export OCA_EXEC_ALARM=60
export OCA_CHK_ALARM=120
# shellcheck source=tests/errors/option_combinator_api_arms.bash
. "$(dirname "$0")/option_combinator_api_arms.bash"
gate_verdict
