#!/bin/bash
# A SERVER MUST SURVIVE ITS SECOND REQUEST, AND THE SHOWCASE EXAMPLE MUST SERVE ONE.
#
# THE BUGS (#476), both measured before the fix:
#
# 1. `Http.free(req)` aborted the process. The checker registers Http_free with an
#    arity and a return type but NO PARAMETER TYPE, so passing the request STRING from
#    Http.accept type-checked and compiled; Http_free then read 8 bytes past the start
#    of a char* as a pointer and called free() on it. A server answered request 1 and
#    died of SIGSEGV (exit 139) before printing anything after it - it never reached
#    request 2. `wyn check` reported no errors.
#
# 2. `examples/29_http_server.wyn`, titled "Simple HTTP Server", never called accept.
#    It listened, slept one second, closed and exited, and its own comment said "Real
#    server would need accept() and proper HTTP parsing". The one artefact a reader
#    copies to learn the feature did not demonstrate it.
#
# ONE REQUEST IS THE CASE THAT ALREADY PASSED, which is why every arm here issues TWO
# and checks the second - a gate that sends one request would have been green
# throughout.
#
# The example is driven through PORT rather than its default 8080: a test may not bind
# a fixed port (tests/errors/run_test_port_hygiene_test.sh gates that, and a clash
# turns a sibling `make test` into a false red), and driving the real example file is
# the only way this gate can claim anything about the example.
set -uo pipefail
set +m 2>/dev/null

WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
ROOT="$(pwd)"
TMP=$(mktemp -d)
EX_BIN="$TMP/example.out"; EX_PID=""
FR_BIN="$TMP/freed.out";   FR_PID=""

cleanup() {
    [ -n "$EX_PID" ] && kill -9 "$EX_PID" 2>/dev/null
    [ -n "$FR_PID" ] && kill -9 "$FR_PID" 2>/dev/null
    pkill -9 -f "^$EX_BIN" 2>/dev/null
    pkill -9 -f "^$FR_BIN" 2>/dev/null
    rm -rf "$TMP"
}
trap cleanup EXIT
PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

echo "=== an HTTP server must serve more than one request (#476) ==="

# A free port from the kernel, then handed to the server to bind. There is a race
# between closing this and the server binding, which is why the caller retries.
free_port() {
    python3 -c '
import socket
s = socket.socket(); s.bind(("127.0.0.1", 0))
print(s.getsockname()[1]); s.close()'
}

read_reported_port() {  # $1 log  $2 leading word
    local i p
    for i in $(seq 1 300); do
        p=$(sed -n "s/^$2 \\([0-9][0-9]*\\)\$/\\1/p" "$1" 2>/dev/null | head -1)
        if [ -n "$p" ]; then printf '%s' "$p"; return 0; fi
        sleep 0.1
    done
    return 1
}

wait_for_line() {  # $1 log  $2 grep pattern
    local i
    for i in $(seq 1 100); do
        grep -q "$2" "$1" 2>/dev/null && return 0
        sleep 0.1
    done
    return 1
}

# --- 1. a server that frees the request record survives two requests ----------
# This is the exact shape that used to die: respond, then Http.free(req), then loop.
cat > "$TMP/freed.wyn" <<'EOF'
fn main() -> int {
    // WALK from the suggested port rather than trusting it. The kernel handed the
    // harness a free port, but it was released before this process could bind it, so
    // a sibling can take it in between - and tests/errors/run_test_port_hygiene_test.sh
    // requires a walk for exactly that reason. The bound port is reported back so the
    // harness never assumes which one it got.
    var port = Env.get("PORT").to_int()
    var server = -1
    var tries = 0
    while tries < 200 {
        server = Http.serve(port)
        if server > 0 { break }
        port = port + 1
        tries = tries + 1
    }
    if server <= 0 { return 7 }
    println("ready ${port}")
    var n = 0
    while n < 5 {
        var req = Http.accept(server)
        if req == "" { break }
        var conn = Http.fd(req)
        Http.respond(conn, 200, "text/plain", "answer-${n}")
        Http.close_client(conn)
        Http.free(req)
        println("served ${n}")
        n = n + 1
    }
    return 0
}
EOF
if ! perl -e 'alarm(180); exec @ARGV' -- "$WYN" build "$TMP/freed.wyn" -o "$FR_BIN" > "$TMP/freed.build" 2>&1; then
    bad "a server calling Http.free(req) builds"
    grep -iE 'error' "$TMP/freed.build" | head -3 | sed 's/^/          /'
