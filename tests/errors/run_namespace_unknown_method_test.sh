#!/usr/bin/env bash
# An unknown method on a builtin stdlib namespace must be a CHECK error.
#
# THE DEFECT (measured on dev @ 11637e24, 2026-09-22)
#
# For all 31 namespaces probed, `wyn check` exited 0 on
#
#     x = Time.no_such_method_xyz()
#
# and the program then died in clang on a symbol (`Time_no_such_method_xyz`) that
# appears nowhere in the user's source. main.c's post-clang translator prints a
# readable message for that, but only AFTER a full codegen + C compile - so
# `wyn check`, the command whose entire job is to answer "is this program valid?",
# said yes to a program that cannot be built. An editor/LSP that runs `check`
# reported nothing at all.
#
# WHY THE CHECKER USED TO STAY SILENT, AND WHAT CHANGED
#
# The checker has a return-type table for namespace methods, but it is deliberately
# partial: 37 of the 217 distinct `Namespace.method` calls in this repo's own .wyn
# corpus are absent from it, so "not in the table" could never mean "does not
# exist". The authority that DOES decide is the C symbol the call lowers to:
# codegen emits it and the C compiler resolves it against the runtime headers.
# wyn_namespace_c_symbol() is now that lowering, single-sourced, and the checker
# rejects a namespace call only when the symbol it will emit is declared nowhere in
# the runtime translation unit. Measured against the corpus: 217 of 217 calls stay
# accepted, and the 31-namespace probe is rejected.
#
# BOTH DIRECTIONS ARE PINNED HERE. A namespace diagnostic that breaks `Time.now()`
# would be worse than the bug it fixes, so the good programs are checked by value.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"

pass=0
fail=0
check() {
    if [ "$2" = "$3" ]; then
        echo "  ok    $1"
        pass=$((pass+1))
    else
        echo "  FAIL  $1"
        echo "          expected: $3"
        echo "          actual:   $2"
        fail=$((fail+1))
    fi
}

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cd "$work" || exit 1

# ---------------------------------------------------------------------------
# ARM 1: every builtin namespace rejects an unknown method AT CHECK TIME.
# ---------------------------------------------------------------------------
NS_LIST="Args Base64 Crypto Csv Data DateTime Db Encoding Env File HashMap HashSet
         Http Json Log Math Net Os Path Process Regex Socket StringBuilder System
         Task Time Toml Url Uuid Ws Shared"
rejected=0
total=0
for n in $NS_LIST; do
    total=$((total+1))
    printf 'fn main() {\n  x = %s.no_such_method_xyz()\n  print(1)\n}\n' "$n" > p.wyn
    out="$("$WYN_ABS" check p.wyn 2>&1)"
    rc=$?
    if [ "$rc" -ne 0 ] && echo "$out" | grep -q "unknown method '$n.no_such_method_xyz'"; then
        rejected=$((rejected+1))
    else
        echo "        $n NOT rejected (rc=$rc): $(echo "$out" | head -2 | tr '\n' ' ')"
    fi
done
check "all builtin namespaces reject an unknown method at check time" "$rejected/$total" "$total/$total"

# The message must name the line and point at the stdlib docs, not leak a C symbol.
printf 'fn main() {\n  x = Time.no_such_method_xyz()\n  print(1)\n}\n' > one.wyn
out="$("$WYN_ABS" check one.wyn 2>&1)"
# "Error at line 2", not a bare "line 2": the unused-variable WARNING on the same
# line also says "(line 2)", so the loose grep passed before the fix existed.
check "names the line"              "$(echo "$out" | grep -c "Error at line 2")"   "1"
check "points at the stdlib docs"   "$(echo "$out" | grep -c "stdlib docs")"       "1"
check "does not leak the C symbol"  "$(echo "$out" | grep -c "Time_no_such")"      "0"

