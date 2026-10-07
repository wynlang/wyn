#!/bin/bash
# A REMOTE CLIENT MUST NOT BE ABLE TO NAME A FILE DESCRIPTOR.
#
# THE BUG (issue #467). Http_accept / Http_read_request hand user code the string
# "METHOD|PATH|BODY|FD" and the documented way to get the descriptor out is
#
#     fd = req.split_at("|", 3).to_int()
#
# The path and the body are attacker-controlled and were interpolated UNESCAPED:
#
#     snprintf(result, 16384, "%s|%s|%s|%d", method, path, body, client_fd);
#
# so `GET /a|b|1 HTTP/1.0` shifted every later field and made that idiom yield 1 -
# the server's own stdout. Naming another live connection's descriptor instead sends
# this response to a DIFFERENT client. Unauthenticated, first line of the request.
#
# THE FIX. The descriptor leaves the payload. Field 3 carries an unguessable
# capability token; the descriptor lives only in a runtime table, and respond /
# read_request / close_client resolve a token through it. A forged, stale or
# out-of-range token resolves to -1 and the call is a no-op. The final field is also
# parsed from the RIGHT, so a body containing '|' can no longer shift it - a body
# legitimately may contain one and must not be mangled to make a delimiter safe.
#
# Cases:
#   1  the DOCUMENTED idiom still serves a normal request          (compatibility)
#   2  `GET /a|b|1` does not write the response to the server's own stdout
#   3  nor does any small descriptor an attacker might name (0,1,2,3,4,5)
#   4  a request body containing '|' survives intact AND is still answered
#   5  a forged high-numbered token is refused, and the server survives it
#   6  the server still answers a real request after all of the above
#
# Case 1 is load-bearing, not a courtesy: the whole design rests on the published
# string shape continuing to work, so a fix that silently broke it would be caught
# here rather than by a reader of the blog.
#
# THE OBSERVABLE for cases 2/3/5 is the server's OWN STDOUT. If an injected
# descriptor is honoured, the response goes there, so the log acquires an
# "HTTP/1.1 200" line that a correct server never prints. That is a positive
# observable rather than an absence - the log is checked for a marker the BUG
# produces, not for the absence of one the fix produces.
#
# Every wait is bounded (perl alarm; stock macOS has no `timeout`) and the server is
# killed on every exit path - a stray server has kernel-panicked this box twice.
# The port is NEGOTIATED, never hard-coded: tests/errors/run_test_port_hygiene_test.sh
# gates that, and a fixed port turns a sibling `make test` into a false red.
set -uo pipefail
set +m 2>/dev/null

WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d)
SRV_SRC="$TMP/srv.wyn"
SRV_BIN="$TMP/srv.out"
SRV_PID=""
SRV_LOG="$TMP/srv.log"

cleanup() {
    [ -n "$SRV_PID" ] && kill -9 "$SRV_PID" 2>/dev/null
    pkill -9 -f "^$SRV_BIN" 2>/dev/null
    rm -rf "$TMP"
}
trap cleanup EXIT
PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

echo "=== HTTP response-descriptor injection gate (#467) ==="

# The server reports the port it actually bound; the harness reads it back rather
# than assuming. 300 x 0.1s, so a slow build cannot race the first request.
read_reported_port() {
    local i p
    for i in $(seq 1 300); do
        p=$(sed -n "s/^$2 \([0-9][0-9]*\)\$/\1/p" "$1" 2>/dev/null | head -1)
        if [ -n "$p" ]; then printf '%s' "$p"; return 0; fi
        sleep 0.1
    done
    return 1
}

# Deliberately the PUBLISHED pattern, pipe-split and all: it is what the book,
# three blog posts, four docs pages and two sample apps tell a reader to write, so
# it is the shape under test. `Http.req_body` is used for the body because that is
# the accessor that knows the record's field boundaries.
PORT_BASE=$(( 21000 + (RANDOM % 2000) ))
cat > "$SRV_SRC" <<EOF
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
    while n < 24 {
        var req = Http.accept(server)
        var fd = req.split_at("|", 3).to_int()
        var b = Http.req_body(req)
        println("REQ path=[\${Http.path(req)}] body=[\${b}]")
        Http.respond(fd, 200, "text/plain", "PAYLOAD-OK")
        Http.close_client(fd)
        n = n + 1
    }
    return 0
}
EOF

if ! perl -e 'alarm(120); exec @ARGV' -- "$WYN" build "$SRV_SRC" -o "$SRV_BIN" > "$TMP/build.log" 2>&1; then
    bad "server builds from the documented pattern"
    sed -n '1,25p' "$TMP/build.log"
    echo ""
    echo "http-fd-injection: $PASS pass, $FAIL fail"
    exit 1
