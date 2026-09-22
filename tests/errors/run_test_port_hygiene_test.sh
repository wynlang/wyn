#!/bin/bash
# V-23: A TEST MAY NOT BIND A FIXED PORT.
#
# Sibling agents run `make test` concurrently out of separate worktrees. A test
# that binds a hard-coded port therefore races other copies of itself, and the
# loser reports the clash as a defect in the code under test. That produced three
# false reds in a single day (2026-09-22); the worst one read EXACTLY like a
# regression in the bug it was guarding:
#
#   FAIL  empty connection must not panic the server
#         [panic: to_int parse error: "" is not a valid integer]
#
# ...because the loser of the bind race got -1 from Http.serve, and Http.accept(-1)
# returns "" - the same observable as the accept bug. One of the three aborted a
# whole suite, so three later gates never ran at all.
#
# This file gates the rule, and it does it by EXECUTION, not by grep. A grep count
# is not evidence (this repo has been bitten by treating one as evidence); the
# load-bearing arms below stand up two servers at once and look at what happens:
#
#   1  NEGATIVE CONTROL: two servers on ONE fixed port - the second MUST fail to
#      bind. If this arm ever passes, the hazard has changed (SO_REUSEPORT, a
#      different OS default) and arms 2-4 are measuring nothing, so it fails loudly.
#   2  THE FIX: two servers walking up from the SAME base - both must bind, on
#      DIFFERENT ports, and both must answer a request.
#   3  tripwire: no test source passes a literal nonzero port to a listen call.
#   4  tripwire: the load gate carries no hard-coded port assignment.
#
# Arms 1+2 together are what make this non-vacuous: 1 proves a fixed port really
# does collide, 2 proves the walk resolves it. Neither alone would.
set -uo pipefail
set +m 2>/dev/null

WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP=$(mktemp -d)
PIDS=""
cleanup() {
    for p in $PIDS; do kill -9 "$p" 2>/dev/null; done
    [ -d "$TMP" ] && pkill -9 -f "^$TMP/" 2>/dev/null
    rm -rf "$TMP"
}
trap cleanup EXIT
PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }
skip(){ echo "  skip  $1"; }

case "${OS:-}" in
  Windows_NT) echo "test-port-hygiene: SKIP (POSIX sockets required)"; exit 0 ;;
esac

# Base well clear of the ephemeral ranges (Linux 32768+, macOS 49152+), and mixed
# with $$ so this gate does not itself become the fixed-port problem it polices.
BASE=${WYN_TEST_PORT_BASE:-$(( 20000 + ((RANDOM + $$) % 9000) ))}

# ---------------------------------------------------------------- arms 1 and 2
# One source, two behaviours, chosen by argv so there is ONE program to build:
#   walk=0  bind $1 exactly, once   (the old, fixed-port shape)
#   walk=1  walk up from $1 until bind succeeds
# It reports the port it actually got, then serves one request per connection.
cat > "$TMP/srv.wyn" <<'WYN'
fn main() -> int {
    var args = System.args()
    if args.len() < 3 { return 2 }
    var base = args[1].to_int()
    var walk = args[2].to_int()

    var port = base
    var server = -1
    var tries = 0
    while tries < 200 {
        server = Http.serve(port)
        if server > 0 { break }
        if walk == 0 { break }
        port = port + 1
        tries = tries + 1
    }
    if server <= 0 {
        println("bindfail")
        return 1
    }
    println("listening ${port}")
    var n = 0
    while n < 4 {
        var conn = Http.accept_fd(server)
        if conn > 0 {
            var req = Http.read_request(conn)
            if req.len() > 0 {
                Http.respond(Http.fd(req), 200, "text/plain", "hello")
                Http.close_client(Http.fd(req))
            }
            n = n + 1
        }
    }
    return 0
}
WYN

SRV="$TMP/srv.out"
if ! perl -e 'alarm(120); exec @ARGV' -- "$WYN" build "$TMP/srv.wyn" -o "$SRV" > "$TMP/build.log" 2>&1; then
    bad "probe server builds"
    sed -n '1,20p' "$TMP/build.log"
    echo ""
    echo "test-port-hygiene: $PASS pass, $FAIL fail"
    exit 1
fi
ok "probe server builds"

# Read the port a server negotiated out of its own stdout. println() flushes, so
# the line lands as soon as bind() returned; the port used downstream is therefore
# a port whose bind() succeeded in THAT process - no probe-then-hand-over window.
read_port() {   # $1 = log  -> prints the port, or nothing
    local i p
    for i in $(seq 1 200); do
        p=$(sed -n 's/^listening \([0-9][0-9]*\)$/\1/p' "$1" 2>/dev/null | head -1)
        if [ -n "$p" ]; then printf '%s' "$p"; return 0; fi
        grep -q '^bindfail$' "$1" 2>/dev/null && return 1
        sleep 0.1
    done
    return 1
}

start_srv() {   # $1 = log  $2 = base  $3 = walk
    "$SRV" "$2" "$3" > "$1" 2>&1 &
    local p=$!
    disown "$p" 2>/dev/null
    PIDS="$PIDS $p"
    printf '%s' "$p"
}

answers() {     # $1 = port  -> 0 if the server answers "hello"
    local body
    body=$(perl -e 'alarm(10); exec @ARGV' -- curl -s -m 5 "http://127.0.0.1:$1/" 2>/dev/null)
    [ "$body" = "hello" ]
}

# --- arm 1: NEGATIVE CONTROL. A fixed port really does collide. ---------------
FIX_BASE=$((BASE + 100))
p1=$(start_srv "$TMP/fix1.log" "$FIX_BASE" 0)
if ! port1=$(read_port "$TMP/fix1.log"); then
    skip "negative control (first fixed-port server could not bind $FIX_BASE at all)"
