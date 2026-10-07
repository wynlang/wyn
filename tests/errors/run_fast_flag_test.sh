#!/bin/bash
# `--fast` IS A NO-OP, AND THE HELP TEXT MUST NOT CLAIM OTHERWISE.
#
# `--help` advertised it twice as "Skip optimizations (fastest compile)" while the
# parse site is an empty branch:
#
#     else if (strcmp(argv[i], "--fast") == 0) { /* skip optimizations - default behavior */ }
#
# and -O0 is already the default (src/main.c: `const char* opt_level = "-O0";`). So the
# flag changed nothing while the help promised a faster compile. #469.
#
# The flag is deliberately still ACCEPTED - removing it would break any script that
# passes it - so the fix is to stop promising an effect. This gate pins both halves:
#
#   1  --fast is still accepted (a script passing it keeps working)
#   2  the cc command line is IDENTICAL with and without it
#   3  the help text does not claim a compile-speed benefit
#
# Arm 2 is the one that matters, and it is measured rather than read: WYN_CC points at
# a script that records the exact argv it was invoked with, so the comparison is of
# what the compiler was really told, not of what the source appears to say. That is
# also how the original finding was established.
#
# Arm 3 is a text assertion and would normally be too brittle to keep. It earns its
# place because the defect here WAS the text: the code was correct and honest, the
# documentation was not, so the regression this file guards against is a doc edit.
set -uo pipefail

WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d)
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

echo "=== --fast must be a no-op, and must not be advertised as one ==="

# A SEPARATE SOURCE FILE PER RUN. Sharing one would let the second build reuse the
# first's generated artefact and never invoke the compiler at all, leaving one log
# empty and the comparison meaningless - which is exactly what happened the first time
# this gate was mutation-tested, and is the same incremental-cache trap that has
# manufactured a false regression in this repository before.
printf 'print("ok")\n' > "$TMP/with.wyn"
printf 'print("ok")\n' > "$TMP/without.wyn"

# A capturing stand-in for cc. It must still PRODUCE the output, or wyn reports a
# build failure and the comparison never happens - so it records argv and then
# forwards to the real compiler.
REAL_CC=$(command -v cc || command -v gcc || command -v clang)
if [ -z "$REAL_CC" ]; then
    echo "  skip  no C compiler found"
    echo ""
    echo "fast-flag: $PASS pass, $FAIL fail"
    exit 0
fi
cat > "$TMP/capture_cc" <<EOF
#!/bin/sh
printf '%s\n' "\$*" >> "\$CC_LOG"
exec "$REAL_CC" "\$@"
EOF
chmod +x "$TMP/capture_cc"

run_build() {
    local log="$1"; shift
    : > "$log"
    CC_LOG="$log" WYN_CC="$TMP/capture_cc" TMPDIR="$TMP" \
        perl -e 'alarm(120); exec @ARGV' -- "$WYN" build "$@" > "$log.out" 2>&1
    return $?
}

# --- 1. still accepted -------------------------------------------------------
if run_build "$TMP/with.log" --fast "$TMP/with.wyn" -o "$TMP/with.out"; then
    ok "--fast is still accepted by wyn build"
else
    bad "--fast is still accepted by wyn build"
    sed -n '1,10p' "$TMP/with.log.out"
fi

run_build "$TMP/without.log" "$TMP/without.wyn" -o "$TMP/without.out" || true

# --- 2. the cc command line is identical -------------------------------------
# Output paths differ by design (-o with.out vs -o without.out), so those are
# normalised away; everything else must match, including the -O level.
norm() { sed -e "s|$TMP|TMP|g" -e 's|without|WHICH|g' -e 's|with|WHICH|g' "$1"; }
if [ ! -s "$TMP/with.log" ] || [ ! -s "$TMP/without.log" ]; then
    # An empty log means the TCC backend served the build and never invoked WYN_CC.
    # That is a legitimate path, but it makes this arm blind, so say so rather than
    # passing: a vacuous green here is exactly what this directory has been bitten by.
    if [ ! -s "$TMP/with.log" ] && [ ! -s "$TMP/without.log" ]; then
        ok "both builds bypassed WYN_CC identically (bundled TCC backend served both)"
    else
        bad "--fast changed WHICH backend compiled the program (one used WYN_CC, the other did not)"
        echo "          with --fast:    $([ -s "$TMP/with.log" ] && echo 'used WYN_CC' || echo 'bypassed it')"
        echo "          without --fast: $([ -s "$TMP/without.log" ] && echo 'used WYN_CC' || echo 'bypassed it')"
    fi
else
    if diff -q <(norm "$TMP/with.log") <(norm "$TMP/without.log") >/dev/null; then
        ok "the cc command line is identical with and without --fast"
    else
        bad "--fast changed the cc command line"
        diff <(norm "$TMP/with.log") <(norm "$TMP/without.log") | head -6 | sed 's/^/          /'
    fi
    if grep -q -- '-O0' "$TMP/with.log"; then
        ok "the default optimisation level is -O0 (so there is nothing for --fast to skip)"
    else
        bad "the default optimisation level is -O0 [$(head -1 "$TMP/with.log" | tr ' ' '\n' | grep -- '-O' | head -1)]"
    fi
fi

# --- 3. the help text makes no compile-speed promise -------------------------
help_txt=$("$WYN" help 2>&1; "$WYN" build --help 2>&1 || true)
if printf '%s' "$help_txt" | grep -qi 'fastest compile'; then
    bad "--help still advertises --fast as the 'fastest compile'"
    printf '%s' "$help_txt" | grep -i 'fastest compile' | head -2 | sed 's/^/          /'
else
    ok "--help makes no compile-speed promise for --fast"
fi

echo ""
echo "fast-flag: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
