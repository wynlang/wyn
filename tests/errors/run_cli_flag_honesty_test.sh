#!/bin/bash
# V-13: AN UNKNOWN FLAG IS AN ERROR. A ✓ over the wrong artifact is not.
#
# `wyn build x.wyn --wasm` exited 0, printed a green ✓, and left a native
# Mach-O/ELF binary. A false success is the worst failure mode available: a
# first-time visitor's first command produced a silently wrong artifact and the
# tool told them it worked. `--relase`, `--debugg`, `-O3` and `--target=wasm`
# behaved the same way - main.c's build loop ended in
#     else if (!dir) dir = argv[i];
# so an unrecognised flag became the FILE if none had been seen yet and was
# DISCARDED otherwise. Hence the position-dependent split:
#     wyn build x.wyn --wasm  ->  0, ✓, native binary   (flag discarded)
#     wyn build --wasm x.wyn  ->  1, "No main.wyn found in --wasm"  (flag as path)
# `--release` works in either position, so the inconsistency was not even uniform.
#
# EXIT STATUS IS THE PROPERTY. The whole defect is that the status was 0, so an
# output-only assertion here would be vacuous by construction - every arm below
# checks the status, and the artifact arms also check that no binary was left.
#
# THE ANTI-CHEAT ARMS MATTER AS MUCH. "Reject anything flag-shaped" would pass
# every rejection arm and destroy the tool, so:
#   * a VALID flag must still work, in BOTH positions;
#   * `wyn run f.wyn --anything` must still forward to the PROGRAM - wyn's own
#     flags come before the path, and a program's own --verbose is
#     indistinguishable from one of ours (src/main.c, the argument-split comment).
#     Policing that region would break every Wyn CLI tool.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

SRC="$TMP/x.wyn"
cat > "$SRC" <<'WYN'
fn main() -> int {
    print("hi")
    return 0
}
WYN

clean() { rm -f "$TMP/x" "$TMP/x.out" "$TMP/x.wyn.c" "$TMP/x.wyn.out" "$TMP/x.js" "$TMP/x.wasm" 2>/dev/null; }
artifact_exists() { [ -f "$TMP/x" ] || [ -f "$TMP/x.out" ]; }

# An unknown flag must: exit nonzero, say so IN THOSE TERMS naming the flag, and
# leave NO artifact.
#
# The "in those terms" part is not pedantry. Two of these arms already exited
# nonzero on pristine `dev` for UNRELATED reasons - `wyn run --wasm f.wyn` died with
# "Could not open file '--wasm'" (the flag was taken as the path) and
# `wyn cross wasm f.wyn --nonsense` died because emcc is not installed - so a
# status-only assertion passed while the defect was live. Requiring the diagnostic
# to be about the FLAG is what makes those two arms mean anything.
#
# $2 must be the offending flag (the arms below are written so it is).
reject() {   # $1 = human label, $2.. = argv (with the offending flag as $2 of argv)
    local label="$1"; shift
    local flag=""
    for a in "$@"; do case "$a" in -*) flag="$a" ;; esac; done
    clean
    local out rc
    out=$(perl -e 'alarm(120); exec @ARGV' -- "$@" 2>&1); rc=$?
    local why=""
    [ "$rc" -eq 0 ] && why="$why exit=0"
    artifact_exists && why="$why left-an-artifact"
    echo "$out" | grep -qi "unknown flag" || why="$why not-reported-as-a-flag-error"
    echo "$out" | grep -qF -- "$flag"     || why="$why does-not-name-$flag"
    if [ -z "$why" ]; then
        ok "$label is rejected (rc=$rc, no artifact, names the flag)"
    else
        bad "$label:$why  [$(echo "$out" | head -2 | tr '\n' ' ' | cut -c1-120)]"
    fi
    clean
}

# A valid flag must still work, and leave the artifact it promises.
accept() {   # $1 = label, $2 = expect-artifact (1/0), rest = argv
    local label="$1" want_art="$2"; shift 2
    clean
    local out rc
    out=$(perl -e 'alarm(180); exec @ARGV' -- "$@" 2>&1); rc=$?
    local why=""
    [ "$rc" -ne 0 ] && why="$why rc=$rc"
    if [ "$want_art" = 1 ] && ! artifact_exists; then why="$why no-artifact"; fi
    if [ -z "$why" ]; then
        ok "$label still works"
    else
        bad "$label:$why  [$(echo "$out" | head -2 | tr '\n' ' ' | cut -c1-120)]"
    fi
    clean
}

