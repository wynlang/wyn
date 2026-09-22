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
#
# `set +m` is NOT sufficient on its own: bash still announces an asynchronous job
# that died on a signal when it reaps it, so the test's own `kill -9` of its own
# server printed "line NNN: PID Killed: 9 ..." on EVERY run (4/4 measured) while
# still reporting "9 pass, 0 fail". That noise has twice been read as evidence the
# server was OOM-killed. Every background server below is therefore `disown`ed -
# the only thing that actually silences it - so a "Killed: 9" line appearing here
# again would be a real signal rather than the test shooting its own child.
set +m 2>/dev/null

WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d)
SRV_BIN="$TMP/srv.out"
SRV_PID=""

# --- PORTS ARE NEGOTIATED WITH THE KERNEL, NEVER HARD-CODED --------------------
# This test used to bind the fixed ports 18099 / 18100 / 18131. Sibling agents run
# `make test` concurrently out of separate worktrees, and a fixed port turns that
# into a cross-lane collision that reports as a defect in the code under test. It
# produced three false reds in one day; the worst read EXACTLY like a regression
# in the bug this file guards:
#
#   FAIL  empty connection must not panic the server
#         [panic: to_int parse error: "" is not a valid integer]
#
# ...because the loser of the bind race got -1 from Http.serve, and Http.accept(-1)
# returns "", which is the same observable as the accept bug. Reproduced on demand
# by running this script twice with a 3s stagger.
#
# The fix has two halves, and BOTH are needed:
#   1. a per-run random base, so two concurrent runs do not even start on the same
#      number (`$$` is mixed in because two shells started in the same second can
#      seed $RANDOM identically);
#   2. each server WALKS UP from its base until bind() succeeds and then prints the
#      port it actually got; the shell drives whatever the server reports.
#
# Half 2 is what makes this collision-proof rather than collision-unlikely. A
# guessed-free-port-then-hand-it-over scheme still has a window between the probe
# closing the socket and the server binding it. Here the port the shell uses IS the
# port a successful bind() returned, so there is no window and no way to
# silently drive somebody else's server: Http_serve sets SO_REUSEADDR but NOT
# SO_REUSEPORT, so a second *listening* socket on a live port gets EADDRINUSE
# (verified: two Http.serve calls on one port return fd, -1) and the walk moves on.
# Range 20000-31000 stays clear of the ephemeral ranges (Linux 32768+, macOS 49152+).
#
# WYN_TEST_PORT_BASE pins the base, which is how the walk itself gets tested:
# start several runs with the SAME base and every one must still pass, because
# each loser of the bind race walks off the contended port. Without that lever the
# random base would make the collision path unreachable in practice, i.e. untested.
PORT_BASE=${WYN_TEST_PORT_BASE:-$(( 20000 + ((RANDOM + $$) % 10000) ))}
PORT=""            # filled in from the server's own report
KA_PORT=""
DOS_PORT=""

# Read the port a server negotiated, out of its own stdout log. println() flushes
# (println_str -> fflush, src/wyn_runtime.h), so the line lands as soon as bind()
# returned. Bounded at 30s, matching the old listener probe.
#   $1 = log file   $2 = the word the server prints before the number
read_reported_port() {
    local i p
    for i in $(seq 1 300); do
        p=$(sed -n "s/^$2 \([0-9][0-9]*\)\$/\1/p" "$1" 2>/dev/null | head -1)
        if [ -n "$p" ]; then printf '%s' "$p"; return 0; fi
        sleep 0.1
    done
    return 1
}

