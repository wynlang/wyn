#!/usr/bin/env bash
# An unknown method on a builtin stdlib namespace must be a CHECK error -
# IN EVERY SPELLING.
#
# THE ORIGINAL DEFECT (dev @ 11637e24, 2026-09-22)
#
# For all 31 namespaces probed, `wyn check` exited 0 on
#
#     x = Time.no_such_method_xyz()
#
# and the program then died in clang on a symbol (`Time_no_such_method_xyz`) that
# appears nowhere in the user's source. `wyn check`, the command whose entire job is
# to answer "is this program valid?", said yes to a program that cannot be built.
#
# WHY THIS FILE IS PARAMETERISED BY SPELLING
#
# The first fix wired the rule to the `.` spelling only, and this gate tested only
# `.`, so it went green while `Time::no_such_xyz()` still passed `wyn check` on all
# 31 namespaces (measured on dev @ 82f8d2bc: 31/31 rejected for `.`, 0/31 for `::`).
# `::` is the spelling the docs and examples use most - `Time::sleep` is the single
# most common stdlib call in this repo's corpus - so the more common form was the
# unchecked one.
#
# A gate that tests one spelling of a two-spelling rule is what let that through.
# So EVERY arm below runs under both separators. Adding a third spelling means
# adding it to SEPARATORS here and watching this whole file fail until the rule
# covers it.
#
# HOW THE RULE DECIDES
#
# The checker's namespace return-type tables are deliberately partial: 37 of the 217
# distinct `Namespace.method` calls in this repo's own .wyn corpus are absent from
# them, so "not in the table" could never mean "does not exist". The authority is the
# C symbol the call lowers to, and the checker rejects a namespace call only when the
# symbol it will emit is declared nowhere in the runtime translation unit.
#
# THE TWO SPELLINGS LOWER DIFFERENTLY, and the rule has to know that or it would
# certify calls it cannot build. wyn_namespace_c_symbol_spelled() holds both
# mappings, so the check is asked per spelling. Some arms below therefore differ by
# spelling ON PURPOSE - `HashMap::set_int` is genuinely unbuildable while
# `HashMap.set_int` works, and `File::is_dir` renders `1` where `File.is_dir` renders
# `true`, because they are different C functions. Converging the two lowerings is a
# separate fix (it needs the File runtime types reconciled first) and is filed, not
# attempted here. These arms pin today's truth per spelling instead of pretending.
#
# BOTH DIRECTIONS ARE PINNED. A namespace diagnostic that breaks `Time.now()` would
# be worse than the bug it fixes, so the good programs are checked by value.
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

NS_LIST="Args Base64 Crypto Csv Data DateTime Db Encoding Env File HashMap HashSet
         Http Json Log Math Net Os Path Process Regex Socket StringBuilder System
         Task Time Toml Url Uuid Ws Shared"

