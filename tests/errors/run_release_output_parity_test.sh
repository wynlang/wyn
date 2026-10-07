#!/bin/bash
# `wyn run` and `wyn run --release` must produce the SAME output for the same program.
#
# WHY THIS EXISTS ALONGSIDE run_slim_header_parity_test.sh. That gate is a TEXT check: it
# proves every runtime function codegen can emit is DECLARED in both headers. This one
# proves the programs actually agree. The two catch different halves, and the text gate's
# blind spot is why this file is needed:
#
#   it asked whether the exact token `"name"` appeared in the codegen sources, which only
#   matches a name that is a string literal all by itself. codegen emits most names from
#   inside longer literals - `emit("({ const char* __pms = map_to_string(")` - so those
#   were invisible. Twelve declarations were missing and the gate was green.
#
# WHAT WAS ACTUALLY BROKEN, measured before the fix, debug vs --release:
#
#   print(m) on a map        {"a": 1}   vs  the map POINTER as a decimal, exit 0
#   "${m}" on a map          {"a": 1}   vs  does not compile ('map_to_string' undeclared)
#   [1.5, 2.5].map(f)        2          vs  does not compile ('wyn_array_map_float')
#   ["a","b"].map(f)         2          vs  does not compile ('wyn_array_map_str')
#   [1.5, 2.5].filter(f)     1          vs  does not compile
#   ["a","bb"].filter(f)     1          vs  does not compile
#   [1.5, 2.5].reduce(f, 0)  4.0        vs  does not compile
#
# The silent one is the worst of them: a map printed its pointer under --release and exited
# 0, so a release build could not be trusted to show you your own data. The other six at
# least failed loudly - but they are `.map`/`.filter`/`.reduce` on the two most ordinary
# non-int element types, which is most of what anyone writes.
#
# EVERY ARM COMPARES THE TWO MODES TO EACH OTHER, and also to an expected value. Comparing
# only the two modes would pass if both were wrong in the same way; asserting the value
# alone in one mode is what let this class survive.
#
# Each mode gets its OWN temp directory. `wyn run` caches a built binary next to the source
# as `<file>.out`, so running two compilers - or two modes - over one path can silently
# re-run the first one's binary. That trap produced a "v1.21.0 printed it correctly"
# reading during this investigation that was simply the debug build being re-run.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# parity <label> <source> <expected-first-line>
parity(){
  d1=$(mktemp -d); d2=$(mktemp -d)
  printf '%b\n' "$2" > "$d1/p.wyn"; printf '%b\n' "$2" > "$d2/p.wyn"
  dbg=$(TMPDIR="$d1" perl -e 'alarm(90); exec @ARGV' -- "$WYNABS" run "$d1/p.wyn" 2>&1)
  rel=$(TMPDIR="$d2" perl -e 'alarm(90); exec @ARGV' -- "$WYNABS" run --release "$d2/p.wyn" 2>&1)
  clean(){ printf '%s' "$1" | sed 's/\x1b\[[0-9;]*m//g' | grep -vE 'Compiled in|^Warning|unused variable' | head -1; }
  dc=$(clean "$dbg"); rc=$(clean "$rel")
  rm -rf "$d1" "$d2"
  if [ "$dc" != "$3" ]; then bad "$1 - DEBUG gave [$dc] want [$3]"; return; fi
  if [ "$rc" != "$3" ]; then bad "$1 - RELEASE gave [$rc] want [$3]"; return; fi
  ok "$1 (both modes: $3)"
}

echo "=== debug and --release must agree (slim-header parity, by behaviour) ==="

# ------------------------------------------------- #468: slim declarations that
# disagreed with their definitions. Each of these compiled and ran in debug (the
# full header, definitions included, is pasted into the TU) and was broken only
# under --release, where the hand-maintained slim header is the prototype.
#
# THE SERVER ARM IS A `parity` ARM AND NOT A "does it build" ARM, ON PURPOSE.
# `wyn build --release` does NOT emit the slim header - it keeps wyn_runtime.h
# (src/main.c:2488-2501 says so in as many words: "a green `wyn build --release`
# proves nothing about it"). Only `wyn run --release` sets slim mode, which is what
# `parity` uses. A `wyn build --release` arm was written here first and PASSED with
# the three-parameter Http_respond declaration reinstated - it had never compiled
# against the slim header at all.
#
# The server body is guarded by `wyn_time_now() < 0`, which is false at runtime and
# which the C compiler cannot fold away (wyn_time_now is an external call), so the
# whole block is COMPILED AND LINKED - prototypes checked, symbols resolved - while
# the program still terminates instead of blocking in accept(). Verified by
# mutation: with Http_respond's declaration reverted to three parameters, debug
# still printed server-api-ok and --release failed to compile.
parity "the whole Http server API compiles and links under --release (#468)" \
  'fn main() {\n    if wyn_time_now() < 0 {\n        s = Http_listen(18791)\n        req = Http_accept(s)\n        print("${Http_method(req)} ${Http_path(req)}")\n        fd = Http_fd(req)\n        Http_respond(fd, 200, "text/plain", "hi")\n        Http_respond_json(fd, 200, "{}")\n        Http_close_client(fd)\n        Http_close_server(s)\n    }\n    print("server-api-ok")\n}' \
  'server-api-ok'

