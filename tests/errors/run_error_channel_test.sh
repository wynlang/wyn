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
# 4. THE CHANNEL IS READABLE UNDER THE NAME A READER WOULD GUESS, and says "" rather
#    than NULL when nothing failed. Both halves were wrong:
#
#    `File.error()` did not exist - the only spelling was `last_error_get()`, which is
#    why point 1 above could be true and the channel still have zero callers. Being
#    able to read a reason is not the same as being able to find it.
#
#    And both channels returned a NULL char* when there was no error, which printf
#    renders as the literal text `(null)`. So on the SUCCESS path a user saw `(null)`,
#    the obvious test `if File.error() != ""` was false exactly when the call had
#    worked, and `%s` on a null pointer is undefined behaviour besides - glibc and
#    macOS happen to print `(null)`; a platform that does not would crash on success.
#    api-reference.md had already published `""`, so the docs were describing code that
#    did not exist.
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
    // The SUCCESS path's error value. This used to be a NULL char*, which printf
    // rendered as the literal text "(null)" - so \`Http.error() != ""\` was false
    // exactly when the request had worked, and %s on a null pointer is undefined
    // behaviour besides. "" is the contract now.
    println("ok_err=[\${Http.error()}]")
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
        okerr=$(printf '%s' "$out" | sed -n 's/^ok_err=\[\(.*\)\]$/\1/p')
        if [ "$okerr" = "" ]; then
            ok "Http.error() is EMPTY after a successful request, not \"(null)\""
        else
            bad "Http.error() is empty after a successful request (got [$okerr])"
        fi
    fi
    kill -9 "$PYSRV" 2>/dev/null
fi

# --- 4. File.error() - the channel under the name a reader of the File docs guesses --
# The channel itself has existed for a while, but only as `last_error_get()`, and the
# header of this gate records the consequence: ZERO callers in the repo, the site or the
# sample apps. Being able to read it is not the same as being able to FIND it.
out=$(run_wyn fileerr <<'EOF'
fn main() {
    var c = File.read("/definitely/not/here/wyn-missing.txt")
    // The idiom the docs now publish. It only works if "" means no-error.
    if File.error() != "" {
        println("fail=${File.error()}")
    } else {
        println("fail=NONE-REPORTED")
    }
    File.write("/tmp/wyn-errchan-ok.txt", "hello")
    var d = File.read("/tmp/wyn-errchan-ok.txt")
    println("ok=${d} okerr=[${File.error()}]")
}
EOF
)
if [ "$out" = "__BUILD_FAILED__" ]; then
    bad "File.error() is reachable from Wyn"
    grep -iE 'error' "$TMP/fileerr.build" | head -3 | sed 's/^/          /'
else
    ok "File.error() is reachable from Wyn"
    if printf '%s' "$out" | grep -qiE '^fail=.*(no such file|cannot find|not found)'; then
        ok "File.error() reports why a failed File.read failed"
    else
        bad "File.error() reports why a failed File.read failed (got [$(printf '%s' "$out" | grep '^fail=' | head -1)])"
    fi
    # The success path. This is the arm that fails if the channel goes back to NULL:
    # `okerr=[(null)]` rather than `okerr=[]`, and the `!= ""` test above inverts.
    if printf '%s' "$out" | grep -q '^ok=hello okerr=\[\]$'; then
        ok "File.error() is EMPTY after a successful File.read, not \"(null)\""
    else
        bad "File.error() is empty after a successful read (got [$(printf '%s' "$out" | grep '^ok=' | head -1)])"
    fi
    # By value, across the whole output: "(null)" is the exact text the old NULL return
    # produced, and it is what a user would have had to compare against.
    if printf '%s' "$out" | grep -q '(null)'; then
        bad "no error channel renders as the literal \"(null)\""
    else
        ok "no error channel renders as the literal \"(null)\""
    fi
fi

# File.error takes NO argument. It is registered with reg_fn rather than the permissive
# builtin table, so the arity is checked; the table sets is_variadic and would accept
# `File.error(path)` and then emit a call C cannot type.
cat > "$TMP/arity.wyn" <<'EOF'
fn main() {
    println(File.error("/some/path"))
}
EOF
if TMPDIR="$TMP" perl -e 'alarm(60); exec @ARGV' -- "$WYN" check "$TMP/arity.wyn" > "$TMP/arity.log" 2>&1; then
    bad "File.error(path) is rejected - it takes no argument (it type-checked)"
else
    ok "File.error(path) is rejected - it takes no argument"
fi

echo ""
echo "error-channel: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
