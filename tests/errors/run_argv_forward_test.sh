#!/bin/bash
# `wyn run prog.wyn <args>` must forward the arguments to the program.
#
# It dropped them silently: the arg loop consumed everything as candidate FILE
# names and nothing forwarded them, so System.args() had length 1. Only the
# explicit `wyn run prog.wyn -- a b` form worked, and even that LEAKED the `--`
# itself into args[1].
#
# This is not a nicety. It made `wyn run` unusable for the one thing a CLI tool
# does, and the sample apps show the damage: logwatch - "Log File Analyzer" -
# cannot open a log file, portscanner has 11 hardcoded ports, envdiff has 27
# hardcoded variable names. They are not lazy; there was no way to pass an
# argument.
#
# THE RULE, and why it is positional rather than shape-based: wyn's own flags come
# BEFORE the file, and everything after the file belongs to the program. A
# program's own `--verbose` is indistinguishable from one of ours, so the split
# cannot be "does it start with --". Once the file is seen we stop interpreting.
#
# CACHING GOTCHA that cost me a wrong diagnosis: `wyn run` reuses <file>.wyn.out if
# it is newer than the source. A stale binary from a previous build will produce
# the OLD behaviour and look like the fix failed. Each case below writes a fresh
# temp file so nothing is reused.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

n=0
# $1=name  $2=expected "a|b|c" (args AFTER argv[0])  $3..=the args to pass
check() {
    local name="$1" want="$2"; shift 2
    n=$((n+1))
    local f="$TMP/p$n.wyn"
    cat > "$f" <<'WYN'
fn main() {
    args = System.args()
    out = ""
    i = 1
    while i < args.len() {
        out = out + args[i]
        if i < args.len() - 1 { out = out + "|" }
        i = i + 1
    }
    print("ARGS[${out}]")
}
WYN
    local got
    got=$(cd "$TMP" && perl -e 'alarm(60); exec @ARGV' -- "$WYN" run "$f" "$@" 2>&1 | grep -o 'ARGS\[[^]]*\]' | head -1)
    if [ "$got" = "ARGS[$want]" ]; then ok "$name"
    else bad "$name (want 'ARGS[$want]', got '$got')"; fi
}

check "plain arguments are forwarded"            "alpha|beta"    alpha beta
check "a single argument is forwarded"           "only"          only
check "no arguments gives an empty list"         ""
check "-- is consumed, not forwarded"            "gamma|delta"   -- gamma delta
check "an argument that looks like a wyn flag"   "--release"     -- --release
check "a program flag needs no --"               "--verbose|x"   --verbose x
check "a LATER -- belongs to the program"        "a|--|b"        a -- b
check "quoted argument with spaces survives"     "two words"     "two words"

# SHELL INJECTION. The arguments are spliced into a command string that goes to a
# shell, and they were appended as a bare " %s" - so an argument containing shell
# metacharacters was EXECUTED, not passed. This asserts the argument arrives as
# LITERAL TEXT and that the side effect did not happen.
n=$((n+1))
f="$TMP/inj.wyn"
cat > "$f" <<'WYN'
fn main() {
    args = System.args()
    if args.len() > 1 { print("LIT[${args[1]}]") }
}
WYN
rm -f "$TMP/PWNED"
got=$(cd "$TMP" && perl -e 'alarm(60); exec @ARGV' -- "$WYN" run "$f" "; touch $TMP/PWNED" 2>&1 | grep -o 'LIT\[[^]]*\]' | head -1)
if [ -f "$TMP/PWNED" ]; then
    bad "a shell metacharacter in an argument must NOT execute (it DID)"
else
    ok "a shell metacharacter in an argument does not execute"
fi
if [ -n "$got" ]; then ok "the metacharacter argument arrives as literal text"
else bad "the metacharacter argument did not arrive at all (got '$got')"; fi

# wyn's OWN flags must still work, and must NOT reach the program.
n=$((n+1))
f="$TMP/dbg.wyn"
cat > "$f" <<'WYN'
fn main() {
    args = System.args()
    print("N=${args.len()}")
}
WYN
out=$(cd "$TMP" && perl -e 'alarm(60); exec @ARGV' -- "$WYN" run --debug "$f" one 2>&1)
if echo "$out" | grep -qxF "N=2"; then ok "--debug before the file stays wyn's, and the arg still arrives"
else bad "--debug handling (want N=2, got: $(echo "$out" | grep -o 'N=[0-9]*' | head -1))"; fi
if [ -f "$TMP/dbg.wyn.c" ]; then ok "--debug still kept the generated .c"
else bad "--debug no longer keeps the .c"; fi

echo ""; echo "argv-forward: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
