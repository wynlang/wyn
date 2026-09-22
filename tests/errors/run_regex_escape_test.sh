#!/bin/bash
# Regex shorthand character classes: \d \w \s and \D \W \S must match what they
# say - or be rejected. They must never match something ELSE.
#
# THE DEFECT THIS GATES (measured on dev @ 11637e24, macOS):
#
#     Regex.replace("2026-04-23 ERROR: disk full", "\\d+", "N")
#       -> "2026-04-23 ERROR: Nisk full"     # rewrote the LETTER "d"isk
#     Regex.match("2026-04-23", "^\\d+")  -> false
#     Regex.match("2026-04-23", "^[0-9]{4}") -> true      # so the engine is fine
#
# Root cause: Wyn has TWO regex engines - <regex.h> (POSIX ERE) everywhere but
# Windows, and the bundled NFA in src/wyn_regex.h on Windows. The NFA engine
# implements \d/\w/\s; POSIX ERE has no such escape, and regcomp() does not
# reject an unknown one - it quietly drops the backslash and matches the letter.
# So the SAME program gave different answers on Windows and Unix, and the Unix
# answer was wrong at exit 0. Both engines now share ONE expansion pass
# (wyn_regex_expand_escapes in src/wyn_regex_escapes.h) applied at the single
# point where a pattern is compiled, which is why the two cannot drift again.
#
# NOTE FOR ANYONE RE-RUNNING THIS: the regex functions live in the RUNTIME,
# precompiled into runtime/libwyn_rt.a. After editing wyn_runtime.h or
# wyn_regex_escapes.h you must `rm -f runtime/obj/*.o && make runtime`, or
# compiled programs keep the old behaviour and this gate appears to fail against
# a correct source tree.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# Compare the program's whole output, lines joined with '|'. Joined rather than
# per-line because Regex.split/find_all legitimately return newline-separated
# text and the newlines are part of the answer under test.
run_expect() {   # $1=name $2=expected-joined-output ; source on stdin
    local f="$TMP/t.wyn"; cat > "$f"
    local out rc got
    out=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" run "$f" 2>&1); rc=$?
    if [ $rc -ne 0 ]; then
        bad "$1 (rc=$rc)"; echo "$out" | grep -E 'error:|^Error|panic' | head -2 | sed 's/^/        /'; return
    fi
    got=$(echo "$out" | grep -vE 'Compiled in|^Warning:' | tr '\n' '|')
    if [ "$got" = "$2" ]; then ok "$1"
    else bad "$1"; echo "        want: $2"; echo "        got:  $got"; fi
}

# --- \d, \w, \s must match the CLASS, not the letter ------------------------
run_expect "\\d+ rewrites the digits, not the letter d" "N-N-N ERROR: disk full|" <<'WYN'
fn main() {
    print(Regex.replace("2026-04-23 ERROR: disk full", "\\d+", "N"))
}
WYN

run_expect "\\d \\w \\s match their classes" "true|false|true|false|true|false|" <<'WYN'
fn main() {
    print(Regex.match("2026-04-23", "^\\d+"))
    print(Regex.match("no digits here", "\\d"))
    print(Regex.match("abc_9", "^\\w+$"))
    print(Regex.match("a-b", "^\\w+$"))
    print(Regex.match("a b", "\\s"))
    print(Regex.match("ab", "\\s"))
}
WYN

# --- the NEGATED forms. \D silently-literal while \d works is the same bug. --
run_expect "\\D \\W \\S match the complement" "true|false|true|false|true|false|" <<'WYN'
fn main() {
    print(Regex.match("abc", "\\D"))
    print(Regex.match("123", "^\\D"))
    print(Regex.match("a b", "\\W"))
    print(Regex.match("a1_", "\\W"))
    print(Regex.match("ab", "\\S"))
    print(Regex.match("   ", "\\S"))
}
WYN

# --- every Regex entry point, not just match/replace ------------------------
# find/find_all/split each compile the pattern at their OWN call site; a fix
# applied to one of them is not a fix.
run_expect "find, find_all and split all honour \\d" "2|1|22||a|b|c|" <<'WYN'
fn main() {
    print(Regex.find("ab12", "\\d"))
    print(Regex.find_all("a1b22c", "\\d+"))
    print(Regex.split("a1b2c", "\\d"))
}
WYN

