#!/bin/bash
# `wyn build --app` must produce a NATIVE, double-clickable GUI application - not
# a console binary that opens Terminal and then a window.
#
# THE COMPLAINT THIS ENCODES (owner, 2026-07): "right now everything is a binary
# that opens in the terminal and then loads the window from there. While it
# works, it doesn't look good for the end consumer." The fix is packaging, per
# platform, and every part of it is silently wrong-able - which is why the
# assertions below are on file LAYOUT, plist CONTENT and the emitted LINK LINE,
# not on exit status.
#
#   macOS   - a real .app bundle. Two keys are load-bearing and neither shows up
#             as a failure: NSHighResolutionCapable (without it AppKit renders at
#             1x and upscales, so the window is BLURRY on Retina - exactly the
#             "doesn't look good" symptom) and CFBundleExecutable (wrong value =
#             app silently won't launch). Both are asserted by name.
#   Windows - `-mwindows`, PE subsystem WINDOWS instead of CONSOLE. That flag is
#             the entire difference between a console flashing up behind the
#             window and not.
#   Linux   - a .desktop entry with Terminal=false.
#
# WHAT CANNOT BE RUN HERE, STATED PLAINLY: this box is macOS/arm64. There is no
# mingw and no zig, so the Windows .exe and the Linux ELF cannot be LINKED, let
# alone executed. Those two targets are therefore asserted through
# `--app-plan --app-target <t>`, which runs the real packaging code and prints
# the real link flags / writes the real .desktop file without invoking the C
# compiler. That is an assertion on the emitted command line and file, and it is
# labelled as such below - it is NOT a claim that a Windows binary was tested.
#
# The macOS path IS fully executed: bundle built, plist linted with plutil, and
# the binary inside Contents/MacOS RUN.
#
# MUTATION-TESTED: each case was confirmed to go red with the feature removed -
# drop the NSHighResolutionCapable line and case 3 fails; drop " -mwindows" from
# wyn_app_link_flags and case 7 fails; return 0 early from wyn_app_begin and
# cases 1-6 fail; make packaging unconditional (ignore app_flag) and case 9,
# the regression that matters most, fails.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac

# Run a command under a timeout. MUST be used instead of a bare
# `perl -e 'alarm(N); exec @ARGV'` for anything whose path may contain a SPACE:
# perl's exec with a single-element list goes through /bin/sh, which re-splits
# "My Great App.app/.../MyGreatApp" into three words and silently runs nothing
# (exit 0, no output). `exec {$ARGV[0]} @ARGV` forces the list form, which never
# involves a shell. There is no `timeout` binary on macOS.
run_to() { local secs="$1"; shift; perl -e 'alarm(shift); exec {$ARGV[0]} @ARGV' "$secs" "$@"; }
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

HOST=$(uname -s)

# A GUI app is not needed to test PACKAGING: the artifact layout, the plist and
# the link flags are identical for any program. A program that prints is used
# instead precisely so the bundled binary's execution is CHECKABLE - a real SDL3
# app cannot be verified headless in a unit test. (WynCanvas itself was built and
# run through this path by hand; see the change's report.)
mkproj() {
    # $1 = dir  $2 = [app] section body (may be empty)
    mkdir -p "$1/src"
    cat > "$1/src/main.wyn" <<'WYN'
fn main() {
    print("app-ok")
}
WYN
    {
        echo '[project]'
        echo 'name = "proj"'
        echo 'version = "3.2.1"'
        if [ -n "$2" ]; then echo ''; echo '[app]'; echo "$2"; fi
    } > "$1/wyn.toml"
}