else
    ok "a server calling Http.free(req) builds"
    FR_PORT=""
    P=$(free_port)
    PORT="$P" "$FR_BIN" > "$TMP/freed.log" 2>&1 &
    FR_PID=$!
    disown "$FR_PID" 2>/dev/null
    FR_PORT=$(read_reported_port "$TMP/freed.log" ready) || FR_PORT=""
    if [ -z "$FR_PORT" ]; then
        bad "Http.free server bound a port"
        sed -n '1,10p' "$TMP/freed.log"
    else
        r1=$(perl -e 'alarm(15); exec @ARGV' -- curl -s -m 10 "http://127.0.0.1:$FR_PORT/one" 2>/dev/null)
        r2=$(perl -e 'alarm(15); exec @ARGV' -- curl -s -m 10 "http://127.0.0.1:$FR_PORT/two" 2>/dev/null)
        if [ "$r1" = "answer-0" ]; then ok "request 1 is answered"
        else bad "request 1 is answered (got [$r1])"; fi
        # The one that used to fail.
        if [ "$r2" = "answer-1" ]; then ok "request 2 is answered (Http.free(req) no longer aborts)"
        else
            bad "request 2 is answered (got [$r2])"
            sed -n '1,12p' "$TMP/freed.log" | sed 's/^/          /'
        fi
        if kill -0 "$FR_PID" 2>/dev/null; then ok "server is still alive after two requests"
        else
            bad "server is still alive after two requests"
            grep -iE 'segm|fault|panic|abort' "$TMP/freed.log" | head -2 | sed 's/^/          /'
        fi
        kill -9 "$FR_PID" 2>/dev/null; FR_PID=""
    fi
fi

# --- 2. the showcase example actually serves requests ------------------------
EX_SRC="$ROOT/examples/29_http_server.wyn"
if [ ! -f "$EX_SRC" ]; then
    bad "examples/29_http_server.wyn exists"
elif ! perl -e 'alarm(180); exec @ARGV' -- "$WYN" build "$EX_SRC" -o "$EX_BIN" > "$TMP/ex.build" 2>&1; then
    bad "examples/29_http_server.wyn builds"
    grep -iE 'error' "$TMP/ex.build" | head -3 | sed 's/^/          /'
else
    ok "examples/29_http_server.wyn builds"
    EX_PORT=""
    for _try in 1 2 3; do
        P=$(free_port)
        PORT="$P" "$EX_BIN" > "$TMP/ex.log" 2>&1 &
        EX_PID=$!
        disown "$EX_PID" 2>/dev/null
        if wait_for_line "$TMP/ex.log" 'Listening on'; then EX_PORT="$P"; break; fi
        kill -9 "$EX_PID" 2>/dev/null; EX_PID=""
    done
    if [ -z "$EX_PORT" ]; then
        bad "the example binds the port it is given"
        sed -n '1,10p' "$TMP/ex.log"
    else
        e1=$(perl -e 'alarm(15); exec @ARGV' -- curl -s -m 10 "http://127.0.0.1:$EX_PORT/" 2>/dev/null)
        e2=$(perl -e 'alarm(15); exec @ARGV' -- curl -s -m 10 "http://127.0.0.1:$EX_PORT/health" 2>/dev/null)
        e3=$(perl -e 'alarm(15); exec @ARGV' -- curl -s -m 10 "http://127.0.0.1:$EX_PORT/nope" 2>/dev/null)
        if [ "$e1" = "Hello from Wyn!" ]; then ok "the example answers / with its advertised body"
        else bad "the example answers / with its advertised body (got [$e1])"; fi
        if [ "$e2" = "ok" ]; then ok "the example answers /health on a SECOND request"
        else bad "the example answers /health on a SECOND request (got [$e2])"; fi
        if [ "$e3" = "404 not found" ]; then ok "the example 404s an unknown path (third request)"
        else bad "the example 404s an unknown path (got [$e3])"; fi
        if kill -0 "$EX_PID" 2>/dev/null; then ok "the example is still alive after three requests"
        else
            bad "the example is still alive after three requests"
            sed -n '1,12p' "$TMP/ex.log" | sed 's/^/          /'
        fi
        kill -9 "$EX_PID" 2>/dev/null; EX_PID=""
    fi
fi

echo ""
echo "http-two-requests: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