# Every arm, for one separator. $1 is the separator as written in Wyn source.
run_spelling_arms() {
    local SEP="$1"
    # How a bool-returning File call RENDERS in this spelling. The two spellings lower
    # File to different C functions - `File.is_dir` -> `bool File_is_dir`, `File::is_dir`
    # -> `int file_is_dir` - so one prints `true` and the other `1`. That divergence is
    # dev's, not this change's, and it is filed separately; the arm pins today's truth
    # per spelling rather than pretending they agree.
    local BOOLTXT="$2"
    local tag="$SEP"

    # --- every builtin namespace rejects an unknown method AT CHECK TIME ---------
    local rejected=0 total=0 n out rc
    for n in $NS_LIST; do
        total=$((total+1))
        printf 'fn main() {\n  x = %s%sno_such_method_xyz()\n  print(1)\n}\n' "$n" "$SEP" > p.wyn
        out="$("$WYN_ABS" check p.wyn 2>&1)"
        rc=$?
        if [ "$rc" -ne 0 ] && echo "$out" | grep -q "unknown method '$n${SEP}no_such_method_xyz'"; then
            rejected=$((rejected+1))
        else
            echo "        $n NOT rejected (rc=$rc): $(echo "$out" | head -2 | tr '\n' ' ')"
        fi
    done
    check "[$tag] all builtin namespaces reject an unknown method at check time" \
        "$rejected/$total" "$total/$total"

    # The message must name the line and point at the docs, not leak a C symbol.
    printf 'fn main() {\n  x = Time%sno_such_method_xyz()\n  print(1)\n}\n' "$SEP" > one.wyn
    out="$("$WYN_ABS" check one.wyn 2>&1)"
    # "Error at line 2", not a bare "line 2": the unused-variable WARNING on the same
    # line also says "(line 2)", so the loose grep passed before the fix existed.
    check "[$tag] names the line"             "$(echo "$out" | grep -c "Error at line 2")" "1"
    check "[$tag] points at the stdlib docs"  "$(echo "$out" | grep -c "stdlib docs")"     "1"
    check "[$tag] does not leak the C symbol" "$(echo "$out" | grep -c "Time_no_such")"    "0"

    # --- did-you-mean ----------------------------------------------------------
    # The exemplar is right-name/wrong-namespace, the typo a user actually makes:
    # the real one is DateTime.millis(). The suggestion must come back in the
    # separator the user TYPED - telling someone who wrote `Time::millis()` to try
    # `DateTime.millis()` hands them a second edit.
    printf 'fn main() {\n  var t = Time%smillis()\n  print(t)\n}\n' "$SEP" > wrongns.wyn
    out="$("$WYN_ABS" check wrongns.wyn 2>&1)"; rc=$?
    check "[$tag] Time${SEP}millis is rejected" \
        "$([ "$rc" -ne 0 ] && echo yes || echo no)" "yes"
    # The greps allow anything between "mean:" and the name: an ANSI colour reset sits
    # there, so a literal "Did you mean: X" match can never succeed.
    check "[$tag] suggests DateTime${SEP}millis(), in the same spelling" \
        "$(echo "$out" | grep -c "Did you mean:.*DateTime${SEP}millis()")" "1"

    # A near-miss WITHIN the namespace suggests that namespace's own spelling.
    printf 'fn main() {\n  if File%sexist("x.txt") { print(1) }\n}\n' "$SEP" > nearmiss.wyn
    out="$("$WYN_ABS" check nearmiss.wyn 2>&1)"
    check "[$tag] File${SEP}exist suggests File${SEP}exists()" \
        "$(echo "$out" | grep -c "Did you mean:.*File${SEP}exists()")" "1"

    # A near miss with NO candidate close enough is still rejected - a missing
    # suggestion must not become a missing error.
    printf 'fn main() {\n  var s = File%sread_al("x.txt")\n  print(s)\n}\n' "$SEP" > nosugg.wyn
    "$WYN_ABS" check nosugg.wyn > /dev/null 2>&1
    check "[$tag] File${SEP}read_al is rejected even with no suggestion" \
        "$([ $? -ne 0 ] && echo yes || echo no)" "yes"

    # --- the good programs -----------------------------------------------------
    # A false positive here breaks working code, so this arm is deliberately wide:
    # it covers every namespace whose lowering is SPECIAL, which is exactly where a
    # naive allowlist goes wrong: HashMap/HashSet/Random lower to a LOWERCASE C prefix
    # in both spellings, Regex does so in `.` only, and File lowers to `File_` under `.`
    # but `file_` under `::`. Only calls that BOTH spellings support live here; the
    # dot-only ones have their own arms below, so neither spelling's coverage is lost.
    cat > good_check.wyn <<EOF
fn main() {
    print("\${Time${SEP}now()}")
    print("\${DateTime${SEP}millis()}")
    print("\${Math${SEP}abs(-3)}")
    print("\${Path${SEP}basename("/a/b.txt")}")
    print("\${Os${SEP}platform()}")
    print("\${Base64${SEP}encode("hi")}")
    print("\${Crypto${SEP}sha256("hi")}")
    print("\${Encoding${SEP}hex_encode("hi")}")
    print("\${Uuid${SEP}v4()}")
    print("\${Url${SEP}encode("a b")}")
    print("\${Env${SEP}get("HOME")}")
    print("\${File${SEP}exists("/tmp")}")
    print("\${File${SEP}read("/etc/hostname")}")
    var m = HashMap${SEP}new()
    HashMap${SEP}set(m, "k", "v")
    print("\${HashMap${SEP}has(m, "k")}")
    var s = HashSet${SEP}new()
    HashSet${SEP}add(s, "a")
    print("\${HashSet${SEP}contains(s, "a")}")
    print("\${Regex${SEP}find_all("a1", "[0-9]").len()}")
    print("\${Random${SEP}float() >= 0.0}")
    print("\${String${SEP}from_chars([72, 105])}")
    var b = StringBuilder${SEP}new()
    StringBuilder${SEP}append(b, "x")
    print("\${StringBuilder${SEP}to_string(b)}")
    var j = Json${SEP}new()
    Json${SEP}set_int(j, "i", 1)
    print("\${Json${SEP}stringify(j)}")
}
EOF
    "$WYN_ABS" check good_check.wyn > check_out.txt 2>&1
    rc=$?
    check "[$tag] the wide good program still checks" \
        "$([ "$rc" -eq 0 ] && echo yes || echo no)" "yes"
    if [ "$rc" -ne 0 ]; then sed -n '1,20p' check_out.txt; fi

    # By VALUE, end to end. This is the arm that catches a lowering that the checker
    # accepts but codegen emits differently - the two must agree, in both spellings.
    #
    # These arms avoid `HashMap::get` (the STRING flavour) on purpose. Reading a
    # string value back out of a map is a pre-existing type error in the `::`
    # spelling alone - measured on a pristine dev @ 82f8d2bc, where
    #     var m = HashMap::new(); HashMap::set(m,"k","v"); print("${HashMap::get(m,"k")}")
    # reports "Expected: int, Got: map (HashMap<string, int>)" while the identical
    # program with `.` checks clean. `has` and the `_int` flavours are fine in both.
    # Filed separately; it is not this rule's doing, and the dot-only arm below keeps
    # the string round-trip covered where it works.
    cat > good_run.wyn <<EOF
fn main() {
    var m = HashMap${SEP}new()
    HashMap${SEP}set(m, "k", "v")
    var s = HashSet${SEP}new()
    HashSet${SEP}add(s, "a")
    print("\${HashMap${SEP}has(m, "k")}|\${HashSet${SEP}contains(s, "a")}|\${String${SEP}from_chars([72, 105])}|\${Math${SEP}abs(-3)}")
}
EOF
    # HashSet.contains is the second field, and it says `true` in BOTH spellings as of
    # V-30. It used to be `1` here: `HashSet` is both a namespace and a registered type,
    # so the dotted form read the set RECEIVER table (bool) and the `::` form read the
    # namespace table (nothing -> int default), and the two printed differently. Both
    # read the one registered `bool` now. Keeping one expected string for both spellings
    # is deliberate - it is what made the split visible.
    check "[$tag] and still runs correctly" \
        "$("$WYN_ABS" run good_run.wyn 2>/dev/null | tail -1)" "true|true|Hi|3"

    # File:: moved from the file_ prefix to File_ when the two spellings were
    # consolidated. Every File_* is a same-arity wrapper over its file_* counterpart,
    # but "should be equivalent" is not evidence - pin it by value.
    cat > good_file.wyn <<EOF
fn main() {
    File${SEP}write("wyn_ns_gate.txt", "hi")
    print("\${File${SEP}read("wyn_ns_gate.txt")}|\${File${SEP}basename("/a/b.txt")}|\${File${SEP}is_dir(".")}")
}
EOF
    check "[$tag] File reads/writes still work by value" \
        "$("$WYN_ABS" run good_file.wyn 2>/dev/null | tail -1)" "hi|b.txt|${BOOLTXT}"

    # --- what must NOT be claimed as an unknown namespace method ----------------
    # A USER module whose name collides with a builtin namespace (`math` is in
    # is_builtin_module's list!) must keep resolving against its own source.
    cat > math.wyn <<'EOF'
pub fn add(a: int, b: int) -> int {
    return a + b
}
EOF
    cat > usermod.wyn <<EOF
import math

fn main() {
    print("\${math${SEP}add(2, 3)}")
}
EOF
    "$WYN_ABS" check usermod.wyn > um.txt 2>&1
    check "[$tag] a user module shadowing a builtin namespace still checks" \
        "$([ $? -eq 0 ] && echo yes || echo no)" "yes"
    check "[$tag] and is not called an unknown namespace method" \
        "$(grep -c "unknown method" um.txt)" "0"
    check "[$tag] and still computes the right answer" \
        "$("$WYN_ABS" run usermod.wyn 2>/dev/null | tail -1)" "5"

    # `math` above is saved by two things at once: no `math_*` symbol is declared
    # anywhere, AND it is a loaded module. `Log` removes the first of those - the
    # runtime really does declare Log_* symbols - so this arm pins that a user module
    # named after a LIVE namespace still resolves against the user's own file.
    cat > Log.wyn <<'EOF'
pub fn tail(n: int) -> int {
    return n
}
EOF
    cat > usermod2.wyn <<EOF
import Log

fn main() {
    print("\${Log${SEP}tail(3)}")
}
EOF
    "$WYN_ABS" check usermod2.wyn > um2.txt 2>&1
    check "[$tag] a user module named after a REAL namespace still checks" \
        "$([ $? -eq 0 ] && echo yes || echo no)" "yes"
    check "[$tag] and is not called an unknown namespace method" \
        "$(grep -c "unknown method" um2.txt)" "0"
}