# ---------------------------------------------------------------------------
# 1. The bundle LAYOUT. The three paths a .app must have; anything else and
#    Finder treats the directory as a plain folder.
# ---------------------------------------------------------------------------
P="$TMP/p1"; mkproj "$P" 'name = "Demo"
identifier = "com.example.demo"'
(cd "$P" && run_to 180 "$WYN" build src/main.wyn --app >"$TMP/o1" 2>&1)
if [ "$HOST" = "Darwin" ]; then
    if [ -d "$P/Demo.app/Contents" ] &&
       [ -f "$P/Demo.app/Contents/Info.plist" ] &&
       [ -x "$P/Demo.app/Contents/MacOS/Demo" ] &&
       [ -d "$P/Demo.app/Contents/Resources" ]; then
        ok "macOS: Demo.app/Contents/{Info.plist,MacOS/Demo,Resources/} created"
    else
        bad "macOS bundle layout (got: $(ls -R "$P" 2>/dev/null | tr '\n' ' ' | head -c 300))"
    fi
    # The bundle belongs next to the wyn.toml that named it, NOT inside src/ - a
    # .app under src/ looks like a source file and is not where anyone looks for
    # the thing they just built.
    if [ ! -e "$P/src/Demo.app" ]; then
        ok "macOS: bundle sits beside wyn.toml, not inside src/"
    else
        bad "bundle was written into src/ instead of the project root"
    fi
else
    ok "macOS bundle layout (skipped: host is $HOST, not Darwin)"
    ok "bundle placement (skipped: host is $HOST)"
fi

# ---------------------------------------------------------------------------
# 2. The plist must be VALID. A malformed Info.plist does not warn - the app
#    just refuses to launch, so plutil is the gate.
# ---------------------------------------------------------------------------
if [ "$HOST" = "Darwin" ]; then
    if command -v plutil >/dev/null 2>&1; then
        if plutil -lint "$P/Demo.app/Contents/Info.plist" >/dev/null 2>&1; then
            ok "Info.plist passes plutil -lint"
        else
            bad "Info.plist is NOT valid: $(plutil -lint "$P/Demo.app/Contents/Info.plist" 2>&1)"
        fi
    else
        bad "plutil not found - cannot validate the plist on a Darwin host"
    fi
else
    ok "plutil -lint (skipped: host is $HOST)"
fi

# ---------------------------------------------------------------------------
# 3. Every REQUIRED plist key, by name and by value. NSHighResolutionCapable is
#    the one that produces the blurry-window complaint when missing, and a
#    missing key is invisible: the app still launches.
# ---------------------------------------------------------------------------
if [ "$HOST" = "Darwin" ]; then
    PL="$P/Demo.app/Contents/Info.plist"
    miss=""
    for k in CFBundleExecutable CFBundleIdentifier CFBundleName \
             CFBundlePackageType CFBundleShortVersionString \
             NSHighResolutionCapable LSMinimumSystemVersion; do
        grep -q "<key>$k</key>" "$PL" 2>/dev/null || miss="$miss $k"
    done
    [ -z "$miss" ] && ok "Info.plist has every required key" \
                   || bad "Info.plist is missing:$miss"

    # Values, not just presence. CFBundleExecutable must name the file actually
    # in Contents/MacOS or the app will not start; PackageType must be APPL;
    # NSHighResolutionCapable must be <true/> (a <false/> is just as blurry as
    # an absent key); the version must come from the manifest, not be hardcoded.
    v_exec=$(plutil -extract CFBundleExecutable raw "$PL" 2>/dev/null)
    v_type=$(plutil -extract CFBundlePackageType raw "$PL" 2>/dev/null)
    v_id=$(plutil -extract CFBundleIdentifier raw "$PL" 2>/dev/null)
    v_ver=$(plutil -extract CFBundleShortVersionString raw "$PL" 2>/dev/null)
    v_hi=$(plutil -extract NSHighResolutionCapable raw "$PL" 2>/dev/null)
    errs=""
    [ "$v_exec" = "Demo" ]              || errs="$errs CFBundleExecutable='$v_exec'"
    [ -x "$P/Demo.app/Contents/MacOS/$v_exec" ] || errs="$errs CFBundleExecutable-names-no-file"
    [ "$v_type" = "APPL" ]              || errs="$errs CFBundlePackageType='$v_type'"
    [ "$v_id" = "com.example.demo" ]    || errs="$errs CFBundleIdentifier='$v_id'"
    [ "$v_ver" = "3.2.1" ]              || errs="$errs version='$v_ver'(want 3.2.1 from [project])"
    [ "$v_hi" = "true" ] || [ "$v_hi" = "1" ] || errs="$errs NSHighResolutionCapable='$v_hi'"
    [ -z "$errs" ] && ok "Info.plist key VALUES are correct (exec, APPL, id, version, hidpi)" \
                   || bad "Info.plist wrong values:$errs"
