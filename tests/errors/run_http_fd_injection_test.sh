#!/bin/bash
# A REMOTE CLIENT MUST NOT BE ABLE TO NAME A FILE DESCRIPTOR.
#
# THE BUG (issue #467). Http_accept / Http_read_request hand user code the string
# "METHOD|PATH|BODY|FD" and the documented way to get the descriptor out is
#
#     fd = req.split_at("|", 3).to_int()
#
# which repos/web/src/web.wyn:69 and docs/stdlib/web.md publish verbatim. The path
# and the body are attacker-controlled and were interpolated UNESCAPED:
#
#     snprintf(result, 16384, "%s|%s|%s|%d", method, path, body, client_fd);
#
# so `GET /x|y|7 HTTP/1.0` shifted every later field and made that idiom yield 7 -
# a descriptor the client chose. Naming ANOTHER LIVE CONNECTION's descriptor sends
# this response to a different client: one client can be served another's data, or
# have content injected into its stream. Unauthenticated, first line of the request.
#
# THE FIX. The descriptor leaves the payload. Field 3 carries an unguessable
# capability token; the descriptor lives only in a runtime table, and respond /
# read_request / close_client resolve a token through it. A forged, stale or
# out-of-range token resolves to -1 and the call is a no-op.
#
# THE OBSERVABLE IS ON THE VICTIM, NOT IN A LOG. The first version of this gate
# asserted that an injected `1` did not put the response in the server's own stdout.
# That arm could never fail: the harness redirects stdout to a FILE, and
# http_send_response uses send(), which returns ENOTSOCK on a regular file. It
# passed with the authentication deleted - a vacuous control, the exact failure the
# project has recorded before. So the arm that matters now opens a VICTIM socket,
# leaves it parked, and asserts the victim never receives a byte it did not ask for.
# Both ends are real sockets, so send() succeeds and the bug is reachable; verified
# by deleting the nonce check and watching this arm go red.
#
# TWO SERVERS, because the two properties have different subjects.
#
# Server A is the CONCURRENT published shape - accept_fd + spawn, with the
# descriptor taken out of the request string, which is what repos/web does. Cases:
#   1  it still serves an ordinary request                          (compatibility)
#   2  a parked victim connection receives nothing while an attacker walks
#      candidate descriptors 3..20                                  (the hijack)
#   3  nor while an attacker offers structurally valid-looking forged tokens
#   4  the server survives all of it and still serves
#
# Case 1 is load-bearing, not a courtesy: the design rests on the published string
# shape continuing to work, so a fix that silently broke it is caught here rather
# than by a reader of the blog.
#
# Server B uses `Http.fd(req)` instead of splitting the string. Cases:
#   5  a path containing '|' is served correctly
#   6  a body containing '|' is served AND reaches the handler intact
#
# Server B exists because `Http.fd` resolves the record's final field from the RIGHT
# and so is robust against extra pipes anywhere earlier, while `split_at` is not: a
# path or body containing a pipe shifts the index-3 field and feeds `.to_int()`
# something non-numeric, which panics the published server. That is a SEPARATE
# pre-existing defect - it reproduces without this change - and this gate does not
# assert it away. What it asserts is that the accessor is a correct escape hatch,
# which is the fix a reader can apply today.
#
# Every wait is bounded (perl alarm; stock macOS has no `timeout`) and both servers
# are killed on every exit path - a stray server has kernel-panicked this box twice.
# Ports are NEGOTIATED, never hard-coded: tests/errors/run_test_port_hygiene_test.sh
# gates that, and a fixed port turns a sibling `make test` into a false red.
set -uo pipefail
set +m 2>/dev/null

WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d)
A_SRC="$TMP/srva.wyn"; A_BIN="$TMP/srva.out"; A_LOG="$TMP/srva.log"; A_PID=""
B_SRC="$TMP/srvb.wyn"; B_BIN="$TMP/srvb.out"; B_LOG="$TMP/srvb.log"; B_PID=""

cleanup() {
    [ -n "$A_PID" ] && kill -9 "$A_PID" 2>/dev/null
    [ -n "$B_PID" ] && kill -9 "$B_PID" 2>/dev/null
    pkill -9 -f "^$A_BIN" 2>/dev/null
    pkill -9 -f "^$B_BIN" 2>/dev/null
    rm -rf "$TMP"
}
trap cleanup EXIT
PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

echo "=== HTTP response-descriptor injection gate (#467) ==="

