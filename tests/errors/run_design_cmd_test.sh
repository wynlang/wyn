#!/bin/bash
# `wyn design [file]` - the launcher for the Visual Wyn form designer.
#
# WHAT IS BEING GATED, and why it is a launcher at all: the designer is a Wyn
# PROGRAM (examples/designer.wyn in the `gui` package), not compiler code. It
# needs SDL3, so compiling it into `wyn` would put an SDL3 dependency on the
# compiler that every user pays for and almost none want. The subcommand's whole
# job is therefore to FIND that program and to behave well when it is absent -
# and absent is the COMMON case, since nobody has the gui package until they ask
# for it. So the missing-designer message is the primary user experience here,
# and it is asserted as hard as the happy path.
#
# The spelling is an owner decision (internal-docs/VISUAL_WYN_DESIGN.md): `wyn
# ui` was already taken by the interactive command browser, and `design` matches
# the all-verb style of run/check/fmt/test/watch/build.
#
# HERMETICITY: resolution consults WYN_PKG_CACHE, HOME and the repos checkout
# beside WYN_ROOT, so a developer box WITH repos/gui present would otherwise
# launch a real SDL3 window from a test run. Every case below either sets
# WYN_GUI_ROOT (which is exclusive - see design_candidates in src/cmd_other.c) or
# points all three of those at empty temp dirs. If this test ever opens a window
# on your machine, that isolation has broken, not the feature.
#
# The designer stub is a plain Wyn program, so the launch path is exercised for
# real - argument forwarding, the absolute-path rewrite, and the chdir into the
# package root - without needing SDL3 in CI.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# An empty world: no package cache, no ~/.wyn, and a WYN_ROOT whose sibling
# ../gui does not exist. WYN_ROOT must still look like a wyn install (it needs
# src/wyn_runtime.h) or resolve_wyn_root falls back to the exe dir - which IS the
# real checkout, whose sibling repos/gui exists on a dev box.
mkdir -p "$TMP/root/src" && : > "$TMP/root/src/wyn_runtime.h"
nogui() { env -u WYN_GUI_ROOT WYN_ROOT="$TMP/root" WYN_PKG_CACHE="$TMP/nocache" \
              HOME="$TMP/nohome" "$@"; }

# ---------------------------------------------------------------------------
# 1. THE MISSING-DESIGNER MESSAGE. Actionable = names the package, gives the
#    command that installs it, and lists what it probed. A bare nonzero exit or
#    a stack trace fails this even though the exit code would be identical.
out=$(cd "$TMP" && nogui perl -e 'alarm(60); exec @ARGV' -- "$WYN" design 2>&1); rc=$?

if [ "$rc" -ne 0 ]; then ok "a missing designer exits non-zero"
else bad "a missing designer exited 0 (silent failure)"; fi

if echo "$out" | grep -q "wyn add gui"; then ok "the message gives the install command"
else bad "the message does not say how to install it [$out]"; fi

if echo "$out" | grep -q "WYN_GUI_ROOT"; then ok "the message offers the checkout override"
else bad "the message never mentions WYN_GUI_ROOT [$out]"; fi

if echo "$out" | grep -q "Looked in:" && echo "$out" | grep -q "designer.wyn"; then
    ok "the message lists the paths it probed"
else bad "the message does not say where it looked [$out]"; fi

# It must not look like a crash. A launcher whose failure mode reads as a
# compiler bug sends the user to the wrong place entirely.
if echo "$out" | grep -qiE "segmentation|internal codegen error|Parse error|Assertion"; then
    bad "the missing-designer path looks like a crash [$out]"
else ok "the missing-designer path is a diagnostic, not a crash"; fi

# An explicitly-set-but-wrong WYN_GUI_ROOT has to say so. Silently falling
# through to some other copy would give you a designer you did not ask for and
# never explain why your checkout was rejected.
out=$(cd "$TMP" && WYN_GUI_ROOT="$TMP/notgui" perl -e 'alarm(60); exec @ARGV' -- \
      "$WYN" design 2>&1); rc=$?
if [ "$rc" -ne 0 ] && echo "$out" | grep -q "is set but has no examples/designer.wyn"; then
    ok "a wrong WYN_GUI_ROOT is named as the reason"
