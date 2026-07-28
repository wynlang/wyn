#!/usr/bin/env bash
# `wyn add` integration test: add libm from the curated registry, then build+run
# a program using the generated bindings - proving add → bindgen → [ffi] link →
# compile → run end to end. Uses libm (always available; linked by default).
set -u
WYN_BIN="${WYN:-./wyn}"
# resolve to an absolute path so it works from the temp dir
case "$WYN_BIN" in /*) ;; *) WYN_BIN="$(pwd)/$WYN_BIN" ;; esac
ROOT="$(pwd)"
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
cd "$work"
export WYN_ROOT="$ROOT"

"$WYN_BIN" add m >/dev/null 2>&1 || { echo "cpkg: FAIL (wyn add m)"; exit 1; }
if ! grep -q 'extern fn sqrt' packages/m/m.wyn 2>/dev/null; then
    # Say WHAT bindgen produced, not just that sqrt is absent. This failed on
    # ubuntu-latest while generating 160 bindings (sqrt included) on macOS, and
    # "no sqrt binding" alone gave no way to tell whether the header was not
    # found, parsed to nothing, or parsed to something unexpected - glibc's
    # math.h differs substantially from the macOS SDK's. This suite had never
    # been gated on Linux before, so nobody had seen it.
    echo "cpkg: FAIL (no sqrt binding)"
    if [ -f packages/m/m.wyn ]; then
        echo "  m.wyn exists: $(wc -l < packages/m/m.wyn) lines, $(grep -c 'extern fn' packages/m/m.wyn) extern fns"
        echo "  first 15 bindings:"; grep 'extern fn' packages/m/m.wyn | head -15 | sed 's/^/    /'
    else
        echo "  packages/m/m.wyn does NOT exist - bindgen produced no file at all"
        ls -la packages/m/ 2>/dev/null | sed 's/^/    /' | head -8
    fi
    exit 1
fi
grep -q '\[ffi\]' wyn.toml 2>/dev/null || { echo "cpkg: FAIL (no [ffi] in wyn.toml)"; exit 1; }
grep -q 'libs = "m"' wyn.toml 2>/dev/null || { echo "cpkg: FAIL (libm not linked)"; exit 1; }

# Use `import m` (NOT hand-copied externs) - proves add -> import resolution ->
# [ffi] link -> build -> run works as a whole.
cat > app.wyn <<'WYN'
import m
fn main() { println("${sqrt(169.0)}") }
WYN
out="$("$WYN_BIN" build app.wyn >/dev/null 2>&1 && ./app 2>&1)"
if [ "$out" != "13.0" ]; then echo "cpkg: FAIL (got '$out', want 13.0)"; exit 1; fi

# --- W4 TUI: `wyn add` with no name ---
# Non-interactive (piped stdin, not a TTY) must fall back to the plain list.
list_out="$(echo '' | "$WYN_BIN" add 2>&1)"
echo "$list_out" | grep -q 'Available C packages' || { echo "cpkg: FAIL (no-name non-TTY should list)"; exit 1; }
echo "$list_out" | grep -q '^  m ' || { echo "cpkg: FAIL (list missing m)"; exit 1; }
# --list forces the list even on a TTY.
"$WYN_BIN" add --list 2>&1 | grep -q 'Available C packages' || { echo "cpkg: FAIL (--list)"; exit 1; }

# Interactive picker over a pty (if python3 is available): pick #1, confirm, and
# verify it pulls the package. Skipped cleanly without python3.
if command -v python3 >/dev/null 2>&1; then
  tproj="$(mktemp -d)"
  WYN_BIN="$WYN_BIN" ROOT="$ROOT" PROJ="$tproj" python3 - <<'PY'
import os, pty, time, select
inp=[b"1\ny\n"]
pid, fd = pty.fork()
if pid == 0:
    os.environ["WYN_ROOT"]=os.environ["ROOT"]
    os.chdir(os.environ["PROJ"])
    os.execv(os.environ["WYN_BIN"], [os.environ["WYN_BIN"], "add"])
out=b""; step=0; t0=time.time()
while time.time()-t0 < 20:
    r,_,_=select.select([fd],[],[],0.5)
    if r:
        try: d=os.read(fd,4096)
        except OSError: break
        if not d: break
        out+=d
    elif step<len(inp):
        os.write(fd, inp[step]); step+=1
try: os.close(fd)
except: pass
PY
  if grep -q 'extern fn sqrt' "$tproj/packages/m/m.wyn" 2>/dev/null; then
    echo "  ok  TUI picker adds a package (pty)"
  else
    echo "cpkg: FAIL (TUI picker did not add m)"; rm -rf "$tproj"; exit 1
  fi
  rm -rf "$tproj"
fi
echo "cpkg: PASS"
