#!/bin/bash
# THE HTTP CLIENT MUST BE ABLE TO REPORT WHETHER THE REQUEST SUCCEEDED.
#
# THE BUG (issue #508). `docs/stdlib/api-reference.md` published an `Http.get` ->
# `Http.status` / `Http.body` / `Http.header` response API. Wyn has TWO HTTP client
# APIs with overlapping names: a string one in `src/wyn_runtime.h` (`http_get()`
# returns the response BODY) and a struct one in `src/net_advanced.c`
# (`Http_get()` returns `HttpResponse*`). `Http.get` was bound to the first and
# `status`/`header` to the second - and no Wyn expression can produce an
# `HttpResponse*`, so those two read a `string`'s bytes AS that struct:
#
#   Http.status(resp)        -> 1347634514, which is 0x50545448: the ASCII bytes
#                               "HTTP" off the front of the response. Exit 0.
#   Http.header(resp, name)  -> SIGSEGV (strstr on resp->headers, a pointer read
#                               out of the string's characters).
#
# `Http.status` was the worse of the two: a silently wrong answer at exit 0, which
# this project treats as worse than a crash. It also meant the documented way to ask
# whether an HTTP request succeeded did not exist - there was NO reachable status code.
#
# THE FIX IS A BINDING, NOT A DELETION. The string API already tracked both: the
# runtime has kept `http_last_status` / `http_last_error` since the HTTPS work, reachable
# through the zero-argument `http_status()` and `http_error()`. So `Http.status` and
# `Http.error` now lower to THOSE (src/types.c, wyn_ns_renames) and are registered with
# arity 0 (src/checker_builtins.c). They report the last request on this thread, because
# that is the only status the string API ever has.
#
# `Http.header` is REMOVED with no replacement, and that is the honest outcome: by the
# time `http_get()` returns, the headers have been parsed and dropped - the body is all
# that is left - so there is nothing for it to look in. It now fails `wyn check` with
# "unknown method", which is a diagnostic the reader can act on.
#
# WHY A LIVE SERVER AND NOT A UNIT ASSERTION. The old behaviour returned an int at
# exit 0, so any gate that merely checks "Http.status() compiles and returns an int"
# passes against the bug. 1347634514 is an int. The gate has to compare against a
# status the server CHOSE, and it has to be a status other than 200 - a 200-only gate
# cannot tell a real status code from a constant. Hence 404 and 503 as well.
#
# ARMS
#   1  the server builds and negotiates a port
#   2  the client builds                                   (binding + C linkage)
#   3  Http.get returns the body                            (unchanged, non-regression)
#   4  Http.status() is 200 on a request the server answered 200
#   5  Http.status() is 404 when the server chose 404        (not a constant)
#   6  Http.status() is 503 when the server chose 503        (not 404 either)
#   7  Http.status() is never 1347634514                     (the old garbage, by value)
#   8  on a REFUSED connection Http.status() is 0 and Http.error() is non-empty
#      - the failure channel, which is the point of the whole issue
#   9  Http.status(resp) - the OLD published call - is now REJECTED by wyn check
#  10  Http.header(resp, name) is now REJECTED by wyn check
#
# Arms 9 and 10 are the ones that fail if the registration is reverted; arms 4-6 are
# the ones that fail if the lowering is reverted. Both halves of the fix are covered,
# which was verified by reverting each in turn.
#
# Every wait is bounded (perl alarm; stock macOS has no `timeout`) and the server is
# killed on every exit path - a stray server has kernel-panicked this box twice. The
# port is NEGOTIATED, never hard-coded: tests/errors/run_test_port_hygiene_test.sh
# gates that, and a fixed port turns a sibling `make test` into a false red.
set -uo pipefail
set +m 2>/dev/null

WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d)
S_SRC="$TMP/srv.wyn"; S_BIN="$TMP/srv.out"; S_LOG="$TMP/srv.log"; S_PID=""
C_SRC="$TMP/cli.wyn"; C_BIN="$TMP/cli.out"

cleanup() {
    [ -n "$S_PID" ] && kill -9 "$S_PID" 2>/dev/null
    pkill -9 -f "^$S_BIN" 2>/dev/null
    rm -rf "$TMP"
}
trap cleanup EXIT
PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

echo "=== HTTP client status/error reachability gate (#508) ==="

read_reported_port() {
    local i p
    for i in $(seq 1 300); do
        p=$(sed -n "s/^$2 \([0-9][0-9]*\)\$/\1/p" "$1" 2>/dev/null | head -1)
        if [ -n "$p" ]; then printf '%s' "$p"; return 0; fi
        sleep 0.1
    done
    return 1
}

PORT_BASE=$(( 23000 + (RANDOM % 2000) ))

# The server answers a DIFFERENT status per path, so the client can prove the value
# tracks what the server sent rather than being any fixed number.
cat > "$S_SRC" <<EOF
fn main() -> int {
    var port = $PORT_BASE
    var server = -1
    var tries = 0
    while tries < 200 {
        server = Http.serve(port)
        if server > 0 { break }
        port = port + 1
        tries = tries + 1
    }
    if server <= 0 { return 7 }
    println("ready \${port}")
    var n = 0
    while n < 60 {
        var req = Http.accept(server)
        if req != "" {
            var path = Http.path(req)
            var fd = Http.fd(req)
            if path == "/missing" {
                Http.respond(fd, 404, "text/plain", "NOPE")
            } else if path == "/broken" {
                Http.respond(fd, 503, "text/plain", "DOWN")
            } else {
                Http.respond(fd, 200, "text/plain", "BODY-OK")
            }
            Http.close_client(fd)
        }
        n = n + 1
    }
    return 0
}
EOF