else
    ok "required plist keys (skipped: host is $HOST)"
    ok "plist key values (skipped: host is $HOST)"
fi

# ---------------------------------------------------------------------------
# 4. The binary inside the bundle must RUN. Staging moves it out of TMPDIR into
#    Contents/MacOS; a byte-copy fallback that dropped the exec bit, or a
#    truncated copy, would pass every check above and fail here.
# ---------------------------------------------------------------------------
if [ "$HOST" = "Darwin" ]; then
    out=$(run_to 60 "$P/Demo.app/Contents/MacOS/Demo" 2>&1)
    [ "$out" = "app-ok" ] && ok "the executable inside the bundle runs" \
                          || bad "bundled executable printed '$out', want 'app-ok'"
else
    ok "bundled executable runs (skipped: host is $HOST)"
fi

# ---------------------------------------------------------------------------
# 5. An [app] name WITH A SPACE. "My Great App" is the normal case for a
#    consumer app and it breaks two things at once: the link line goes through
#    system(), so an unquoted `-o My Great App.app` was measured failing with
#    clang's `no such file or directory: 'App'`; and a space is illegal in
#    CFBundleExecutable. The name must survive in CFBundleName while the
#    executable is sanitized.
# ---------------------------------------------------------------------------
P2="$TMP/p2"; mkproj "$P2" 'name = "My Great App"
identifier = "com.example.great"'
(cd "$P2" && run_to 180 "$WYN" build src/main.wyn --app >"$TMP/o2" 2>&1)
if [ "$HOST" = "Darwin" ]; then
    B="$P2/My Great App.app"
    if [ -d "$B" ] && [ -x "$B/Contents/MacOS/MyGreatApp" ]; then
        nm=$(plutil -extract CFBundleName raw "$B/Contents/Info.plist" 2>/dev/null)
        ex=$(plutil -extract CFBundleExecutable raw "$B/Contents/Info.plist" 2>/dev/null)
        got=$(run_to 60 "$B/Contents/MacOS/MyGreatApp" 2>&1)
        if [ "$nm" = "My Great App" ] && [ "$ex" = "MyGreatApp" ] && [ "$got" = "app-ok" ]; then
            ok "a name with spaces: bundle builds, CFBundleName keeps them, exec is sanitized, runs"
        else
            bad "spaced name (CFBundleName='$nm' exec='$ex' output='$got')"
        fi
    else
        bad "spaced name produced no bundle at '$B' (build said: $(tail -2 "$TMP/o2" | tr '\n' ' '))"
    fi
else
    ok "spaced [app] name (skipped: host is $HOST)"
fi

# ---------------------------------------------------------------------------
# 6. A MISSING ICON must NOT be fatal. An icon is cosmetic; failing a build over
#    an absent .icns would be worse than shipping the default one. The build must
#    succeed, warn, and omit CFBundleIconFile rather than point at nothing.
# ---------------------------------------------------------------------------
P3="$TMP/p3"; mkproj "$P3" 'name = "NoIcon"
identifier = "com.example.noicon"
icon = "assets/does-not-exist.icns"'
(cd "$P3" && run_to 180 "$WYN" build src/main.wyn --app >"$TMP/o3" 2>&1)
rc3=$?
if [ "$HOST" = "Darwin" ]; then
    if [ $rc3 -eq 0 ] && [ -x "$P3/NoIcon.app/Contents/MacOS/NoIcon" ]; then
        if grep -qi "warning" "$TMP/o3" &&
           ! grep -q "<key>CFBundleIconFile</key>" "$P3/NoIcon.app/Contents/Info.plist"; then
            ok "a missing icon warns but still builds, and CFBundleIconFile is omitted"
        else
            bad "missing icon: expected a warning and no CFBundleIconFile (log: $(tr '\n' ' ' <"$TMP/o3" | head -c 200))"
        fi
    else
        bad "a missing icon was FATAL (rc=$rc3) - it must only warn"
    fi
