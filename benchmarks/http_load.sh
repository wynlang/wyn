#!/bin/bash
# HTTP throughput benchmark - the harness behind every req/s figure we publish.
#
# WHY THIS EXISTS: the site once claimed "22,000 req/s" with no committed way to
# reproduce it. When someone finally tried, the server wedged instead (a real
# bug, since fixed - see tests/errors/run_http_server_load_test.sh). A number
# nobody can regenerate is not a measurement, so: if a req/s figure appears in
# docs, it comes from this script, and the docs say which line of output it is.
#
# Run from the wyn/ directory:
#   ./benchmarks/http_load.sh            # full sweep
#   ./benchmarks/http_load.sh --quick    # one config, for a smoke check
#
# METHOD (and its limits, stated plainly):
#   * The server is the LOOPING-handler pattern. Keep-alive is a property of the
#     handler: one that reads once and returns cannot serve a second request on
#     the connection whatever the runtime does. A one-shot handler is correct and
#     supported (it is measured below as the no-keep-alive rows) but it cannot
#     produce a persistent-connection number.
#   * Every configuration gets a WARMUP run whose result is thrown away, then a
#     measured run. This is not cosmetic: the scheduler's worker pool spins up
#     lazily, and the same config measures ~15k cold and ~23k warm. A benchmark
#     without warmup mostly measures pool startup.
#   * Loopback only, so this is a language/runtime number, not a network one.
#   * Load generator: ApacheBench (`ab`) when present, else a bundled python
#     generator. They do NOT produce identical numbers - ab is the reference, and
#     the output labels which one ran.
#   * The machine must be otherwise IDLE. A parallel build or test run halves the
#     result; the script warns if the load average looks busy.
#
# Numbers are hardware- and load-specific. Publish the platform line with them.
set -uo pipefail
# The server is killed with SIGKILL; bash job control would otherwise print
# "Killed: 9 ..." after the results table.
set +m 2>/dev/null
cd "$(dirname "$0")/.."

WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
PORT="${PORT:-18099}"
QUICK=0
[ "${1:-}" = "--quick" ] && QUICK=1

TMP=$(mktemp -d)
SRV_BIN="$TMP/bench_srv.out"
SRV_PID=""
# Process hygiene is not optional here: a leaked server spins a core, and stray
# load generators have kernel-panicked a dev machine twice.
cleanup() {
    [ -n "$SRV_PID" ] && kill -9 "$SRV_PID" 2>/dev/null
    pkill -9 -f "^$SRV_BIN" 2>/dev/null
    rm -rf "$TMP"
}
trap cleanup EXIT INT TERM

echo "=== Wyn HTTP throughput ==="
echo "Platform: $(uname -s) $(uname -m), $( (sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo '?') ) cores"
echo "Wyn: $(cat VERSION 2>/dev/null || echo '?')"
echo "Port: $PORT"

LOAD=$(uptime | sed -n 's/.*load averages*:[ ]*\([0-9.]*\).*/\1/p')
echo "Load average at start: ${LOAD:-unknown}"
case "$LOAD" in
  ''|*[!0-9.]*) ;;
  *) if [ "${LOAD%%.*}" -ge 2 ] 2>/dev/null; then
         echo "  WARNING: machine is busy (load ${LOAD}). Numbers will be LOW and noisy."
         echo "           Re-run on an idle machine before publishing anything."
     fi ;;
esac
echo ""

# --- the server under test ----------------------------------------------------
# Looping handler (see METHOD). Plain-text body so the measurement is the
# request/response path, not HTML generation.
cat > "$TMP/bench_srv.wyn" <<WYN
fn handle(conn: int) {
    while true {
        var req = Http.read_request(conn)
        if req.len() == 0 { return }
        Http.respond(Http.fd(req), 200, "text/plain", "hello")
    }
}

fn main() -> int {
    var server = Http.serve($PORT)
    if server <= 0 { return 1 }
    println("listening")
    while true {
        var conn = Http.accept_fd(server)
        if conn > 0 { spawn handle(conn) }
    }
    return 0
}
WYN

echo "Building server (--release)..."
if ! "$WYN" build "$TMP/bench_srv.wyn" -o "$SRV_BIN" --release > "$TMP/build.log" 2>&1; then
    echo "BUILD FAILED:"; sed -n '1,25p' "$TMP/build.log"; exit 1
fi

"$SRV_BIN" > "$TMP/srv.log" 2>&1 &
SRV_PID=$!
# Detach from job control: we SIGKILL the server at the end, and bash would
# otherwise print "Killed: 9 ..." after the results table.
disown "$SRV_PID" 2>/dev/null
# Wait for the listener. Poll INSIDE one python process: spawning an interpreter
# per attempt takes ~0.2s each on a loaded machine, so an 80-iteration shell loop
# spent its whole budget on interpreter startup and declared a healthy server
# dead. One process, 30s of polling, negligible overhead.
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
    echo "SERVER DID NOT START:"; sed -n '1,25p' "$TMP/srv.log"; exit 1
fi
echo "Server up (pid $SRV_PID)"
echo ""