# ---------------------------------------------------------------------------
# ARM 2: did-you-mean. The exemplar is right-name/wrong-namespace, which is the
# typo a user actually makes: the real one is DateTime.millis().
# ---------------------------------------------------------------------------
printf 'fn main() {\n  var t = Time.millis()\n  print(t)\n}\n' > wrongns.wyn
out="$("$WYN_ABS" check wrongns.wyn 2>&1)"; rc=$?
check "Time.millis is rejected"          "$([ "$rc" -ne 0 ] && echo yes || echo no)" "yes"
# The greps allow anything between "mean:" and the name: an ANSI colour reset sits
# there, so a literal "Did you mean: X" match can never succeed.
check "Time.millis suggests DateTime.millis()" \
    "$(echo "$out" | grep -c "Did you mean:.*DateTime.millis()")" "1"

# A near-miss WITHIN the namespace suggests that namespace's own spelling.
printf 'fn main() {\n  if File.exist("x.txt") { print(1) }\n}\n' > nearmiss.wyn
out="$("$WYN_ABS" check nearmiss.wyn 2>&1)"
check "File.exist suggests File.exists()" \
    "$(echo "$out" | grep -c "Did you mean:.*File.exists()")" "1"

# A near miss with NO candidate close enough is still rejected - a missing
# suggestion must not become a missing error.
printf 'fn main() {\n  var s = File.read_al("x.txt")\n  print(s)\n}\n' > nosugg.wyn
"$WYN_ABS" check nosugg.wyn > /dev/null 2>&1
check "File.read_al is rejected even with no suggestion" \
    "$([ $? -ne 0 ] && echo yes || echo no)" "yes"

# ---------------------------------------------------------------------------
# ARM 3: the good programs. A false positive here breaks working code, so this
# arm is deliberately wide - it covers every namespace whose lowering is special
# (HashMap/HashSet/Regex/Random lower to a LOWERCASE C prefix; HashMap.set_int
# lowers to hashmap_insert_int; String.char to String_char_from_int; Task.try_recv
# to Task_try_recv_opt), which is exactly where a naive allowlist goes wrong.
# ---------------------------------------------------------------------------
cat > good_check.wyn <<'EOF'
fn main() {
    // plain Ns_method lowering
    print("${Time.now()}")
    print("${DateTime.millis()}")
    print("${Math.abs(-3)}")
    print("${Path.basename("/a/b.txt")}")
    print("${Os.platform()}")
    print("${Base64.encode("hi")}")
    print("${Crypto.sha256("hi")}")
    print("${Encoding.hex_encode("hi")}")
    print("${Uuid.v4()}")
    print("${Url.encode("a b")}")
    print("${Env.get("HOME")}")
    print("${File.exists("/tmp")}")
    print("${System.exec("true")}")
    // lowercase-prefix namespaces
    var m = HashMap.new()
    HashMap.set(m, "k", "v")
    HashMap.set_int(m, "n", 1)
    print("${HashMap.has(m, "k")}")
    print("${HashMap.get(m, "k")}")
    print("${HashMap.get_int(m, "n")}")
    var s = HashSet.new()
    HashSet.add(s, "a")
    print("${HashSet.contains(s, "a")}")
    print("${Regex.match("a", "a")}")
    print("${Random.float()}")
    // per-method renames
    print("${String.char(65)}")
    print("${String.from_chars([72, 105])}")
    // builder + json
    var b = StringBuilder.new()
    StringBuilder.append(b, "x")
    print("${StringBuilder.to_string(b)}")
    var j = Json.new()
    Json.set_int(j, "i", 1)
    print("${Json.stringify(j)}")
}
EOF
"$WYN_ABS" check good_check.wyn > check_out.txt 2>&1
rc=$?
check "the wide good program still checks" "$([ "$rc" -eq 0 ] && echo yes || echo no)" "yes"
if [ "$rc" -ne 0 ]; then sed -n '1,20p' check_out.txt; fi