run_spelling_arms "."  "true"
run_spelling_arms "::" "1"

# The string-valued map round-trip, which #357's version of this gate pinned by
# value. It lives outside run_spelling_arms because `HashMap::get` is a pre-existing
# `::`-only type error (see the note above) - dropping it entirely would have lost
# coverage of the hashmap_get_string rename, which is exactly the kind of quiet
# erosion that let the `::` hole through in the first place.
echo "--- dot-only (HashMap::get is a pre-existing :: type error) ---"
cat > good_run_str.wyn <<'EOF'
fn main() {
    var m = HashMap.new()
    HashMap.set(m, "k", "v")
    print("${HashMap.get(m, "k")}")
}
EOF
check "the string map round-trip still runs correctly (dot)" \
    "$("$WYN_ABS" run good_run_str.wyn 2>/dev/null | tail -1)" "v"

# The OTHER-SPELLING suggestion tier. `HashMap::set_int` lowers to an undeclared
# hashmap_set_int while `HashMap.set_int` lowers to hashmap_insert_int and builds, so
# the useful answer is the spelling that works - and the help must NOT claim Wyn has
# no such function, because it does. (That is the same confidently-wrong help #365
# had to remove from the C-compile-step message.)
cat > otherspelling.wyn <<'EOF'
fn main() {
    var m = HashMap::new()
    HashMap::set_int(m, "n", 1)
    print(1)
}
EOF
out="$("$WYN_ABS" check otherspelling.wyn 2>&1)"; rc=$?
check "a method real in the OTHER spelling is still rejected" \
    "$([ "$rc" -ne 0 ] && echo yes || echo no)" "yes"