if ! perl -e 'alarm(150); exec @ARGV' -- "$WYN" build "$S_SRC" -o "$S_BIN" > "$TMP/builds.log" 2>&1; then
    bad "status server builds"
    sed -n '1,25p' "$TMP/builds.log"
    echo ""; echo "http-client-status: $PASS pass, $FAIL fail"; exit 1
fi

"$S_BIN" > "$S_LOG" 2>&1 &
S_PID=$!
disown "$S_PID" 2>/dev/null
SPORT=$(read_reported_port "$S_LOG" ready) || SPORT=""
if [ -z "$SPORT" ]; then
    bad "server negotiates a port (walked 200 from $PORT_BASE, none bound)"
    sed -n '1,20p' "$S_LOG"
    echo ""; echo "http-client-status: $PASS pass, $FAIL fail"; exit 1
fi
ok "status server builds and negotiates a port"

# A port nothing listens on, for the failure arm. Port 1 is privileged and unused;
# a connect() to it on loopback is refused immediately, with no DNS and no network.
cat > "$C_SRC" <<EOF
fn main() {
    var body = Http.get("http://127.0.0.1:$SPORT/ok")
    println("body=\${body}")
    println("ok_status=\${Http.status()}")

    Http.get("http://127.0.0.1:$SPORT/missing")
    println("missing_status=\${Http.status()}")

    Http.get("http://127.0.0.1:$SPORT/broken")
    println("broken_status=\${Http.status()}")

    Http.get("http://127.0.0.1:1/refused")
    println("refused_status=\${Http.status()}")
    println("refused_error=\${Http.error()}")
}
EOF

if ! perl -e 'alarm(150); exec @ARGV' -- "$WYN" build "$C_SRC" -o "$C_BIN" > "$TMP/buildc.log" 2>&1; then
    bad "client using Http.status()/Http.error() builds"
    sed -n '1,25p' "$TMP/buildc.log"
    echo ""; echo "http-client-status: $PASS pass, $FAIL fail"; exit 1
fi
ok "client using Http.status()/Http.error() builds"

OUT=$(perl -e 'alarm(60); exec @ARGV' -- "$C_BIN" 2>&1)
RC=$?
get(){ printf '%s\n' "$OUT" | sed -n "s/^$1=//p" | head -1; }

if [ "$RC" -ne 0 ]; then
    bad "client exits 0 (rc=$RC)"
    printf '%s\n' "$OUT" | sed -n '1,20p'
else
    ok "client exits 0"
fi

[ "$(get body)" = "BODY-OK" ] \
    && ok "Http.get still returns the body" \
    || bad "Http.get still returns the body (got [$(get body)])"

[ "$(get ok_status)" = "200" ] \
    && ok "Http.status() is 200 on an answered request" \
    || bad "Http.status() is 200 on an answered request (got [$(get ok_status)])"

[ "$(get missing_status)" = "404" ] \
    && ok "Http.status() is 404 when the server chose 404" \
    || bad "Http.status() is 404 when the server chose 404 (got [$(get missing_status)])"

[ "$(get broken_status)" = "503" ] \
    && ok "Http.status() is 503 when the server chose 503" \
    || bad "Http.status() is 503 when the server chose 503 (got [$(get broken_status)])"

# By value, because this is the exact number the bug produced and it is the one
# symptom a reader of #508 will look for.
if printf '%s\n' "$OUT" | grep -q '1347634514'; then
    bad "no status is the old type-confusion value 1347634514"
else
    ok "no status is the old type-confusion value 1347634514"
fi

[ "$(get refused_status)" = "0" ] \
    && ok "Http.status() is 0 on a refused connection" \
    || bad "Http.status() is 0 on a refused connection (got [$(get refused_status)])"

[ -n "$(get refused_error)" ] \
    && ok "Http.error() carries a reason on a refused connection [$(get refused_error)]" \
    || bad "Http.error() carries a reason on a refused connection (empty)"

# --- 9/10. the two calls that cannot work must not type-check ------------------
# They are checked, not built: the point is that the reader is told at check time
# instead of getting a garbage int or a SIGSEGV at run time.
cat > "$TMP/old_status.wyn" <<'EOF'
fn main() {
    var resp = Http.get("http://127.0.0.1:1/x")
    println("${Http.status(resp)}")
}
EOF
if perl -e 'alarm(60); exec @ARGV' -- "$WYN" check "$TMP/old_status.wyn" > "$TMP/os.log" 2>&1; then
    bad "Http.status(resp) is rejected (it type-checked)"
else
    ok "Http.status(resp) is rejected"
fi

cat > "$TMP/old_header.wyn" <<'EOF'
fn main() {
    var resp = Http.get("http://127.0.0.1:1/x")
    println(Http.header(resp, "Content-Type"))
}
EOF
if perl -e 'alarm(60); exec @ARGV' -- "$WYN" check "$TMP/old_header.wyn" > "$TMP/oh.log" 2>&1; then
    bad "Http.header(resp, name) is rejected (it type-checked)"
else
    ok "Http.header(resp, name) is rejected"
fi

echo ""
echo "http-client-status: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