else
    ok "missing icon is non-fatal (skipped: host is $HOST)"
fi

# A PRESENT icon must be copied in and referenced by base name.
P4="$TMP/p4"; mkproj "$P4" 'name = "WithIcon"
identifier = "com.example.withicon"
icon = "assets/app.icns"'
mkdir -p "$P4/assets"; printf 'icns-bytes' > "$P4/assets/app.icns"
(cd "$P4" && run_to 180 "$WYN" build src/main.wyn --app >"$TMP/o4" 2>&1)
if [ "$HOST" = "Darwin" ]; then
    ic=$(plutil -extract CFBundleIconFile raw "$P4/WithIcon.app/Contents/Info.plist" 2>/dev/null)
    if [ -f "$P4/WithIcon.app/Contents/Resources/app.icns" ] && [ "$ic" = "app.icns" ]; then
        ok "a configured icon is copied into Resources/ and named in CFBundleIconFile"
    else
        bad "icon not installed (CFBundleIconFile='$ic', Resources: $(ls "$P4/WithIcon.app/Contents/Resources" 2>&1 | tr '\n' ' '))"
    fi
else
    ok "icon installation (skipped: host is $HOST)"
fi

# ---------------------------------------------------------------------------
# 7. WINDOWS. NOT RUN - there is no mingw on this machine. Asserted on the
#    EMITTED LINK FLAG via --app-plan, which executes the real packaging code
#    path. `-mwindows` selects PE subsystem WINDOWS; without it a console window
#    appears behind the GUI, which is the whole Windows half of this feature.
#
#    ENTRY POINT NOTE: the generated C emits a plain `int main`, and -mwindows
#    does NOT require WinMain - mingw-w64's crtexe.c defines both
#    mainCRTStartup and WinMainCRTStartup and both call main. So no shim is
#    emitted. This asserts the flag; the actual link is a Windows-CI concern.
# ---------------------------------------------------------------------------
P5="$TMP/p5"; mkproj "$P5" 'name = "WinApp"
identifier = "com.example.winapp"'
plan_w=$(cd "$P5" && run_to 120 "$WYN" build src/main.wyn --app-plan --app-target windows 2>&1)
if echo "$plan_w" | grep -q -- "-mwindows" &&
   echo "$plan_w" | grep -q "WinApp.exe" &&
   echo "$plan_w" | grep -q "windows-gui"; then
    ok "windows (flags only, NOT linked here): -mwindows + WinApp.exe"
else
    bad "windows plan lacks -mwindows or WinApp.exe: $(echo "$plan_w" | tr '\n' ' ' | head -c 250)"
fi

# A non-host target must REFUSE to link rather than emit a host binary wearing
# another platform's name. (On a Windows host this case is the identity, so the
# refusal is only asserted off-Windows.)
if [ "$HOST" != "MINGW"* ] && [ "$HOST" != "CYGWIN"* ]; then
    (cd "$P5" && run_to 120 "$WYN" build src/main.wyn --app --app-target windows >"$TMP/o5" 2>&1)
    if [ $? -ne 0 ] && grep -q "cannot be LINKED" "$TMP/o5"; then
        ok "--app --app-target windows REFUSES to link on a non-Windows host"
    else
        bad "a cross-target --app link should fail loudly (log: $(tr '\n' ' ' <"$TMP/o5" | head -c 200))"
    fi
else
    ok "cross-target refusal (skipped: host IS Windows)"
fi