check "and is told to use the spelling that builds" \
    "$(echo "$out" | grep -c "Did you mean:.*HashMap.set_int()")" "1"
check "and is NOT told Wyn has no such function" \
    "$(echo "$out" | grep -c "is not a function Wyn knows about")" "0"
check "and the help says the spellings lower differently" \
    "$(echo "$out" | grep -c "lower to different C symbols")" "1"
# The recommended spelling must actually build and run - advice that does not work is
# worse than none.
cat > otherspelling_fixed.wyn <<'EOF'
fn main() {
    var m = HashMap.new()
    HashMap.set_int(m, "n", 7)
    print("${HashMap.get_int(m, "n")}")
}
EOF
check "and that recommended spelling really works" \
    "$("$WYN_ABS" run otherspelling_fixed.wyn 2>/dev/null | tail -1)" "7"

# THE ASYMMETRIC FILE METHOD. `File.read_lines` is the one method in the whole runtime
# where exactly one File prefix is declared: `File_read_lines` exists, `file_read_lines`
# does not. Since `.` lowers File to `File_` and `::` lowers it to `file_`, the dotted
# call builds and the `::` call cannot - and it is the case that proves the rule asks
# about the symbol the SPELLING emits, not about the name in the builtin registry
# (which is keyed by the dotted name and made both look fine). On dev @ 82f8d2bc this
# passed `wyn check` and then failed the C compile.
cat > asym.wyn <<'EOF'
fn main() {
    var ls = File::read_lines("/etc/hostname")
    print(ls.len() >= 0)
}
EOF
out="$("$WYN_ABS" check asym.wyn 2>&1)"; rc=$?
check "File::read_lines is rejected (only File_read_lines is declared)" \
    "$([ "$rc" -ne 0 ] && echo yes || echo no)" "yes"