cleanup() {
    [ -n "$SRV_PID" ] && kill -9 "$SRV_PID" 2>/dev/null
    pkill -9 -f "^$SRV_BIN" 2>/dev/null
    # The empty-connection server is now disowned (see `set +m` above), so the
    # shell will not reap it for us on an early exit; name it explicitly. A stray
    # load-generator/server pair has kernel-panicked this dev machine twice.
    [ -n "${DOS_PID:-}" ] && kill -9 "$DOS_PID" 2>/dev/null
    [ -n "${DOS_BIN:-}" ] && pkill -9 -f "^$DOS_BIN" 2>/dev/null
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
cat > "$TMP/srv.wyn" <<WYN
fn handle(conn: int) {
    var req = Http.read_request(conn)
    if req.len() == 0 { return }
    Http.respond(Http.fd(req), 200, "text/plain", "hello")
    Http.close_client(Http.fd(req))
}

fn main() -> int {
    // Walk up from the base until the kernel gives us a port, then report which
    // one. Never a fixed port: see the PORT_BASE comment at the top.
    var port = $PORT_BASE
    var server = -1
    var tries = 0
    while tries < 200 {
        server = Http.serve(port)
        if server > 0 { break }
        port = port + 1
        tries = tries + 1
    }
    if server <= 0 { return 1 }
    println("listening \${port}")
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

# The port comes from the server, not from this script - so it is by construction
# a port whose bind() succeeded in THIS process.
if ! PORT=$(read_reported_port "$TMP/srv.log" listening); then
    bad "server negotiates a port (walked 200 from $PORT_BASE, none bound)"
    sed -n '1,20p' "$TMP/srv.log"
    echo ""
    echo "http-server-load: $PASS pass, $((FAIL)) fail"
    exit 1
fi

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
# A distinct base from the one-shot server's. The one-shot server is dead by now
# and SO_REUSEADDR would let us take its port straight back, but reusing it would
# make a stale-socket symptom look like a keep-alive bug, so keep the two apart.
KA_BASE=$((PORT_BASE + 400))
KA_PID=""
cleanup_ka() { [ -n "$KA_PID" ] && kill -9 "$KA_PID" 2>/dev/null; pkill -9 -f "^$KA_BIN" 2>/dev/null; }
trap 'cleanup_ka; cleanup' EXIT

cat > "$TMP/ka.wyn" <<WYN
fn handle(conn: int) {
    while true {
        var req = Http.read_request(conn)
        if req.len() == 0 { return }
        Http.respond(Http.fd(req), 200, "text/plain", "hello")
    }
}

fn main() -> int {
    var port = $KA_BASE
    var server = -1
    var tries = 0
    while tries < 200 {
        server = Http.serve(port)
        if server > 0 { break }
        port = port + 1
        tries = tries + 1
    }
    if server <= 0 { return 1 }
    println("listening \${port}")
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
    KA_PORT=$(read_reported_port "$TMP/ka.log" listening) || KA_PORT=""
    if [ -z "$KA_PORT" ]; then
        kaup=0
    elif python3 - "$KA_PORT" <<'PY'
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
        if [ -z "$KA_PORT" ]; then
            bad "keep-alive server negotiates a port (walked 200 from $KA_BASE, none bound)"
        else
            bad "keep-alive server comes up on 127.0.0.1:$KA_PORT"
        fi
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

# --- A connection that sends NOTHING must not kill the server ---------------
# ONE PACKET, UNAUTHENTICATED, AND THE SERVER IS GONE. Http_accept returned the
# empty string when a client connected and then sent nothing, and the DOCUMENTED
# server pattern is
#
#     req = Http.accept(server)
#     fd  = req.split_at("|", 3).to_int()
#
# so "" reached `.to_int()`, which panics since to_int was correctly made loud
# about bad input. Two individually-correct changes, lethal in combination: any
# Wyn HTTP server died on the first empty connection. A port scan, a TCP health
# check, an L4 load-balancer probe or a browser preconnect all do exactly this.
# Nothing in the corpus checked accept for emptiness - not one example, doc
# snippet or stdlib test - so every server shipped with it.
#
# Verified against the blog's flagship "REST API in 93 lines": 30 concurrent real
# POSTs were fine, and a SINGLE connect-then-close killed it. Normal load was
# never the trigger, which is why this survived the load gate above.
#
# Http_accept now skips a connection that yields no request and keeps accepting,
# which is what every real server does.
#
# THE BIND IS DELIBERATELY SEPARATED FROM THE ACCEPT LOOP HERE. The naive
# `req.split_at("|",3).to_int()` shape below IS the property under test and must
# stay naive - but the bind must not be, because an unchecked bind gives this arm
# TWO causes with ONE observable. When this file bound the fixed port 18131 and a
# sibling lane held it, Http.serve returned -1, Http.accept(-1) returned "", and
# `.to_int()` panicked - reporting verbatim
#     FAIL  empty connection must not panic the server
#           [panic: to_int parse error: "" is not a valid integer]
# i.e. the exact regression this arm exists to catch, from a port clash. So the
# port is negotiated (and checked) first, and only then is the naive loop entered.
DOS_SRC="$TMP/dos.wyn"
DOS_BIN="$TMP/dos.out"
DOS_BASE=$((PORT_BASE + 800))
cat > "$DOS_SRC" <<EOF
fn main() -> int {
    var port = $DOS_BASE
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
    while n < 3 {
        req = Http.accept(server)
        fd = req.split_at("|", 3).to_int()
        Http.respond(fd, 200, "text/plain", "ok")
        n = n + 1
    }
    return 0
}
EOF
if ! perl -e 'alarm(90); exec @ARGV' -- "$WYN" build "$DOS_SRC" -o "$DOS_BIN" >/dev/null 2>&1; then
    bad "empty-connection: server built"
else
    "$DOS_BIN" > "$TMP/dos.log" 2>&1 &
    DOS_PID=$!
    disown "$DOS_PID" 2>/dev/null
    DOS_PORT=$(read_reported_port "$TMP/dos.log" ready) || DOS_PORT=""
fi
if [ -z "${DOS_PORT:-}" ] && [ -n "${DOS_PID:-}" ]; then
    bad "empty-connection server negotiates a port (walked 200 from $DOS_BASE, none bound)"
    sed -n '1,20p' "$TMP/dos.log"
    kill -9 "$DOS_PID" 2>/dev/null
elif [ -n "${DOS_PORT:-}" ]; then
    # Three clients that connect and close without sending a byte. Before the fix
    # the FIRST one killed the server.
    python3 - "$DOS_PORT" <<'PY' >/dev/null 2>&1
import socket, sys
for _ in range(3):
    try:
        s = socket.create_connection(('127.0.0.1', int(sys.argv[1])), timeout=3); s.close()
    except Exception:
        pass
PY
    sleep 1
    if grep -qi "panic" "$TMP/dos.log"; then
        bad "empty connection must not panic the server [$(grep -i panic "$TMP/dos.log" | head -1)]"
    elif ! kill -0 "$DOS_PID" 2>/dev/null; then
        bad "server died on an empty connection (no panic logged)"
    else
        ok "a connection that sends nothing does not kill the server"
        # And it must still serve a REAL request afterwards - skipping the dead
        # connection is only correct if the accept loop carries on.
        body=$(perl -e 'alarm(10); exec @ARGV' -- curl -s -m 5 "http://127.0.0.1:$DOS_PORT/" 2>/dev/null)
        if [ "$body" = "ok" ]; then ok "server still answers a real request afterwards"
        else bad "server still answers a real request afterwards (got [$body])"; fi
    fi
    kill -9 "$DOS_PID" 2>/dev/null
    pkill -9 -f "^$DOS_BIN" 2>/dev/null
fi

echo ""
echo "http-server-load: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