parity "Http_respond accepts its four arguments (content_type)" \
  'fn main() {\n    Http_respond(-1, 200, "text/plain", "x")\n    print("respond-ok")\n}' \
  'respond-ok'

parity "Http_method takes the request STRING, not an int handle" \
  'fn main() {\n    print(Http_method("GET|/a|body|7"))\n}' \
  'GET'

parity "Http_path takes the request STRING, not an int handle" \
  'fn main() {\n    print(Http_path("GET|/a|body|7"))\n}' \
  '/a'

# http_status/http_error/http_clear_headers are `static inline` in wyn_runtime.h,
# so there is no archive symbol: declaring them in the slim header as ordinary
# functions made this exact program fail at LINK under --release with
# `Undefined symbols: _http_status`, while debug printed 0.
parity "http_status() links under --release (static inline alias, no archive symbol)" \
  'fn main() {\n    print(http_status())\n}' \
  '0'

parity "http_error() links under --release" \
  'fn main() {\n    e = http_error()\n    print("err-ok")\n}' \
  'err-ok'

# Db_exec is `int`; the slim header said `long long`, so the caller read 64 bits
# out of a 32-bit return value.
parity "Db_exec returns its int, not 64 bits of register" \
  'fn main() {\n    print(Db_exec(0, "select 1"))\n}' \
  '-1'

# wyn_time_now was declared `long` in BOTH headers against a `long long`
# definition - the same width on LP64, half of it on Windows.
parity "wyn_time_now() is wide enough for a millisecond epoch" \
  'fn main() {\n    print(wyn_time_now() > 1700000000)\n}' \
  'true'


# --------------------------------------------------- the silently-wrong one
parity "print(map) renders the map, not its pointer" \
  'fn main() {\n    m = {"a": 1}\n    print(m)\n}' \
  '{"a": 1}'

parity "\${map} interpolation renders the map" \
  'fn main() {\n    m = {"a": 1}\n    print("${m}")\n}' \
  '{"a": 1}'

parity "print(map) with several value kinds" \
  'fn main() {\n    m = {"n": 1}\n    m.set_float("f", 2.5)\n    print("${m.len()}")\n}' \
  '2'

# ------------------------------------- .map / .filter / .reduce on non-int element types
parity "float array .map" \
  'fn main() {\n    xs = [1.5, 2.5]\n    print(xs.map((v) => v * 2.0).len())\n}' \
  '2'

parity "string array .map" \
  'fn main() {\n    xs = ["a", "b"]\n    print(xs.map((s) => s.upper()).len())\n}' \
  '2'

parity "float array .filter" \
  'fn main() {\n    xs = [1.5, 2.5]\n    print(xs.filter((v) => v > 2.0).len())\n}' \
  '1'

parity "string array .filter" \
  'fn main() {\n    xs = ["a", "bb"]\n    print(xs.filter((s) => s.len() > 1).len())\n}' \
  '1'

parity "float array .reduce" \
  'fn main() {\n    xs = [1.5, 2.5]\n    print(xs.reduce((a, b) => a + b, 0.0))\n}' \
  '4.0'

# ------------------------------- the int forms, which already worked - the control group
parity "int array .map (control - already declared)" \
  'fn main() {\n    xs = [1, 2, 3]\n    print(xs.map((n) => n * 2).len())\n}' \
  '3'

parity "int array .filter (control)" \
  'fn main() {\n    xs = [1, 2, 3]\n    print(xs.filter((n) => n > 1).len())\n}' \
  '2'

parity "int array .reduce (control)" \
  'fn main() {\n    xs = [1, 2, 3]\n    print(xs.reduce((a, b) => a + b, 0))\n}' \
  '6'

parity "int array .sort (control)" \
  'fn main() {\n    xs = [3, 1, 2]\n    print(xs.sort().len())\n}' \
  '3'

echo "--- $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