check "and is pointed at the dotted form, which does build" \
    "$(echo "$out" | grep -c "Did you mean:.*File.read_lines()")" "1"
cat > asym_fixed.wyn <<'EOF'
fn main() {
    var ls = File.read_lines("/etc/hostname")
    print(ls.len() >= 0)
}
EOF
check "and the dotted form really does run" \
    "$("$WYN_ABS" run asym_fixed.wyn 2>/dev/null | tail -1)" "true"

# ---------------------------------------------------------------------------
# An enum variant uses the `::` spelling too. `Shape::Circle(5)` must never be
# read as a namespace method - this is the false positive the `::` half risks and
# the `.` half never could.
# ---------------------------------------------------------------------------
echo "--- spelling-independent ---"
cat > enumvar.wyn <<'EOF'
enum Shape {
    Circle(int),
    Dot
}
fn main() {
    var c = Shape::Circle(5)
    match c {
        Shape::Circle(r) => print("r=${r}"),
        Shape::Dot => print("dot")
    }
}
EOF
"$WYN_ABS" check enumvar.wyn > ev.txt 2>&1
check "an enum variant via :: still checks" \
    "$([ $? -eq 0 ] && echo yes || echo no)" "yes"
check "and is not called an unknown namespace method" \
    "$(grep -c "unknown method" ev.txt)" "0"
check "and still runs correctly" \
    "$("$WYN_ABS" run enumvar.wyn 2>/dev/null | tail -1)" "r=5"

# ---------------------------------------------------------------------------
# The VALUE receivers share one emitter with the namespace rule. A grep for the
# message BODY alone cannot tell one emitter from three, so these pin its header
# too - break it and the namespace, struct and string arms fail together.
# ---------------------------------------------------------------------------
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
check "a struct method typo still says struct" \
    "$(echo "$out" | grep -c "Error at line 4: struct 'P' has no method 'frobnicate'")" "1"
check "a struct method typo is not called a namespace" \
    "$(echo "$out" | grep -c "on namespace")" "0"

printf 'fn main() { print("hi".uppr()) }\n' > strmiss.wyn
out="$("$WYN_ABS" check strmiss.wyn 2>&1)"
check "a string method typo still says string has no method" \
    "$(echo "$out" | grep -c "Error at line 1: string has no method 'uppr'")" "1"
check "a string method typo still suggests .upper()" \
    "$(echo "$out" | grep -c "Did you mean:.*upper()")" "1"

echo ""
echo "namespace-unknown-method: $pass pass, $fail fail"
[ "$fail" -eq 0 ] || exit 1
