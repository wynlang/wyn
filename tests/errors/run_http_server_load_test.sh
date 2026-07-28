#!/bin/bash
# An HTTP server built from the DOCUMENTED one-shot handler pattern must answer
# every request and release every connection, under real concurrent load.
#
# THE BUG (found 2026-07-28 while trying to reproduce the published req/s claim):
# Http_respond deferred the connection FIN to the handler's NEXT read_request
# call. That is fine for a handler written as `while true { read_request... }`,
# but EVERY documented example - homepage, README, guides - is one-shot:
#
#     fn handle(conn: int) {
#         req = web.read_request(conn)      # reads once
#         web.html(req, 200, ...)           # responds once
#     }                                     # returns; never reads again
#
# So the FIN was never sent. We had already told the client
# `Connection: close`, which means "the response ends at EOF" - and the EOF
# never came. Every non-keep-alive client (ApacheBench's default, curl
# --no-keepalive, any HTTP/1.0 client) hung waiting for it. `ab` wedged even at
# concurrency 1: `apr_pollset_poll: The timeout specified has expired`. The fd
# leaked too, one per request. A single sequential `curl` masked it because curl
# honours Content-Length and exits before noticing the connection never closed.
#
# THE FIX (src/wyn_runtime.h, Http_respond): shutdown(fd, SHUT_WR) immediately
# on close semantics - the FIN goes out now, while the fd NUMBER stays reserved
# so a looping handler cannot have it recycled underneath it. Plus
# Http_close_client, which the checker had advertised for ages with no
# implementation at all (any program calling it failed to compile), so a
# one-shot handler now has a way to hand the fd back.
#
# Cases:
#   1  HTTP/1.0 request gets a complete response AND an EOF        (the wedge)
#   2  HTTP/1.1 + `Connection: close` likewise
#   3  200 concurrent no-keep-alive requests all COMPLETE          (the claim)
#   4  fds do not grow without bound across the load               (the leak)
#   5  keep-alive still works, many requests on ONE connection     (no regress)
#
# Cases 1-4 drive the ONE-SHOT handler (the broken shape). Case 5 drives a
# second server written as a LOOPING handler, because keep-alive is a property
# of the handler, not just the runtime: a handler that reads once and returns
# has nothing left to serve a second request with, whatever the runtime does.
# Persistent connections therefore require the loop, and that is the pattern the
# throughput harness (benchmarks/http_load.sh) uses.
#
# Every wait is bounded: stock macOS has no `timeout`, so use perl's alarm (the
# convention in this directory). A REGRESSION must fail loudly, never hang the
# suite. And the server is killed on every exit path - stray load generators
# have kernel-panicked this dev machine twice.
set -uo pipefail
# The servers are killed with SIGKILL, and bash's job control would otherwise
# print "Killed: 9 ..." to stderr mid-report. Silence job notifications; the
# test's own ok/FAIL lines are the output that matters.
set +m 2>/dev/null

WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d)
SRV_BIN="$TMP/srv.out"
SRV_PID=""
# Not 8080: that port is contended on dev machines (something already answered
# there during the investigation, which is its own way to get a wrong number).
PORT=18099

cleanup() {
    [ -n "$SRV_PID" ] && kill -9 "$SRV_PID" 2>/dev/null
    pkill -9 -f "^$SRV_BIN" 2>/dev/null
    rm -rf "$TMP"
}
trap cleanup EXIT
PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }
skip(){ echo "  skip  $1"; }

case "${OS:-}" in
  Windows_NT) echo "http-server-load: SKIP (POSIX sockets/pgrep required)"; exit 0 ;;
esac
if ! command -v python3 >/dev/null 2>&1; then
    echo "http-server-load: SKIP (python3 needed to drive the load)"; exit 0
fi

# The server under test. Deliberately the ONE-SHOT handler shape - the one every
# doc example uses and the one that was broken. Uses the Http builtins directly
# rather than the `web` package so the test needs no network fetch.
cat > "$TMP/srv.wyn" <<'WYN'
fn handle(conn: int) {
    var req = Http.read_request(conn)
    if req.len() == 0 { return }
    Http.respond(Http.fd(req), 200, "text/plain", "hello")
    Http.close_client(Http.fd(req))
}

fn main() -> int {
    var server = Http.serve(18099)
    if server <= 0 { return 1 }
    println("listening")
    while true {
        var conn = Http.accept_fd(server)
        if conn > 0 { spawn handle(conn) }
    }
    return 0
}
WYN

if ! perl -e 'alarm(120); exec @ARGV' -- "$WYN" build "$TMP/srv.wyn" -o "$SRV_BIN" > "$TMP/build.log" 2>&1; then
    # A build failure here is a real failure (Http.close_client not linking is
    # exactly one of the bugs this guards), so say so rather than skipping.
    echo "  FAIL  server builds"
    sed -n '1,20p' "$TMP/build.log"
    echo ""
    echo "http-server-load: 0 pass, 1 fail"
    exit 1