else
    p2=$(start_srv "$TMP/fix2.log" "$FIX_BASE" 0)
    # No walk, so this one either wins the port (it cannot: p1 holds it) or says
    # bindfail. Give it a bounded moment to say so.
    got2=""
    for _ in $(seq 1 100); do
        if grep -q '^bindfail$' "$TMP/fix2.log" 2>/dev/null; then got2="bindfail"; break; fi
        got2=$(sed -n 's/^listening \([0-9][0-9]*\)$/\1/p' "$TMP/fix2.log" 2>/dev/null | head -1)
        [ -n "$got2" ] && break
        sleep 0.1
    done
    if [ "$got2" = "bindfail" ]; then
        ok "negative control: a second server on the SAME fixed port cannot bind (so a fixed port IS a collision)"
    else
        bad "negative control: two servers both bound fixed port $FIX_BASE (got [$got2]) - the collision hazard changed, arms 2-4 now prove nothing"
    fi
    kill -9 "$p2" 2>/dev/null
fi
kill -9 "$p1" 2>/dev/null

# --- arm 2: THE FIX. Two walkers off ONE base both get a port and both work. --
WALK_BASE=$((BASE + 200))
w1=$(start_srv "$TMP/w1.log" "$WALK_BASE" 1)
wp1=$(read_port "$TMP/w1.log") || wp1=""
w2=$(start_srv "$TMP/w2.log" "$WALK_BASE" 1)
wp2=$(read_port "$TMP/w2.log") || wp2=""

if [ -z "$wp1" ] || [ -z "$wp2" ]; then
    bad "two walking servers off base $WALK_BASE both bind (got [$wp1] and [$wp2])"
elif [ "$wp1" = "$wp2" ]; then
    bad "two walking servers off base $WALK_BASE reported the SAME port $wp1"
else
    ok "two concurrent servers off one base bind DIFFERENT ports ($wp1, $wp2)"
    if answers "$wp1" && answers "$wp2"; then
        ok "both concurrent servers answer a real request"
    else
        bad "a concurrent server bound a port but did not answer ($wp1, $wp2)"
    fi
fi
kill -9 "$w1" 2>/dev/null; kill -9 "$w2" 2>/dev/null

# ------------------------------------------------------------------ arm 3 + 4
# Tripwires. Cheap, and they catch the NEXT test that reintroduces a fixed port -
# which no execution arm can do, because a new test's collision only shows up once
# somebody runs two copies at the same time.
LISTEN_RE='(Http\.serve|Http\.listen|Net\.listen|Http::serve|Net::listen)[[:space:]]*\('

# 3a. A literal argument is always wrong (port 0 excepted - that is "ask the OS").
offenders=$(grep -rnE "${LISTEN_RE}[0-9]+\)" "$ROOT/tests" 2>/dev/null \
            | grep -vE ':[0-9]+:[[:space:]]*(//|#|\*)' \
            | grep -vE "${LISTEN_RE}0\)" \
            | grep -vE '__pycache__|/golden/|\.wyn\.c:' || true)
if [ -z "$offenders" ]; then
    ok "no test source passes a literal port to a listen call"
else
    bad "test sources bind literal ports (use a walk from a randomised base):"
    printf '%s\n' "$offenders" | sed 's|^'"$ROOT"'/|          |'
fi

# 3b. A variable argument is not enough either: `var port = 18744; Http.serve(port)`
# is just as fixed. What makes it safe is the WALK, so require that every listen
# call is followed within 3 lines by the walk's exit (`break`) - or that it asks the
# OS for a port outright. This is the arm that catches the shape the literal grep
# above cannot see, and it was the shape 6 of the 13 offending sites actually had.
walkless=""
while IFS= read -r f; do
    [ -f "$f" ] || continue
    w=$(awk -v F="$f" '
        /^[[:space:]]*(\/\/|#)/ { next }
        countdown > 0 { if ($0 ~ /break/) { countdown = 0 } else { countdown--
                          if (countdown == 0) print F ":" pend_no ":" pend } }
        $0 ~ /(Http\.serve|Http\.listen|Net\.listen|Http::serve|Net::listen)[[:space:]]*\(/ {
            if ($0 ~ /\([[:space:]]*0[[:space:]]*\)/) next
            pend = $0; pend_no = FNR; countdown = 4
        }
        END { if (countdown > 0) print F ":" pend_no ":" pend }
    ' "$f")
    [ -n "$w" ] && walkless="${walkless}${w}
"
done <<EOF
$(grep -rlE "$LISTEN_RE" "$ROOT/tests" 2>/dev/null | grep -vE '__pycache__|/golden/|\.wyn\.c$' || true)
EOF
if [ -z "$(printf '%s' "$walkless" | tr -d '[:space:]')" ]; then
    ok "every listen call in tests/ sits inside a bind walk"
else
    bad "listen calls with no bind walk (a variable holding a fixed port is still fixed):"
    printf '%s' "$walkless" | sed 's|^'"$ROOT"'/|          |'
fi

LOAD="$ROOT/tests/errors/run_http_server_load_test.sh"
if [ ! -f "$LOAD" ]; then
    skip "load gate present"
elif grep -qE '^[A-Z_]*PORT[A-Z_]*=[0-9]+' "$LOAD"; then
    bad "the load gate assigns a hard-coded port:"
    grep -nE '^[A-Z_]*PORT[A-Z_]*=[0-9]+' "$LOAD" | sed 's/^/          /'
else
    ok "the load gate carries no hard-coded port assignment"
fi

echo ""
echo "test-port-hygiene: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