# --- shorthand INSIDE a bracket expression ----------------------------------
# POSIX brackets treat backslash as a literal member, so "[\d]" was the set
# {backslash, d} - wrong in the same silent way.
run_expect "[\\d] and [\\w\\s] work inside a class" "true|false|true|true|" <<'WYN'
fn main() {
    print(Regex.match("5", "[\\d]"))
    print(Regex.match("x", "[\\d]"))
    print(Regex.match("a b", "^[\\w\\s]+$"))
    print(Regex.match("9", "[a-c\\d]"))
}
WYN

# --- what must NOT change ---------------------------------------------------
# The engine was never broken - only the shorthand was. The expansion pass runs
# over EVERY pattern, so these are the arms that catch it corrupting one.
run_expect "ordinary ERE is untouched" "true|true|true|false|true|true|true|" <<'WYN'
fn main() {
    print(Regex.match("2026", "^[0-9]{4}$"))
    print(Regex.match("cat", "^(cat|dog)$"))
    print(Regex.match("a.b", "a\\.b"))
    print(Regex.match("axb", "a\\.b"))
    print(Regex.match("5", "[[:digit:]]"))
    print(Regex.match("a]b", "[]]"))
    print(Regex.match("x^y", "[\\^]"))
}
WYN

# An escaped backslash is a literal backslash - it must NOT be read as the start
# of a shorthand. "\\\\d" in Wyn source is the two-char ERE \\ then a literal d.
run_expect "an escaped backslash is not a shorthand" "true|false|" <<'WYN'
fn main() {
    print(Regex.match("a\\d", "\\\\d"))
    print(Regex.match("a7", "\\\\d"))
}
WYN

# --- the one case ERE cannot express: reject it, loudly ----------------------
# "[a\D]" is {a} union complement({0-9}). POSIX bracket expressions have no
# complement operator, so there is no honest translation. Guessing one is how
# this defect started, so the runtime refuses instead - naming the class to use.
reject_test() {
    local f="$TMP/r.wyn"
    cat > "$f" <<'WYN'
fn main() {
    print(Regex.match("x", "[a\\D]"))
}
WYN
    local out rc
    out=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" run "$f" 2>&1); rc=$?
    if [ $rc -eq 0 ]; then
        bad "a negated shorthand inside a class is rejected (exited 0: $(echo "$out" | tr '\n' '|'))"
        return
    fi
    if echo "$out" | grep -q 'panic: regex:' && echo "$out" | grep -q '\[\^0-9\]'; then
        ok "a negated shorthand inside a class is rejected, naming the class to use"
    else
        bad "a negated shorthand inside a class is rejected (wrong message)"
        echo "$out" | head -3 | sed 's/^/        /'
    fi
}
reject_test

# --- wyn build must agree with wyn run --------------------------------------
# `wyn run` and `wyn build` take different paths through the runtime header
# (slim vs full), and a stale runtime/libwyn_rt.a shows up as exactly one of
# them being right. Assert the built binary too.
build_test() {
    local f="$TMP/b.wyn"
    cat > "$f" <<'WYN'
fn main() {
    print(Regex.replace("a1b22c", "\\d+", "#"))
}
WYN
    local out rc
    out=$(cd "$TMP" && perl -e 'alarm(120); exec @ARGV' -- "$WYN" build "$f" 2>&1); rc=$?
    if [ $rc -ne 0 ]; then bad "wyn build agrees with wyn run (build rc=$rc)"; return; fi
    local bin="${f%.wyn}"
    [ -x "$bin" ] || bin="$TMP/b"
    if [ ! -x "$bin" ]; then bad "wyn build agrees with wyn run (no binary at $bin)"; return; fi
    out=$(perl -e 'alarm(60); exec @ARGV' -- "$bin" 2>&1)
    if [ "$out" = "a#b#c" ]; then ok "wyn build agrees with wyn run"
    else bad "wyn build agrees with wyn run (want 'a#b#c', got '$out')"; fi
}
build_test

echo ""; echo "regex-escapes: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