fi
ok "server builds from the documented pattern"

"$SRV_BIN" > "$SRV_LOG" 2>&1 &
SRV_PID=$!
disown "$SRV_PID" 2>/dev/null
PORT=$(read_reported_port "$SRV_LOG" ready) || PORT=""
if [ -z "$PORT" ]; then
    bad "server negotiates a port (walked 200 from $PORT_BASE, none bound)"
    sed -n '1,20p' "$SRV_LOG"
    echo ""
    echo "http-fd-injection: $PASS pass, $FAIL fail"
    exit 1
fi

# --- 1. the documented idiom still serves a normal request --------------------
body=$(perl -e 'alarm(10); exec @ARGV' -- curl -s -m 5 "http://127.0.0.1:$PORT/hello" 2>/dev/null)
if [ "$body" = "PAYLOAD-OK" ]; then ok "the documented split_at(\"|\",3) idiom still serves a request"
else bad "the documented split_at(\"|\",3) idiom still serves a request (got [$body])"; fi

# --- 2/3. an injected descriptor must not be honoured -------------------------
# Raw sockets: curl would percent-encode the '|' and the injection would never
# reach the parser. The request target is sent verbatim, which is the attack.
inject() {
    python3 - "$PORT" "$1" <<'PY' >/dev/null 2>&1
import socket, sys
try:
    s = socket.create_connection(('127.0.0.1', int(sys.argv[1])), timeout=3)
    s.sendall(("GET " + sys.argv[2] + " HTTP/1.0\r\nHost: x\r\n\r\n").encode())
    s.settimeout(3)
    try:
        while s.recv(4096):
            pass
    except Exception:
        pass
    s.close()
except Exception:
    pass
PY
}

inject '/a|b|1'
sleep 0.4
if grep -q 'HTTP/1.1' "$SRV_LOG"; then
    bad "GET /a|b|1 wrote the response to the server's own stdout [$(grep -m1 'HTTP/1.1' "$SRV_LOG")]"
else
    ok "GET /a|b|1 does not write the response to the server's own stdout"
fi

for n in 0 2 3 4 5; do
    inject "/x|y|$n"
done
sleep 0.5
if grep -q 'HTTP/1.1' "$SRV_LOG"; then
    bad "an injected small descriptor was honoured [$(grep -m1 'HTTP/1.1' "$SRV_LOG")]"
else
    ok "no injected small descriptor (0,2,3,4,5) is honoured"
fi

# --- 5. a forged high-numbered token is refused -------------------------------
# A token is (nonce << 20) | fd. Guessing one means guessing a 42-bit nonce; these
# are structurally valid-looking values with the wrong nonce, which must resolve to
# -1 rather than to the fd in their low bits.
for t in 1048577 4294967296 1152921504606846977; do
    inject "/z|w|$t"
done
sleep 0.5
if grep -q 'HTTP/1.1' "$SRV_LOG"; then
    bad "a forged capability token was honoured [$(grep -m1 'HTTP/1.1' "$SRV_LOG")]"
else
    ok "a forged capability token is refused"
fi

# --- 4. a body containing '|' survives intact and is still answered -----------
# The old left-to-right scan truncated the body at the first pipe AND shifted the
# descriptor field. Both halves are asserted: the body round-trips whole, and the
# request is still answered 200.
rbody=$(perl -e 'alarm(10); exec @ARGV' -- curl -s -m 5 -X POST --data-binary 'a|b|1' \
        -H 'Content-Type: text/plain' "http://127.0.0.1:$PORT/post" 2>/dev/null)
if [ "$rbody" = "PAYLOAD-OK" ]; then ok "a request body containing '|' is still answered"
else bad "a request body containing '|' is still answered (got [$rbody])"; fi
if grep -q 'body=\[a|b|1\]' "$SRV_LOG"; then
    ok "a request body containing '|' reaches the handler intact"
else
    bad "a request body containing '|' reaches the handler intact [$(grep -m1 'body=' "$SRV_LOG" || echo 'no body= line')]"
fi

# --- 6. the server survived all of it ----------------------------------------
if ! kill -0 "$SRV_PID" 2>/dev/null; then
    bad "server survived the injection attempts"
    sed -n '1,25p' "$SRV_LOG"
else
    body=$(perl -e 'alarm(10); exec @ARGV' -- curl -s -m 5 "http://127.0.0.1:$PORT/after" 2>/dev/null)
    if [ "$body" = "PAYLOAD-OK" ]; then ok "server still answers a real request afterwards"
    else bad "server still answers a real request afterwards (got [$body])"; fi
fi

echo ""
echo "http-fd-injection: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