# By VALUE, end to end: a diagnostics change must not alter what a program prints.
cat > good_run.wyn <<'EOF'
fn main() {
    var m = HashMap.new()
    HashMap.set(m, "k", "v")
    HashMap.set_int(m, "n", 7)
    var s = HashSet.new()
    HashSet.add(s, "a")
    print("${HashMap.get(m, "k")}|${HashMap.get_int(m, "n")}|${HashSet.contains(s, "a")}|${String.char(65)}|${Math.abs(-3)}")
}
EOF
check "and still runs correctly" "$("$WYN_ABS" run good_run.wyn 2>/dev/null | tail -1)" "v|7|1|A|3"

# ---------------------------------------------------------------------------
# ARM 4: what must NOT be claimed as an unknown namespace method.
# ---------------------------------------------------------------------------
# A USER module whose name collides with a builtin namespace (`math` is in
# is_builtin_module's list!) must keep resolving against its own source. Without
# this guard every `math.add()` in tests/modules stops compiling.
cat > math.wyn <<'EOF'
pub fn add(a: int, b: int) -> int {
    return a + b
}
EOF
cat > usermod.wyn <<'EOF'
import math

fn main() {
    print("${math.add(2, 3)}")
}
EOF
"$WYN_ABS" check usermod.wyn > um.txt 2>&1
check "a user module shadowing a builtin namespace still checks" \
    "$([ $? -eq 0 ] && echo yes || echo no)" "yes"
check "and is not called an unknown namespace method" \
    "$(grep -c "unknown method" um.txt)" "0"

# `math` above is saved by two things at once: no `math_*` symbol is declared
# anywhere, AND it is a loaded module. `Log` removes the first of those - the runtime
# really does declare Log_* symbols - so this arm pins that a user module named after
# a LIVE namespace still resolves against the user's own file.
cat > Log.wyn <<'EOF'
pub fn tail(n: int) -> int {
    return n
}
EOF
cat > usermod2.wyn <<'EOF'
import Log

fn main() {
    print("${Log.tail(3)}")
}
EOF
"$WYN_ABS" check usermod2.wyn > um2.txt 2>&1
check "a user module named after a REAL namespace still checks" \
    "$([ $? -eq 0 ] && echo yes || echo no)" "yes"
check "and is not called an unknown namespace method" \
    "$(grep -c "unknown method" um2.txt)" "0"

# A struct method typo keeps ITS OWN diagnostic - the shared emitter must not
# relabel a value receiver as a namespace.
# NOT written as `print("${p.frobnicate()}")`: inside an interpolation the method
# token carries line 1, so this diagnostic reports "Error at line 1". That is a
# pre-existing span bug, unrelated and not fixed here - the call is spelled plainly
# so this arm measures the emitter, not that bug.
cat > structmiss.wyn <<'EOF'
struct P { x: int }
fn main() {
    var p = P { x: 1 }
    print(p.frobnicate())
}
EOF
out="$("$WYN_ABS" check structmiss.wyn 2>&1)"
# One emitter renders all three receivers, so all three must carry its header. A
# grep for the message BODY alone cannot tell one emitter from three.
check "a struct method typo still says struct" \
    "$(echo "$out" | grep -c "Error at line 4: struct 'P' has no method 'frobnicate'")" "1"
check "a struct method typo is not called a namespace" \
    "$(echo "$out" | grep -c "on namespace")" "0"

# A string method typo keeps PR #115's wording and its suggestion.
printf 'fn main() { print("hi".uppr()) }\n' > strmiss.wyn
out="$("$WYN_ABS" check strmiss.wyn 2>&1)"
check "a string method typo still says string has no method" \
    "$(echo "$out" | grep -c "Error at line 1: string has no method 'uppr'")" "1"
check "a string method typo still suggests .upper()" \
    "$(echo "$out" | grep -c "Did you mean:.*upper()")" "1"

echo ""
echo "namespace-unknown-method: $pass pass, $fail fail"
[ "$fail" -eq 0 ] || exit 1