fi
ok "one-shot-handler server builds (Http.close_client links)"

"$SRV_BIN" > "$TMP/srv.log" 2>&1 &
SRV_PID=$!
disown "$SRV_PID" 2>/dev/null

# Wait (bounded) for the listener, polling INSIDE one python process. A shell
# loop that starts a fresh interpreter per attempt spends ~0.2s of its budget on
# interpreter startup per iteration on a loaded machine, and will call a healthy
# server dead - which is a flaky test, not a real failure.
if python3 - "$PORT" <<'PY'
import socket, sys, time
port = int(sys.argv[1])
deadline = time.time() + 30
while time.time() < deadline:
    s = socket.socket(); s.settimeout(0.3)
    if s.connect_ex(("127.0.0.1", port)) == 0:
        s.close(); sys.exit(0)
    s.close(); time.sleep(0.1)
sys.exit(1)
PY
then up=1; else up=0; fi
if [ "$up" != "1" ]; then
    bad "server comes up on 127.0.0.1:$PORT"
    sed -n '1,20p' "$TMP/srv.log"
    echo ""
    echo "http-server-load: $PASS pass, $((FAIL+1)) fail"
    exit 1
fi
ok "server listening on 127.0.0.1:$PORT"

# --- Cases 1-3: response completeness per connection style -------------------
# Reads to EOF with a hard socket timeout. Before the fix, cases 1 and 2 hit
# that timeout with the body already in hand but no EOF - the exact way `ab`
# wedged.
run_py() { perl -e 'alarm(60); exec @ARGV' -- python3 - "$PORT" "$@"; }

r=$(run_py <<'PY'
import socket, sys
port = int(sys.argv[1])
def probe(req):
    s = socket.create_connection(("127.0.0.1", port), timeout=5.0)
    s.settimeout(5.0)
    s.sendall(req)
    data = b""
    try:
        while True:
            b = s.recv(65536)
            if not b: break        # EOF - what a Connection: close client waits for
            data += b
    except socket.timeout:
        s.close(); return "NOEOF"
    s.close()
    return "OK" if b"200" in data and b"hello" in data else "BAD"
print("http10=" + probe(b"GET / HTTP/1.0\r\nHost: x\r\n\r\n"))
print("close11=" + probe(b"GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"))
PY
)
case "$r" in
  *http10=OK*)    ok "HTTP/1.0 request gets a complete response and an EOF" ;;
  *http10=NOEOF*) bad "HTTP/1.0: response sent but connection never closed (the ab wedge)" ;;
  *)              bad "HTTP/1.0 request answered ($r)" ;;
esac
case "$r" in
  *close11=OK*)    ok "HTTP/1.1 Connection: close gets a complete response and an EOF" ;;
  *close11=NOEOF*) bad "HTTP/1.1 close: response sent but connection never closed" ;;
  *)               bad "HTTP/1.1 Connection: close answered ($r)" ;;
esac

# --- Case 3: real concurrent load, no keep-alive ------------------------------
# 200 concurrent connections, one request each - the shape of the published
# claim, and what `ab -c 200` could not complete. Asserts COMPLETION, not a
# rate: a throughput number here would be a flaky assertion on a shared CI box.
# The rate lives in benchmarks/http_load.sh, which is run by hand.
r=$(run_py <<'PY'
import socket, sys
from concurrent.futures import ThreadPoolExecutor
port = int(sys.argv[1])
N = 200
def one(_):
    try:
        s = socket.create_connection(("127.0.0.1", port), timeout=10.0)
        s.settimeout(10.0)
        s.sendall(b"GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n")
        data = b""
        while True:
            b = s.recv(65536)
            if not b: break
            data += b
        s.close()
        return 1 if b"hello" in data else 0
    except Exception:
        return 0
with ThreadPoolExecutor(max_workers=N) as ex:
    got = sum(ex.map(one, range(N)))
print(f"load={got}/{N}")
PY
)
if [ "$r" = "load=200/200" ]; then
    ok "200 concurrent no-keep-alive requests all complete"
else
    bad "concurrent load did not complete ($r, want load=200/200)"
fi

# --- Case 5: fds released ----------------------------------------------------
# The deferred-FIN bug also leaked the fd. Count open fds, push another 300
# requests through, count again: a per-request leak shows up as ~+300. Allow
# generous slack for the kernel's own churn and any in-flight sockets.
count_fds() { lsof -p "$SRV_PID" 2>/dev/null | wc -l | tr -d ' '; }
if ! command -v lsof >/dev/null 2>&1; then
    skip "fd-leak check (no lsof)"