else bad "a wrong WYN_GUI_ROOT is not diagnosed (rc=$rc) [$out]"; fi

# ---------------------------------------------------------------------------
# 2. A DIRECTORY ARGUMENT IS REFUSED, exactly as `wyn check` refuses one. That
#    bug shipped: a directory "read" as empty source and printed "✓ no errors",
#    a false green. Here the equivalent would launch the designer on something it
#    can never save, so it is caught before resolution even runs.
mkdir -p "$TMP/formdir"
out=$(cd "$TMP" && nogui "$WYN" design formdir 2>&1); rc=$?
if [ "$rc" -ne 0 ] && echo "$out" | grep -q "is a directory"; then
    ok "a directory argument is refused with a clear message"
else bad "a directory argument was not refused (rc=$rc) [$out]"; fi
# The directory check must fire BEFORE the missing-designer path, or the user is
# told to install a package when the real problem is their argument.
if echo "$out" | grep -q "wyn add gui"; then
    bad "a directory argument reported the wrong problem (install advice)"
else ok "a directory argument is diagnosed as such, not as a missing package"; fi

# ---------------------------------------------------------------------------
# 3. `--help` ON THE SUBCOMMAND, and a line in the main help output.
out=$(nogui "$WYN" design --help 2>&1); rc=$?
if [ "$rc" -eq 0 ]; then ok "wyn design --help exits 0"
else bad "wyn design --help exited $rc"; fi
for want in "wyn design" "Form1.json" "designer.wyn" "wyn add gui"; do
    if echo "$out" | grep -qF "$want"; then ok "design --help mentions '$want'"
    else bad "design --help omits '$want' [$out]"; fi
done
# --help must NOT try to resolve or launch anything: it is documentation, and it
# has to work identically whether or not the package is installed.
if echo "$out" | grep -q "Looked in:"; then bad "design --help ran the resolver"
else ok "design --help does not attempt to resolve the designer"; fi

# The main help output has to learn the new verb too, in both spellings that
# print it (bare `wyn` and `wyn help`).
helptext=$("$WYN" help 2>&1)
if echo "$helptext" | grep -q "design"; then ok "wyn help lists design"
else bad "wyn help does not list design"; fi
# The bare banner is captured rather than piped: `wyn` with no arguments exits 1
# (it is a usage error), and under `pipefail` a pipeline through grep inherits
# that - which reads as "design is missing" when it is present.
banner=$("$WYN" 2>&1)
if echo "$banner" | grep -q "design"; then ok "the no-args banner lists design"
else bad "the no-args banner does not list design"; fi

# ---------------------------------------------------------------------------
# 4. THE DEFAULT FILE AND THE LAUNCH. A stub designer stands in for the real one:
#    it prints the arguments it was handed and whether it is running in the
#    package root. Both matter.
#
#    The form path must arrive ABSOLUTE, because the launcher chdirs into the
#    package root before running: the designer links SDL3 through that package's
#    wyn.toml [ffi] section, so started from the user's cwd it does not link at
#    all - while the FORM belongs to the user's cwd. The two directories cannot
#    be the same, so exactly one of the paths has to be absolute. A relative form
#    path would silently resolve inside the gui package and create the form in
#    the wrong place.
mkdir -p "$TMP/gui/examples"
cat > "$TMP/gui/examples/designer.wyn" <<'WYN'
fn main() {
    args = System.args()
    print("STUB")
    var i = 1
    while i < args.len() { print("ARG=${args[i]}"); i = i + 1 }
}
WYN
# Marks the package root, which is what the launcher must chdir to.
: > "$TMP/gui/PKGROOT"
mkdir -p "$TMP/work"

launch() {  # $@ = args after `design`; echoes the stub's output
    (cd "$TMP/work" && WYN_GUI_ROOT="$TMP/gui" perl -e 'alarm(120); exec @ARGV' -- \
        "$WYN" design "$@" 2>&1)
}

out=$(launch); rc=$?
if [ "$rc" -eq 0 ] && echo "$out" | grep -qx "STUB"; then ok "the designer is launched"
else bad "the designer did not launch (rc=$rc) [$out]"; fi
# No argument defaults to Form1.json in the CURRENT directory - not in the gui
# package, and not to some other name.
if echo "$out" | grep -q "^ARG=.*/Form1\.json$"; then
    ok "no argument defaults to Form1.json"
