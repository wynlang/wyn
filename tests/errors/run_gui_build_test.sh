#!/bin/bash
# `wyn build` links SDL for a Gui program, so a built GUI binary actually works (2026-08).
#
# `wyn run app.wyn` compiled the runtime from source with -DWYN_USE_GUI + SDL and worked.
# `wyn build app.wyn` linked the PRECOMPILED runtime/libwyn_rt.a, which is built WITHOUT
# that define and so already contains the STUB Gui implementation whose every call prints
#
#     Error: Gui module requires SDL2.
#
# and returns nothing. So a built GUI binary died after one frame even with SDL2 installed:
# you could develop a game in Wyn and never ship one.
#
# The fix: a Gui program forces the from-source build path (the prebuilt archive's flags
# do not match what it needs - the same shape as the -fPIC library-mode problem), and puts
# -DWYN_USE_GUI plus SDL's cflags/libs on the COMPILE line (gui.h does
# `#include <SDL2/SDL.h>`, so a link-only flag is not enough).
#
# SKIPS cleanly where SDL2 is not installed - it asserts the LINKING, which cannot be
# tested without the library. That is honest for CI images without SDL rather than a
# false pass.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# Is SDL2 available to link against? If not, this test cannot run meaningfully.
if ! pkg-config --exists sdl2 2>/dev/null && [ ! -e /usr/lib/libSDL2.so ] \
     && [ ! -e /usr/lib/x86_64-linux-gnu/libSDL2.so ] \
     && [ ! -e /opt/homebrew/lib/libSDL2.dylib ] && [ ! -e /usr/local/lib/libSDL2.dylib ]; then
  echo "  skip  SDL2 not installed - cannot test GUI linking"
  echo ""
  echo "gui-build: 0 pass, 0 fail (skipped)"
  exit 0
fi

cat > "$TMP/game.wyn" <<'EOF'
fn main() {
    var w = Gui.create("t", 100, 80)
    // In a real display this opens a window; headless it returns 0. Either way the
    // point of the test is that the SDL SYMBOLS are LINKED, checked with nm below.
    Gui.clear(0, 0, 0)
    Gui.color(255, 0, 0)
    Gui.rect(0, 0, 10, 10)
    Gui.present()
    Gui.destroy()
    print("built")
}
EOF

out=$(cd "$TMP" && "$WYN_ABS" build game.wyn 2>&1); code=$?
if [ $code -eq 0 ] && [ -x "$TMP/game" ]; then
  ok "a Gui program builds to a binary"
else
  bad "build failed"; printf '%s\n' "$out" | grep -iE 'error|SDL' | head -3 | sed 's/^/        /'
fi

# The real assertion: SDL is actually linked in, not the stub. nm on the binary must show
# an SDL symbol. This is what a stub-linked binary lacks.
if command -v nm >/dev/null 2>&1 && [ -x "$TMP/game" ]; then
  if nm "$TMP/game" 2>/dev/null | grep -q 'SDL_CreateWindow'; then
    ok "the built binary links SDL_CreateWindow (not the stub)"
  else
    bad "no SDL in the binary - it linked the stub runtime again"
  fi
fi

# And it must not print the stub's error at runtime.
if [ -x "$TMP/game" ]; then
  rout=$("$TMP/game" 2>&1 || true)
  if printf '%s' "$rout" | grep -q "requires SDL2"; then
    bad "the built binary printed 'requires SDL2' - stub linked"
  else
    ok "the built binary does not hit the SDL2-missing stub"
  fi
fi

# A NON-gui program must still take the fast prebuilt-runtime path unchanged.
cat > "$TMP/plain.wyn" <<'EOF'
fn main() { print("plain ${2 + 2}") }
EOF
out=$(cd "$TMP" && "$WYN_ABS" build plain.wyn 2>&1); code=$?
if [ $code -eq 0 ] && [ -x "$TMP/plain" ]; then
  rout=$("$TMP/plain" 2>&1)
  if printf '%s' "$rout" | grep -q '^plain 4$'; then
    ok "a non-Gui build is unaffected"
  else
    bad "non-Gui program wrong output"; printf '%s\n' "$rout" | head -2 | sed 's/^/        /'
  fi
  if nm "$TMP/plain" 2>/dev/null | grep -q 'SDL_CreateWindow'; then
    bad "a non-Gui binary pulled in SDL it does not need"
  else
    ok "a non-Gui binary does not link SDL"
  fi
else
  bad "non-Gui build failed"
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "gui-build: $PASS pass, 0 fail"
  exit 0
fi
echo "gui-build: $PASS pass, $FAIL fail"
exit 1
