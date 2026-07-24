#!/bin/bash
# A plain blocking recv()/send() on the main thread with no live task that can
# ever satisfy it used to `pthread_cond_wait` forever - a silent hang with no
# output and no exit (worst possible UX for a plain logic error). The runtime
# now polls + pumps the scheduler and, when nothing ran and zero tasks are in
# flight, reports a deadlock and exits(1) - exactly like select{} already did.
#
# The coroutine path is untouched (it yields, so a cooperative producer still
# feeds the channel); the positive control below exercises that.
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

# 2. send() on a full (unbuffered) channel with no receiver: same.
printf 'fn main() -> int {\n    ch = channel(0)\n    ch.send(42)\n    return 0\n}\n' > "$TMP/send.wyn"
out=$(perl -e 'alarm(15); exec @ARGV' -- "$WYN" run "$TMP/send.wyn" 2>&1); rc=$?
if [ $rc -ge 1 ] && [ $rc -lt 128 ] && echo "$out" | grep -q "deadlock"; then ok "send with no receiver errors, no hang"
else bad "send no-receiver (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 3. Positive control: a spawned cooperative producer feeds the channel, so the
#    main-thread recv succeeds and the program completes cleanly.
printf 'fn producer(ch: int) {\n    Task.send(ch, 7)\n}\nfn main() -> int {\n    ch = channel(1)\n    spawn producer(ch)\n    x = ch.recv()\n    print(x)\n    return 0\n}\n' > "$TMP/ok.wyn"
out=$(perl -e 'alarm(15); exec @ARGV' -- "$WYN" run "$TMP/ok.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q "7"; then ok "spawned producer feeds recv, completes"
else bad "producer control (rc=$rc) [$(echo "$out" | tail -1)]"; fi

echo ""; echo "channel-deadlock: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