# ---------------------------------------------------------------------------
# 8. LINUX. NOT RUN as an ELF - asserted on the GENERATED .desktop FILE, which
#    is the actual Linux deliverable. Terminal=false is the "no terminal window"
#    half; Exec must be absolute (a launcher does not run from the build dir) and
#    QUOTED when it contains a space, because Exec is parsed per the Desktop
#    Entry Spec and not by a shell - unquoted, "My Great App" is read as a
#    program plus two arguments and launches nothing.
# ---------------------------------------------------------------------------
P6="$TMP/p6"; mkproj "$P6" 'name = "My Linux App"
identifier = "com.example.linuxapp"
category = "public.app-category.graphics-design"'
(cd "$P6" && run_to 120 "$WYN" build src/main.wyn --app-plan --app-target linux >"$TMP/o6" 2>&1)
D="$P6/MyLinuxApp.desktop"
if [ -f "$D" ]; then
    errs=""
    grep -q '^Type=Application$' "$D"                || errs="$errs Type"
    grep -q '^Name=My Linux App$' "$D"               || errs="$errs Name"
    grep -q '^Terminal=false$' "$D"                  || errs="$errs Terminal=false"
    grep -q '^Categories=' "$D"                      || errs="$errs Categories"
    grep -qE '^Exec="?/' "$D"                        || errs="$errs Exec-not-absolute"
    grep -q '^Exec="' "$D"                           || errs="$errs Exec-not-quoted-despite-space"
    # An Apple UTI is meaningless to a freedesktop launcher; it must be mapped.
    grep -q '^Categories=.*public.app-category' "$D" && errs="$errs Categories-left-as-Apple-UTI"
    [ -z "$errs" ] && ok "linux (file only, NOT linked here): .desktop has Type/Name/Terminal=false/quoted-abs-Exec/mapped-Categories" \
                   || bad ".desktop entry problems:$errs -> $(tr '\n' ' ' <"$D")"
else
    bad "no .desktop entry generated at $D (log: $(tr '\n' ' ' <"$TMP/o6" | head -c 200))"
fi

# ---------------------------------------------------------------------------
# 9. THE REGRESSION THAT MATTERS MOST: a plain `wyn build`, with no --app and no
#    [app] section, must be COMPLETELY unchanged - a plain executable file, no
#    bundle, no .desktop, no .exe. Every existing user depends on this, and
#    packaging must never be inferred from "this looks like a GUI program".
# ---------------------------------------------------------------------------
P7="$TMP/p7"; mkproj "$P7" ''
(cd "$P7" && run_to 180 "$WYN" build src/main.wyn >"$TMP/o7" 2>&1)
rc7=$?
errs=""
[ $rc7 -eq 0 ]                       || errs="$errs build-failed(rc=$rc7)"
[ -f "$P7/src/main" ]                || errs="$errs no-plain-binary-at-src/main"
[ ! -e "$P7/src/main.app" ]          || errs="$errs stray-.app"
[ ! -e "$P7/main.app" ]              || errs="$errs stray-root-.app"
[ ! -e "$P7/src/main.exe" ]          || errs="$errs stray-.exe"
[ -z "$(find "$P7" -name '*.desktop' 2>/dev/null)" ] || errs="$errs stray-.desktop"
[ -z "$(find "$P7" -name 'Contents' 2>/dev/null)" ]  || errs="$errs stray-Contents-dir"
if [ -z "$errs" ]; then
    got=$(run_to 60 "$P7/src/main" 2>&1)
    [ "$got" = "app-ok" ] && ok "a PLAIN wyn build is unchanged: bare binary, no bundle, runs" \
                          || bad "plain build's binary printed '$got', want 'app-ok'"
else
    bad "a plain wyn build was changed by this feature:$errs"
fi

# An [app] section WITHOUT `bundle = true` and WITHOUT --app must also stay a
# plain build: the metadata section alone is not a trigger. (Present so a future
# "just bundle whenever [app] exists" shortcut fails here instead of surprising
# a user whose scripts consume the bare binary.)
P8="$TMP/p8"; mkproj "$P8" 'name = "NotTriggered"
identifier = "com.example.nt"'
(cd "$P8" && run_to 180 "$WYN" build src/main.wyn >"$TMP/o8" 2>&1)
if [ -f "$P8/src/main" ] && [ ! -e "$P8/NotTriggered.app" ] && [ ! -e "$P8/src/NotTriggered.app" ]; then
    ok "an [app] section alone does NOT trigger packaging (needs --app or bundle = true)"
else
    bad "[app] metadata alone triggered packaging - it must be opt-in"
fi

