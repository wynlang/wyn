#!/bin/bash
# A plain blocking recv()/send() on the main thread with no live task that can
# ever satisfy it used to `pthread_cond_wait` forever - a silent hang with no
# output and no exit (worst possible UX for a plain logic error). The runtime
# now polls + pumps the scheduler and, when nothing ran and zero tasks are in
# flight, reports a deadlock and exits(1) - exactly like select{} already did.
#
# The coroutine path is untouched (it yields, so a cooperative producer still
# feeds the channel); the positive control below exercises that.
#
# Cases 4-6 cover the other half: a statically-known capacity < 1 (channel(0) or
# bare channel()) is a CHECK-TIME error, since unbuffered/rendezvous channels
# are not implemented and a constant bad capacity should never reach runtime.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# 1. recv() on a buffered channel with no sender: must ERROR (rc in [1,127])
#    within the alarm, not hang (alarm-kill → rc >= 128).
printf 'fn main() -> int {\n    ch = channel(1)\n    x = ch.recv()\n    print(x)\n    return 0\n}\n' > "$TMP/recv.wyn"
out=$(perl -e 'alarm(15); exec @ARGV' -- "$WYN" run "$TMP/recv.wyn" 2>&1); rc=$?
if [ $rc -ge 1 ] && [ $rc -lt 128 ] && echo "$out" | grep -q "deadlock"; then ok "recv with no sender errors, no hang"
else bad "recv no-sender (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 2. send() on a FULL buffered channel with no receiver: same. capacity 1, one
#    value already queued, so the second send can never fit and nothing is alive
#    to drain it. (This used to be written as channel(0) - see case 4: capacity-0
#    is now a check-time error, so it could never reach the deadlock backstop.)
printf 'fn main() -> int {\n    ch = channel(1)\n    ch.send(1)\n    ch.send(2)\n    return 0\n}\n' > "$TMP/send.wyn"
out=$(perl -e 'alarm(15); exec @ARGV' -- "$WYN" run "$TMP/send.wyn" 2>&1); rc=$?
if [ $rc -ge 1 ] && [ $rc -lt 128 ] && echo "$out" | grep -q "deadlock"; then ok "send on full channel with no receiver errors, no hang"
else bad "send no-receiver (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 3. Positive control: a spawned cooperative producer feeds the channel, so the
#    main-thread recv succeeds and the program completes cleanly.
printf 'fn producer(ch: int) {\n    Task.send(ch, 7)\n}\nfn main() -> int {\n    ch = channel(1)\n    spawn producer(ch)\n    x = ch.recv()\n    print(x)\n    return 0\n}\n' > "$TMP/ok.wyn"
out=$(perl -e 'alarm(15); exec @ARGV' -- "$WYN" run "$TMP/ok.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q "7"; then ok "spawned producer feeds recv, completes"
else bad "producer control (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 4. channel(0) - a statically-known bad capacity - must be a CHECK-TIME error,
#    not a runtime panic. Unbuffered (rendezvous) channels are not implemented;
#    the runtime rejects capacity < 1, and a constant `0` is knowable at compile
#    time, so `wyn check` reports it with a channel(1) suggestion.
printf 'fn main() -> int {\n    ch = channel(0)\n    ch.send(42)\n    return 0\n}\n' > "$TMP/cap0.wyn"
out=$("$WYN" check "$TMP/cap0.wyn" 2>&1); rc=$?
if [ $rc -ne 0 ] && echo "$out" | grep -q "capacity must be >= 1" && echo "$out" | grep -q "channel(1)"; then
  ok "channel(0) rejected at check time with channel(1) suggestion"
else bad "channel(0) check (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 5. Bare channel() lowers to capacity 0 - same check-time error, not a runtime
#    panic hours later.
printf 'fn main() -> int {\n    ch = channel()\n    ch.send(42)\n    return 0\n}\n' > "$TMP/capnone.wyn"
out=$("$WYN" check "$TMP/capnone.wyn" 2>&1); rc=$?
if [ $rc -ne 0 ] && echo "$out" | grep -q "capacity must be >= 1"; then
  ok "bare channel() rejected at check time"
else bad "bare channel() check (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 6. Negative control on the new diagnostic: a valid capacity still checks clean
#    (the constant fold must not over-reject).
printf 'fn main() -> int {\n    ch = channel(1)\n    ch.send(9)\n    x = ch.recv()\n    print(x)\n    return 0\n}\n' > "$TMP/cap1.wyn"
out=$("$WYN" check "$TMP/cap1.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ]; then ok "channel(1) still checks clean"
else bad "channel(1) check (rc=$rc) [$(echo "$out" | tail -1)]"; fi

echo ""; echo "channel-deadlock: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