# The server reports the port it actually bound; the harness reads it back rather
# than assuming. 300 x 0.1s, so a slow start cannot race the first request.
read_reported_port() {
    local i p
    for i in $(seq 1 300); do
        p=$(sed -n "s/^$2 \([0-9][0-9]*\)\$/\1/p" "$1" 2>/dev/null | head -1)
        if [ -n "$p" ]; then printf '%s' "$p"; return 0; fi
        sleep 0.1
    done
    return 1
}

PORT_BASE=$(( 21000 + (RANDOM % 2000) ))

# Server A: the concurrent published shape. The handler deliberately takes the
# descriptor from the REQUEST STRING - that is the vulnerable idiom under test, and
# it is what repos/web/src/web.wyn:69 publishes. It must stay naive.
cat > "$A_SRC" <<EOF
fn handle(conn: int) {
    var req = Http.read_request(conn)
    if req == "" { return }
    var fd = req.split_at("|", 3).to_int()
    Http.respond(fd, 200, "text/plain", "PAYLOAD-OK")
}

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
    while n < 200 {
        var conn = Http.accept_fd(server)
        if conn > 0 { spawn handle(conn) }
        n = n + 1
    }
    return 0
}
EOF

if ! perl -e 'alarm(150); exec @ARGV' -- "$WYN" build "$A_SRC" -o "$A_BIN" > "$TMP/builda.log" 2>&1; then
    bad "concurrent published server builds"
    sed -n '1,25p' "$TMP/builda.log"
    echo ""; echo "http-fd-injection: $PASS pass, $FAIL fail"; exit 1
fi
ok "concurrent published server builds"

"$A_BIN" > "$A_LOG" 2>&1 &
A_PID=$!
disown "$A_PID" 2>/dev/null
APORT=$(read_reported_port "$A_LOG" ready) || APORT=""
if [ -z "$APORT" ]; then
    bad "server A negotiates a port (walked 200 from $PORT_BASE, none bound)"
    sed -n '1,20p' "$A_LOG"
    echo ""; echo "http-fd-injection: $PASS pass, $FAIL fail"; exit 1
fi

# --- 1. the documented idiom still serves an ordinary request ------------------
body=$(perl -e 'alarm(15); exec @ARGV' -- curl -s -m 10 "http://127.0.0.1:$APORT/hello" 2>/dev/null)
if [ "$body" = "PAYLOAD-OK" ]; then ok "the published split_at(\"|\",3) idiom still serves a request"
else bad "the published split_at(\"|\",3) idiom still serves a request (got [$body])"; fi

# --- 2/3. a parked victim must never receive someone else's response -----------
# The victim connects and sends NOTHING, so its handler parks in read_request and
# its descriptor stays open and un-shutdown. The attacker then offers candidate
# descriptors; if any is honoured, the attacker's response lands on the victim's
# socket. The victim polls after each attempt with a short timeout.
#
# Both the candidate sweep (3..20, which covers every descriptor a freshly started
# server can have handed out) and the forged-token set are driven from one script,
# because they share the victim: opening a second one would change the descriptor
# numbering the first arm depends on.
hijack=$(python3 - "$APORT" <<'PY' 2>/dev/null
import socket, sys
port = int(sys.argv[1])

def attempt(target):
    try:
        a = socket.create_connection(('127.0.0.1', port), timeout=5)
        a.sendall(("GET /x|y|%s HTTP/1.0\r\nHost: x\r\n\r\n" % target).encode())
        a.settimeout(2)
        try:
            while a.recv(4096):
                pass
        except Exception:
            pass
        a.close()
    except Exception:
        pass

try:
    victim = socket.create_connection(('127.0.0.1', port), timeout=5)
except Exception:
    print("SETUP-FAILED SETUP-FAILED")
    sys.exit(0)
victim.settimeout(0.3)

leaked_fd = []
for cand in range(3, 21):
    attempt(cand)
    try:
        d = victim.recv(4096)
        if d:
            leaked_fd.append(cand)
    except Exception:
        pass

leaked_tok = []
# (nonce << 20) | fd shapes carrying the WRONG nonce. The low bits sweep the same
# descriptor range as above, because the first version of this arm used a handful of
# fixed values that happened to miss the victim's actual descriptor - so a build
# with the nonce-table comparison deleted still passed it. Every candidate must
# resolve to -1 rather than to the descriptor in its low bits.
forged = []
for cand in range(3, 21):
    for nonce in (1, 4096):
        forged.append((nonce << 20) | cand)
forged += [4294967296, 1152921504606846977, 999999999999999999]
for tok in forged:
    attempt(tok)
    try:
        d = victim.recv(4096)
        if d:
            leaked_tok.append(tok)
    except Exception:
        pass

try:
    victim.close()
except Exception:
    pass
r1 = ("LEAKFD:" + ",".join(str(x) for x in leaked_fd)) if leaked_fd else "CLEAN-FD"
r2 = ("LEAKTOK:" + ",".join(str(x) for x in leaked_tok)) if leaked_tok else "CLEAN-TOK"
print(r1, r2)
PY
)
h_fd=$(printf '%s' "$hijack" | awk '{print $1}')
h_tok=$(printf '%s' "$hijack" | awk '{print $2}')
if [ "$h_fd" = "SETUP-FAILED" ] || [ -z "$h_fd" ]; then
    bad "victim connection could be established (harness setup) [$hijack]"
