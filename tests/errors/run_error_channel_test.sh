#!/bin/bash
# THE STDLIB'S EXISTING ERROR CHANNELS MUST BE READABLE AND MUST NOT GO STALE.
#
# This gate does NOT settle the #477 design question (whether File.read should return
# a Result, panic, or keep returning ""). It pins the three things that were broken
# about the machinery already in the tree, so that whichever answer is chosen, the
# channel underneath it works:
#
# 1. `last_error_get()` returns a STRING. It was in the checker's untyped
#    fall-through list, so it type-checked as `int` while the C function returns
#    `char*` - a user who found it got an integer. That is why it had zero callers
#    anywhere in the repo, the site or the sample apps. Same class as #468.
#
# 2. A FAILED File operation records a REASON, with the OS error attached. Only
#    file_read, file_write, file_mkdir and file_rmdir wrote `last_error`; copy, move,
#    delete, append, list_dir and size all failed silently. `File.size` on a missing
#    file returning 0 - identical to a real empty file - is the sharpest of those.
#
# 3. `http_status()` is not stale after a failure. http_request cleared
#    http_last_error on entry but never http_last_status, which is assigned only once
#    a response arrives, so after a failed call it reported the PREVIOUS request's
#    status. The HTTPS path already reset it; plain HTTP did not.
#
# Arm 3 needs a request that FAILS without a response. It uses a port nothing is
# listening on, obtained from the kernel and then released, so the connection is
# refused rather than timing out - no network access and no fixed port.
set -uo pipefail

WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d)
PYSRV=""
cleanup() { [ -n "$PYSRV" ] && kill -9 "$PYSRV" 2>/dev/null; rm -rf "$TMP"; }
trap cleanup EXIT

PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

echo "=== the stdlib's error channels must be readable (#477) ==="

run_wyn() {  # $1 label, stdin = source. Echoes the program's stdout.
    local label="$1"
    cat > "$TMP/$label.wyn"
    if ! TMPDIR="$TMP" perl -e 'alarm(180); exec @ARGV' -- \
            "$WYN" build "$TMP/$label.wyn" -o "$TMP/$label.out" > "$TMP/$label.build" 2>&1; then
        echo "__BUILD_FAILED__"
        return 1
    fi
    TMPDIR="$TMP" perl -e 'alarm(60); exec @ARGV' -- "$TMP/$label.out" 2>&1
}

# --- 1. last_error_get() is a string, and reading a missing file sets it ------
# `.len()` on the result is the discriminator: it only type-checks on a string, so a
# build failure here IS the regression (the symbol typed as int again).
out=$(run_wyn readable <<'EOF'
fn main() {
    var c = File.read("/definitely/not/here/wyn-missing.txt")
    var e = last_error_get()
    println("len=${e.len()}")
    println("err=${e}")
}
EOF
)
if [ "$out" = "__BUILD_FAILED__" ]; then
    bad "last_error_get() type-checks as a string (string methods are callable on it)"
    grep -iE 'error' "$TMP/readable.build" | head -3 | sed 's/^/          /'
else
    ok "last_error_get() type-checks as a string (string methods are callable on it)"
    if printf '%s' "$out" | grep -qE '^len=[1-9][0-9]*$'; then
        ok "a failed File.read records a non-empty reason"
    else
        bad "a failed File.read records a non-empty reason (got [$(printf '%s' "$out" | head -1)])"
    fi
    # The OS reason is what makes it worth reading - "cannot open" alone does not
    # distinguish absent from unreadable.
    if printf '%s' "$out" | grep -qiE 'no such file|cannot find|not found'; then
        ok "the reason carries the OS error, not just a generic message"
    else
        bad "the reason carries the OS error [$(printf '%s' "$out" | grep '^err=' | head -1)]"
    fi
fi

# --- 2. the File operations that used to fail silently now record a reason ----
out=$(run_wyn covered <<'EOF'
fn main() {
    // Each of these failed with no recorded reason before #477.
    var sz = File.size("/definitely/not/here/wyn-missing.txt")
    println("size=${sz} err=${last_error_get().len()}")

    var d = File.delete("/definitely/not/here/wyn-missing.txt")
    println("delete=${d} err=${last_error_get().len()}")

    var c = File.copy("/definitely/not/here/wyn-missing.txt", "/tmp/wyn-copy-target")
    println("copy=${c} err=${last_error_get().len()}")
}
EOF
)
if [ "$out" = "__BUILD_FAILED__" ]; then
    bad "the File coverage program builds"
    grep -iE 'error' "$TMP/covered.build" | head -3 | sed 's/^/          /'