echo "  -- an unknown flag is an error, in BOTH positions --"
reject "build <file> --wasm"     "$WYN" build "$SRC" --wasm
reject "build --wasm <file>"     "$WYN" build --wasm "$SRC"
reject "build <file> --relase"   "$WYN" build "$SRC" --relase
reject "build <file> --debugg"   "$WYN" build "$SRC" --debugg
reject "build <file> -O3"        "$WYN" build "$SRC" -O3
reject "build <file> --target=wasm" "$WYN" build "$SRC" --target=wasm
reject "check <file> --wasm"     "$WYN" check "$SRC" --wasm
reject "run --wasm <file>"       "$WYN" run --wasm "$SRC"
reject "cross wasm <file> --nonsense" "$WYN" cross wasm "$SRC" --nonsense

echo "  -- a typo'd flag names the flag it could not accept --"
out=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" build "$SRC" --relase 2>&1 || true); clean
if echo "$out" | grep -q -- "--relase"; then ok "the message quotes the flag the user typed"
else bad "the message does not quote '--relase' [$(echo "$out" | head -2 | tr '\n' ' ')]"; fi
if echo "$out" | grep -q -- "--release"; then ok "and suggests the flag they meant"
else bad "no suggestion for '--relase' -> '--release' [$(echo "$out" | head -2 | tr '\n' ' ')]"; fi
out=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" build "$SRC" --target=wasm 2>&1 || true); clean
if echo "$out" | grep -qi "target wasm\|--target <"; then ok "--flag=value points at the space-separated spelling"
else bad "--target=wasm gives no hint about the accepted spelling [$(echo "$out" | head -2 | tr '\n' ' ')]"; fi

echo "  -- a VALID flag must still work, in both positions (the anti-cheat arms) --"
accept "build <file> --release"  1 "$WYN" build "$SRC" --release
accept "build --release <file>"  1 "$WYN" build --release "$SRC"
accept "build <file> --fast"     1 "$WYN" build "$SRC" --fast
accept "build <file> -o <name>"  0 "$WYN" build "$SRC" -o "$TMP/named"
[ -f "$TMP/named" ] && ok "-o still names the output" || bad "-o did not produce \$TMP/named"
accept "run --release <file>"    0 "$WYN" run --release "$SRC"
accept "run <file>"              0 "$WYN" run "$SRC"
accept "check <file>"            0 "$WYN" check "$SRC"
accept "fmt <file> --check"      0 "$WYN" fmt "$SRC" --check

echo "  -- flags AFTER the path belong to the PROGRAM for wyn run (deliberate) --"
cat > "$TMP/args.wyn" <<'WYN'
fn main() -> int {
    var a = System.args()
    print("n=${a.len()}")
    return 0
}
WYN
rm -f "$TMP/args.wyn.out"
out=$(perl -e 'alarm(180); exec @ARGV' -- "$WYN" run "$TMP/args.wyn" --wasm --whatever 2>&1); rc=$?
if [ "$rc" -eq 0 ] && echo "$out" | grep -q "n=3"; then
    ok "wyn run <file> --wasm --whatever forwards both to the program (n=3)"
else
    bad "wyn run stopped forwarding a program's own flags (rc=$rc) [$(echo "$out" | head -2 | tr '\n' ' ')]"
fi
rm -f "$TMP/args.wyn.out" "$TMP/args.wyn.c"

echo "  -- wyn cross: the usage text and the unknown-target error must AGREE --"
u=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" cross 2>&1 || true)
e=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" cross definitely-not-a-target "$SRC" 2>&1 || true); clean
# Read ONLY the canonical list line ("Targets:" in the usage, "Available:" in the
# error). The usage also prints an ACCEPTED-ALIASES line (linux-x64, win64, wasm32,
# ...) which is deliberately not the same set - aliases are spellings, not targets -
# so a naive scan of the whole output compares apples with pears and reports a
# contradiction that is not one.
extract() {
    printf '%s' "$1" \
      | sed -nE 's/^[[:space:]]*(Targets|Available):[[:space:]]*//p' \
      | tr 'A-Z|,' 'a-z  ' \
      | grep -oE '\b(linux|linux-x64|linux-arm64|macos|macos-x64|macos-arm64|windows|windows-x64|ios|android|wasm)\b' \
      | sort -u
}
ul=$(extract "$u"); el=$(extract "$e")
if [ -z "$ul" ] || [ -z "$el" ]; then
    bad "could not read a target list from cross usage / cross error [usage=$(echo "$u" | head -1)] [err=$(echo "$e" | head -1)]"
elif [ "$ul" = "$el" ]; then
    ok "cross usage and cross error advertise the same targets ($(echo $ul | tr '\n' ' '))"
else
    bad "cross contradicts itself - usage says [$(echo $ul | tr '\n' ' ')] but the error says [$(echo $el | tr '\n' ' ')]"
fi
# wasm specifically: `wyn cross wasm` IS implemented (emcc), so both lists must name it.
if printf '%s' "$ul" | grep -qx wasm && printf '%s' "$el" | grep -qx wasm; then
    ok "wasm appears in both (wyn cross wasm is implemented via emcc)"
else
    bad "wasm is missing from one of the two lists"
fi

echo ""
echo "cli-flag-honesty: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
