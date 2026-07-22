#!/bin/bash
# wyn ui coverage: the TUI's command table (src/cmd_ui.c) must cover every
# command main.c dispatches. Extracts `strcmp(command, "...")` names from
# src/main.c and diffs against `wyn ui --list-commands`. Adding a new CLI
# command without a TUI table row fails this test.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# Dispatch names from main.c ("--flags" excluded: they are spellings, not commands).
dispatched=$(grep -oE 'strcmp\(command, "[a-z][a-z-]*"\)' "$ROOT/src/main.c" \
             | sed 's/.*"\([^"]*\)".*/\1/' | sort -u)
listed=$("$WYN" ui --list-commands | sort -u)

missing=$(comm -23 <(echo "$dispatched") <(echo "$listed"))
if [ -z "$missing" ]; then
    ok "TUI table covers every dispatched command"
else
    bad "commands missing from src/cmd_ui.c table: $(echo $missing)"
fi

# Reverse direction: the table must not advertise commands main.c won't accept.
stale=$(comm -13 <(echo "$dispatched") <(echo "$listed"))
if [ -z "$stale" ]; then
    ok "TUI table has no stale commands"
else
    bad "TUI table lists commands main.c does not dispatch: $(echo $stale)"
fi

# The alias must dispatch too.
"$WYN" tui --list-commands >/dev/null 2>&1 && ok "wyn tui alias dispatches" \
                                            || bad "wyn tui alias broken"

# ui must be non-interactive-safe: with stdin a pipe it must fail fast, not hang.
out=$(echo "" | "$WYN" ui 2>&1); rc=$?
if [ $rc -ne 0 ]; then ok "wyn ui without a TTY exits nonzero"; else bad "wyn ui without TTY: rc=$rc [$out]"; fi

echo "ui coverage: $PASS pass, $FAIL fail"
[ $FAIL -eq 0 ]