else
    for op in size delete copy; do
        line=$(printf '%s' "$out" | grep "^$op=" | head -1)
        n=$(printf '%s' "$line" | sed -n 's/.*err=\([0-9]*\)$/\1/p')
        if [ -n "$n" ] && [ "$n" -gt 0 ] 2>/dev/null; then
            ok "a failed File.$op records a reason"
        else
            bad "a failed File.$op records a reason (got [$line])"
        fi
    done
fi

# --- 3. http_status() is not stale after a failed request --------------------
# THIS ARM NEEDS A SUCCESSFUL REQUEST FIRST, and the first version of it did not have
# one. It asserted only that the status was 0 after two FAILING requests - but
# http_last_status starts at 0, so that arm passed with or without the fix. A vacuous
# control, caught by mutation-testing it rather than by reading it.
#
# So: serve one real 200 from a throwaway python server, confirm the status is 200,
# then make a request that cannot connect and require the status to be 0. Without the
# reset, the second call reports the FIRST call's 200 - which is the bug.
LIVE=$(python3 -c '
import socket
s = socket.socket(); s.bind(("127.0.0.1", 0))
print(s.getsockname()[1]); s.close()')
DEAD=$(python3 -c '
import socket
s = socket.socket(); s.bind(("127.0.0.1", 0))
print(s.getsockname()[1]); s.close()')

python3 - "$LIVE" > "$TMP/pysrv.log" 2>&1 <<'PYSRV_EOF' &
import http.server, socketserver, sys
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        body = b"hello"
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)
    def log_message(self, *a):
        pass
socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(("127.0.0.1", int(sys.argv[1])), H) as srv:
    srv.serve_forever()
PYSRV_EOF
PYSRV=$!
disown "$PYSRV" 2>/dev/null

# Wait for it to accept rather than sleeping a guess.
up=0
for _i in $(seq 1 100); do
    if python3 -c '
import socket, sys
try:
    socket.create_connection(("127.0.0.1", int(sys.argv[1])), timeout=0.5).close()
except Exception:
    sys.exit(1)
' "$LIVE" 2>/dev/null; then up=1; break; fi
    sleep 0.1
done

if [ "$up" != "1" ]; then
    bad "the probe HTTP server came up (harness setup)"
else
    out=$(run_wyn httpstale <<EOF
fn main() {
    var a = Http.get("http://127.0.0.1:$LIVE/")
    println("s1=\${http_status()}")
    // Nothing is listening here: connection refused, so there is no status at all.
    var b = Http.get("http://127.0.0.1:$DEAD/")
    println("s2=\${http_status()} elen=\${http_error().len()}")
}
EOF
)
    if [ "$out" = "__BUILD_FAILED__" ]; then
        bad "the http status program builds"
        grep -iE 'error' "$TMP/httpstale.build" | head -3 | sed 's/^/          /'
    else
        s1=$(printf '%s' "$out" | sed -n 's/^s1=\([0-9-]*\)$/\1/p')
        s2=$(printf '%s' "$out" | sed -n 's/^s2=\([0-9-]*\) .*/\1/p')
        # Without this first assertion the next one is meaningless: 0 after a failure
        # proves nothing if the channel never held anything else.
        if [ "$s1" = "200" ]; then ok "http_status() is 200 after a successful request"
        else bad "http_status() is 200 after a successful request (got [$s1]) - the next arm is vacuous without it"; fi
        if [ "$s2" = "0" ]; then ok "http_status() is 0 after a FAILED request, not the previous 200"
        else bad "http_status() is 0 after a FAILED request (got [$s2] - stale from the 200 above)"; fi
        elen=$(printf '%s' "$out" | sed -n 's/.*elen=\([0-9]*\)$/\1/p')
        if [ -n "$elen" ] && [ "$elen" -gt 0 ] 2>/dev/null; then
            ok "http_error() records a reason for the failure"
        else
            bad "http_error() records a reason for the failure (got elen=[$elen])"
        fi
    fi
    kill -9 "$PYSRV" 2>/dev/null
fi

echo ""
echo "error-channel: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