# ...but `bundle = true` IS the in-manifest opt-in, so a plain `wyn build`
# packages. This is the half of the spelling that lets a GUI project commit its
# packaging decision instead of relying on everyone remembering a flag.
P9="$TMP/p9"; mkproj "$P9" 'name = "Opted"
identifier = "com.example.opted"
bundle = true'
(cd "$P9" && run_to 180 "$WYN" build src/main.wyn >"$TMP/o9" 2>&1)
if [ "$HOST" = "Darwin" ]; then
    if [ -x "$P9/Opted.app/Contents/MacOS/Opted" ] &&
       [ "$(run_to 60 "$P9/Opted.app/Contents/MacOS/Opted" 2>&1)" = "app-ok" ]; then
        ok "[app] bundle = true packages a plain 'wyn build'"
    else
        bad "bundle = true did not package (log: $(tr '\n' ' ' <"$TMP/o9" | head -c 200))"
    fi
else
    ok "bundle = true opt-in (skipped: host is $HOST)"
fi

# ---------------------------------------------------------------------------
# 10. The cwd problem, which is the difference between "works from the terminal"
#     and "works when double-clicked". A Finder launch gives the app cwd "/", so
#     File.read("data.txt") relative to cwd breaks the moment it is a .app.
#     `[app] cwd = "resources"` opts into a launcher that chdirs first. Asserted
#     by RUNNING the bundled app from a different directory.
# ---------------------------------------------------------------------------
if [ "$HOST" = "Darwin" ]; then
    PA="$TMP/pa"; mkproj "$PA" 'name = "Assets"
identifier = "com.example.assets"
resources = "assets"
cwd = "resources"
bundle = true'
    mkdir -p "$PA/assets"; printf 'bundled-data' > "$PA/assets/data.txt"
    cat > "$PA/src/main.wyn" <<'WYN'
fn main() {
    var s = File.read("data.txt")
    print("asset=${s}")
}
WYN
    (cd "$PA" && run_to 180 "$WYN" build src/main.wyn >"$TMP/oa" 2>&1)
    if [ -f "$PA/Assets.app/Contents/Resources/data.txt" ]; then
        # Run from / - i.e. what Finder does. Without the launcher this prints an
        # empty asset because the file is not in cwd.
        got=$(cd / && run_to 60 "$PA/Assets.app/Contents/MacOS/Assets" 2>&1)
        if [ "$got" = "asset=bundled-data" ]; then
            ok "[app] cwd + resources: a bundled asset resolves when launched from another directory"
        else
            bad "bundled asset did not resolve from cwd=/ (got '$got')"
        fi
    else
        bad "[app] resources were not copied into Contents/Resources (log: $(tr '\n' ' ' <"$TMP/oa" | head -c 200))"
    fi
else
    ok "cwd/resources launcher (skipped: host is $HOST)"
fi

# ---------------------------------------------------------------------------
# 11. Hostile metadata must not be able to produce an invalid plist. `&` and `<`
#     in a name are XML-significant; unescaped, plutil rejects the file and the
#     app will not launch. Also pins that the identifier default is announced
#     rather than silently chosen, since two apps sharing a CFBundleIdentifier
#     are ONE app to LaunchServices.
# ---------------------------------------------------------------------------
if [ "$HOST" = "Darwin" ]; then
    PB="$TMP/pb"; mkproj "$PB" 'name = "Fish & <Chips>"'
    (cd "$PB" && run_to 180 "$WYN" build src/main.wyn --app >"$TMP/ob" 2>&1)
    BD=$(find "$PB" -maxdepth 1 -name '*.app' | head -1)
    if [ -n "$BD" ] && plutil -lint "$BD/Contents/Info.plist" >/dev/null 2>&1; then
        nm=$(plutil -extract CFBundleName raw "$BD/Contents/Info.plist" 2>/dev/null)
        if [ "$nm" = "Fish & <Chips>" ] && grep -qi "identifier" "$TMP/ob"; then
            ok "XML-significant characters in a name stay valid and are round-tripped"
        else
            bad "escaped name round-trip (CFBundleName='$nm'; default-id note present? $(grep -ci identifier "$TMP/ob"))"
        fi
    else
        bad "a name with & and <> produced an invalid or missing plist"
    fi
else
    ok "XML escaping (skipped: host is $HOST)"
fi

echo ""; echo "app-bundle: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