else
    before=$(count_fds)
    run_py <<'PY' >/dev/null
import socket, sys
from concurrent.futures import ThreadPoolExecutor
port = int(sys.argv[1])
def one(_):
    try:
        s = socket.create_connection(("127.0.0.1", port), timeout=10.0)
        s.settimeout(10.0)
        s.sendall(b"GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n")
        while s.recv(65536): pass
        s.close()
    except Exception:
        pass
with ThreadPoolExecutor(max_workers=20) as ex:
    list(ex.map(one, range(300)))
PY
    sleep 1
    after=$(count_fds)
    growth=$((after - before))
    if [ "$growth" -lt 60 ]; then
        ok "fds released across 300 more requests (+$growth, was +1/request)"
    else
        bad "fd leak: +$growth fds over 300 requests (before=$before after=$after)"
    fi
fi

kill -9 "$SRV_PID" 2>/dev/null
SRV_PID=""

# --- Case 5: keep-alive, LOOPING handler -------------------------------------
# Persistent connections need a handler that reads again after responding, so
# this is a second server. The guard is that the fix did not turn keep-alive into
# one-shot: 25 requests must go down a SINGLE socket. This is also the shape the
# throughput harness measures, so a regression here invalidates the published
# req/s figure.
KA_BIN="$TMP/ka.out"
KA_PORT=18100
KA_PID=""
cleanup_ka() { [ -n "$KA_PID" ] && kill -9 "$KA_PID" 2>/dev/null; pkill -9 -f "^$KA_BIN" 2>/dev/null; }
trap 'cleanup_ka; cleanup' EXIT

cat > "$TMP/ka.wyn" <<'WYN'
fn handle(conn: int) {
    while true {
        var req = Http.read_request(conn)
        if req.len() == 0 { return }
        Http.respond(Http.fd(req), 200, "text/plain", "hello")
    }
}

fn main() -> int {
    var server = Http.serve(18100)
    if server <= 0 { return 1 }
    println("listening")
    while true {
        var conn = Http.accept_fd(server)
        if conn > 0 { spawn handle(conn) }
    }
    return 0
}
WYN

if ! perl -e 'alarm(120); exec @ARGV' -- "$WYN" build "$TMP/ka.wyn" -o "$KA_BIN" > "$TMP/kabuild.log" 2>&1; then
    bad "looping-handler server builds"
    sed -n '1,20p' "$TMP/kabuild.log"
else
    "$KA_BIN" > "$TMP/ka.log" 2>&1 &
    KA_PID=$!
    disown "$KA_PID" 2>/dev/null
    if python3 - "$KA_PORT" <<'PY'
import socket, sys, time
port = int(sys.argv[1])
deadline = time.time() + 30
while time.time() < deadline:
    s = socket.socket(); s.settimeout(0.3)
    if s.connect_ex(("127.0.0.1", port)) == 0:
        s.close(); sys.exit(0)
    s.close(); time.sleep(0.1)
sys.exit(1)
PY
    then kaup=1; else kaup=0; fi
    if [ "$kaup" != "1" ]; then
        bad "keep-alive server comes up on 127.0.0.1:$KA_PORT"
        echo "    --- ka.log ---"; sed -n '1,20p' "$TMP/ka.log"
        echo "    --- alive? ---"; ps -p "$KA_PID" -o pid,stat,command 2>&1 | sed -n '1,3p'
    else
        r=$(perl -e 'alarm(60); exec @ARGV' -- python3 - "$KA_PORT" <<'PY'
import socket, sys
port = int(sys.argv[1])
N = 25
s = socket.create_connection(("127.0.0.1", port), timeout=5.0)
s.settimeout(5.0)
buf = b""; done = 0
try:
    for _ in range(N):
        s.sendall(b"GET / HTTP/1.1\r\nHost: x\r\n\r\n")
        while b"\r\n\r\n" not in buf:
            b = s.recv(65536)
            if not b: raise EOFError
            buf += b
        head, rest = buf.split(b"\r\n\r\n", 1)
        cl = 0
        for line in head.split(b"\r\n"):
            if line.lower().startswith(b"content-length:"):
                cl = int(line.split(b":")[1])
        while len(rest) < cl:
            b = s.recv(65536)
            if not b: raise EOFError
            rest += b
        buf = rest[cl:]
        done += 1
except Exception:
    pass
s.close()
print(f"ka={done}/{N}")
PY
)
        if [ "$r" = "ka=25/25" ]; then
            ok "keep-alive: 25 requests on one connection (looping handler)"
        else
            bad "keep-alive regressed ($r, want ka=25/25)"
        fi
    fi
fi
cleanup_ka
KA_PID=""

echo ""
echo "http-server-load: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