# --- load generators ----------------------------------------------------------
# Each prints a single number: requests/sec. Empty output = the run failed.
HAVE_AB=0
command -v ab >/dev/null 2>&1 && HAVE_AB=1
[ -x /usr/sbin/ab ] && HAVE_AB=1 && AB=/usr/sbin/ab
AB="${AB:-ab}"

ab_run() {  # ab_run <n> <c> <keepalive:0|1>
    local n=$1 c=$2 ka=$3 flag=""
    [ "$ka" = "1" ] && flag="-k"
    # `ab` fails the whole run rather than reporting a partial rate, which is
    # what we want: a wedged server must not silently produce a number.
    perl -e 'alarm(300); exec @ARGV' -- "$AB" -n "$n" -c "$c" $flag \
        "http://127.0.0.1:$PORT/" 2>/dev/null \
        | awk '/Requests per second/ {print $4}'
}

py_run() {  # py_run <n> <c> <keepalive:0|1>
    perl -e 'alarm(300); exec @ARGV' -- python3 - "$PORT" "$1" "$2" "$3" <<'PY' 2>/dev/null
import socket, sys, time
from concurrent.futures import ThreadPoolExecutor
port, n, c, ka = (int(x) for x in sys.argv[1:5])
REQ_KA = b"GET / HTTP/1.1\r\nHost: b\r\n\r\n"
REQ_CL = b"GET / HTTP/1.1\r\nHost: b\r\nConnection: close\r\n\r\n"
per = max(1, n // c)

def read_one(s, buf):
    while b"\r\n\r\n" not in buf:
        b2 = s.recv(65536)
        if not b2: raise EOFError
        buf += b2
    head, rest = buf.split(b"\r\n\r\n", 1)
    cl = 0
    for line in head.split(b"\r\n"):
        if line.lower().startswith(b"content-length:"):
            cl = int(line.split(b":")[1])
    while len(rest) < cl:
        b2 = s.recv(65536)
        if not b2: raise EOFError
        rest += b2
    return rest[cl:]

def worker(_):
    done = 0
    try:
        if ka:
            s = socket.create_connection(("127.0.0.1", port), timeout=15.0)
            s.settimeout(15.0)
            buf = b""
            for _i in range(per):
                s.sendall(REQ_KA)
                buf = read_one(s, buf)
                done += 1
            s.close()
        else:
            for _i in range(per):
                s = socket.create_connection(("127.0.0.1", port), timeout=15.0)
                s.settimeout(15.0)
                s.sendall(REQ_CL)
                while s.recv(65536): pass
                s.close()
                done += 1
    except Exception:
        pass
    return done

t0 = time.perf_counter()
with ThreadPoolExecutor(max_workers=c) as ex:
    total = sum(ex.map(worker, range(c)))
dt = time.perf_counter() - t0
want = per * c
# Only report a rate if EVERY request completed. A partial run means the server
# failed, and averaging over the failures would manufacture a plausible number.
print(f"{total/dt:.0f}" if total == want and dt > 0 else "")
PY
}

if [ "$HAVE_AB" = "1" ]; then
    GEN="ab"; RUN=ab_run
else
    GEN="python (ab not installed - not directly comparable to ab numbers)"; RUN=py_run
fi
echo "Load generator: $GEN"
echo ""

# --- the sweep ----------------------------------------------------------------
if [ "$QUICK" = "1" ]; then
    CONFIGS="20000:50:1"
else
    #     n : concurrency : keep-alive
    CONFIGS="20000:10:1 20000:50:1 20000:100:1 20000:200:1 10000:10:0 10000:50:0 10000:200:0"
fi

WARM_N=5000
printf "%-34s %14s\n" "configuration" "req/s"
printf "%-34s %14s\n" "----------------------------------" "--------------"
for cfg in $CONFIGS; do
    n=${cfg%%:*}; rest=${cfg#*:}; c=${rest%%:*}; ka=${rest##*:}
    label="c=$c, $( [ "$ka" = "1" ] && echo 'keep-alive' || echo 'connection/request' )"
    # Warmup (discarded) - the scheduler pool spins up lazily.
    $RUN "$WARM_N" "$c" "$ka" >/dev/null 2>&1
    rate=$($RUN "$n" "$c" "$ka")
    if [ -z "$rate" ]; then
        printf "%-34s %14s\n" "$label" "FAILED"
    else
        printf "%-34s %14s\n" "$label" "$rate"
    fi
done

echo ""
# An fd leak would show up as a server holding thousands of sockets after the
# sweep. It did, before the Http_respond fix; keep an eye on it.
if command -v lsof >/dev/null 2>&1 && [ -n "$SRV_PID" ]; then
    echo "Server open fds after sweep: $(lsof -p "$SRV_PID" 2>/dev/null | wc -l | tr -d ' ') (a per-request leak would be in the thousands)"
fi
echo "Load average at end: $(uptime | sed -n 's/.*load averages*:[ ]*\([0-9.]*\).*/\1/p')"
echo ""
echo "=== Done ==="