else bad "no argument did not default to Form1.json [$(echo "$out" | grep '^ARG=')]"; fi
# macOS resolves /tmp through a /private symlink, so compare the tail, not the
# whole string. The property under test is "the user's cwd", not the exact prefix.
if echo "$out" | grep -q "^ARG=/.*work/Form1\.json$"; then
    ok "the default form resolves in the user's cwd, absolute"
else bad "the default form is not an absolute path under the cwd [$(echo "$out" | grep '^ARG=')]"; fi
# Exactly one argument reaches the designer: the form. A stray extra (an empty
# string, a leaked flag) would be read as a second file.
nargs=$(echo "$out" | grep -c '^ARG=')
if [ "$nargs" -eq 1 ]; then ok "exactly one argument is forwarded"
else bad "expected 1 forwarded argument, got $nargs [$out]"; fi

out=$(launch MyForm.json)
if echo "$out" | grep -q "^ARG=/.*work/MyForm\.json$"; then
    ok "an explicit file is passed through, made absolute"
else bad "an explicit file did not arrive absolute [$(echo "$out" | grep '^ARG=')]"; fi

# An absolute argument must be left exactly as given, not re-rooted at the cwd.
out=$(launch "$TMP/work/Abs.json")
if echo "$out" | grep -q "^ARG=.*/work/Abs\.json$" && \
   ! echo "$out" | grep -q "work/.*work/"; then
    ok "an absolute file argument is not re-rooted"
else bad "an absolute file argument was mangled [$(echo "$out" | grep '^ARG=')]"; fi

# The designer must run FROM the package root (where wyn.toml lives), which is
# the only place its [ffi] SDL3 link flags are found. Verified by asking the
# process itself, rather than by reading the code.
cat > "$TMP/gui/examples/designer.wyn" <<'WYN'
fn main() {
    if File.exists("PKGROOT") { print("CWD=pkgroot") } else { print("CWD=elsewhere") }
}
WYN
out=$(launch)
if echo "$out" | grep -qx "CWD=pkgroot"; then
    ok "the designer runs from the gui package root (so [ffi] SDL3 links)"
else bad "the designer did not run from the package root [$out]"; fi

# A flat checkout (designer.wyn at the root, no examples/) resolves too, and the
# package root is then that same directory - the examples/ strip must not eat a
# level it did not add.
mkdir -p "$TMP/flat"
cat > "$TMP/flat/designer.wyn" <<'WYN'
fn main() { if File.exists("PKGROOT") { print("CWD=pkgroot") } else { print("CWD=elsewhere") } }
WYN
: > "$TMP/flat/PKGROOT"
out=$(cd "$TMP/work" && WYN_GUI_ROOT="$TMP/flat" perl -e 'alarm(120); exec @ARGV' -- \
      "$WYN" design 2>&1)
if echo "$out" | grep -qx "CWD=pkgroot"; then ok "a flat gui checkout resolves and roots correctly"
else bad "a flat gui checkout mis-resolved [$out]"; fi

# ---------------------------------------------------------------------------
# 5. THE TYPO SUGGESTION. `wyn desgin` must suggest `design`.
#
# This is not cosmetic: the suggester was first-letter-wins in list order, so
# every d-typo got "doc" no matter what was typed, and adding `design` to the
# list would not have changed that. It is edit-distance-first now, with the old
# first-letter/substring rule kept as the fallback - so the cases that already
# worked still do, which the last three assertions pin.
typo() {  # $1 = typed command, $2 = expected suggestion
    local out; out=$("$WYN" "$1" 2>&1)
    if echo "$out" | grep -q "Did you mean.*$2"; then ok "'wyn $1' suggests '$2'"
    else bad "'wyn $1' did not suggest '$2' [$out]"; fi
}
typo desgin design
typo deisgn design
typo desing design
typo buld   build
typo chek   check
typo tset   test

# The unknown-command path still exits non-zero, suggestion or not.
"$WYN" zzzznotacommand >/dev/null 2>&1
if [ $? -ne 0 ]; then ok "an unknown command still exits non-zero"
else bad "an unknown command exited 0"; fi

echo ""; echo "design-cmd: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