elif [ "$h_fd" = "CLEAN-FD" ]; then
    ok "a parked victim receives nothing while descriptors 3..20 are injected"
else
    bad "a parked victim received another client's response [$h_fd]"
fi
if [ "$h_tok" = "CLEAN-TOK" ]; then
    ok "a parked victim receives nothing while forged tokens are injected"
elif [ -n "$h_tok" ] && [ "$h_tok" != "SETUP-FAILED" ]; then
    bad "a forged capability token was honoured [$h_tok]"
fi

# --- 4. the server survived all of it -----------------------------------------
if ! kill -0 "$A_PID" 2>/dev/null; then
    bad "server A survived the injection attempts"
    sed -n '1,25p' "$A_LOG"
else
    body=$(perl -e 'alarm(15); exec @ARGV' -- curl -s -m 10 "http://127.0.0.1:$APORT/after" 2>/dev/null)
    if [ "$body" = "PAYLOAD-OK" ]; then ok "server A still answers a real request afterwards"
    else bad "server A still answers a real request afterwards (got [$body])"; fi
fi
kill -9 "$A_PID" 2>/dev/null; A_PID=""

# --- Server B: Http.fd(req) is robust where split_at is not -------------------
B_BASE=$(( PORT_BASE + 300 ))
cat > "$B_SRC" <<EOF
fn main() -> int {
    var port = $B_BASE
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
    while n < 8 {
        var req = Http.accept(server)
        var fd = Http.fd(req)
        println("B body=[\${Http.req_body(req)}]")
        Http.respond(fd, 200, "text/plain", "B-OK")
        Http.close_client(fd)
        n = n + 1
    }
    return 0
}
EOF
if ! perl -e 'alarm(150); exec @ARGV' -- "$WYN" build "$B_SRC" -o "$B_BIN" > "$TMP/buildb.log" 2>&1; then
    bad "accessor-based server builds"
    sed -n '1,25p' "$TMP/buildb.log"
else
    "$B_BIN" > "$B_LOG" 2>&1 &
    B_PID=$!
    disown "$B_PID" 2>/dev/null
    BPORT=$(read_reported_port "$B_LOG" ready) || BPORT=""
    if [ -z "$BPORT" ]; then
        bad "accessor-based server negotiates a port (walked 200 from $B_BASE, none bound)"
        sed -n '1,20p' "$B_LOG"
    else
        # Sent raw: curl would percent-encode the '|' and the case would evaporate.
        got=$(python3 - "$BPORT" <<'PY' 2>/dev/null
import socket, sys
try:
    s = socket.create_connection(('127.0.0.1', int(sys.argv[1])), timeout=5)
    s.sendall(b"GET /a|b|1 HTTP/1.0\r\nHost: x\r\n\r\n")
    s.settimeout(5)
    buf = b""
    while True:
        c = s.recv(4096)
        if not c:
            break
        buf += c
    s.close()
    sys.stdout.write(buf.decode('latin-1').split("\r\n\r\n", 1)[-1])
except Exception:
    pass
PY
)
        if [ "$got" = "B-OK" ]; then ok "Http.fd: a path containing '|' is served correctly"
        else bad "Http.fd: a path containing '|' is served correctly (got [$got])"; fi

        rbody=$(perl -e 'alarm(15); exec @ARGV' -- curl -s -m 10 -X POST --data-binary 'a|b|1' \
                -H 'Content-Type: text/plain' "http://127.0.0.1:$BPORT/post" 2>/dev/null)
        if [ "$rbody" = "B-OK" ]; then ok "Http.fd: a body containing '|' is served"
        else bad "Http.fd: a body containing '|' is served (got [$rbody])"; fi
        if grep -q 'B body=\[a|b|1\]' "$B_LOG"; then
            ok "Http.req_body: a body containing '|' reaches the handler intact"
        else
            bad "Http.req_body: a body containing '|' reaches the handler intact [$(grep -m1 'B body=' "$B_LOG" || echo 'no B body= line')]"
        fi
    fi
fi

echo ""
echo "http-fd-injection: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
