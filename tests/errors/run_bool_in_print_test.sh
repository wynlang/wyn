#!/bin/bash
# V-30: ONE bool-in-print authority. A value whose Wyn type is `bool` renders as
# true/false in EVERY spelling, and the same program never disagrees with itself.
#
# What was wrong (current `dev`, one program):
#
#     print("1.5".is_numeric())        -> 1        print("1.5".is_int())      -> false
#     print(3.is_odd())                -> 1        print(x > 2)               -> true
#     print("/tmp".exists())           -> 1        print(ns.contains(3))      -> true
#     print(Json.is_valid(h))          -> 1        var v = Json.is_valid(h)   -> true
#     print(Random::bool())            -> 1        print(Random.bool())       -> true
#
# print(), println(), to_string() and wyn_out_append() dispatch with C's _Generic on
# the STATIC C TYPE they are handed (wyn_runtime.h / wyn_runtime_slim.h), so the
# rendering is decided by the C type at the call site - not by the runtime helper the
# call lowers to. A long list of spellings printed `1` because their runtime helpers
# happen to be declared `int`/`long long`.
#
# The `--release` arm below earns its keep: it found EIGHT functions that
# wyn_runtime_slim.h declared `int` while wyn_runtime.h defines them `bool` (a return
# type mismatch across translation units - undefined behaviour). Invisible on arm64,
# where the return register held a clean 0/1; on x86-64 a `bool` return sets only the
# low byte of eax, so `"abc".ends_with("z")` came back TRUE under --release. Only the
# macos-15-intel CI job could see it.
#
# The 2026-08 fix (run_bool_method_format_test.sh) cast at ONE emit site, which is
# exactly why `arr.contains(3)` was fixed and none of the above were. The rule now
# lives in one place - cg_expr_is_bool_typed() in codegen_expr.c, keyed on the type
# the CHECKER already resolved - and every spelling is routed through it.
#
# THIS GATE IS A TABLE ON PURPOSE. Each row is exercised in four FORMS:
#
#   A  bare            print(expr)
#   B  interpolated    print("label ${expr}")
#   C  via a variable  var v = expr; print("label ${v}")
#   D  as a condition  if expr { ... } else { ... }        <- the control group
#
# so a future change that fixes or breaks ONE spelling cannot pass while another
# regresses. Form D is what proves the cast changed the C TYPE and not the VALUE:
# every condition must land on the same branch as before.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# ---------------------------------------------------------------------------
# THE TABLE: label | Wyn expression | expected rendering
#
# Grouped by SPELLING, because the spelling is the thing under test. Every group
# must agree with every other group about how a bool looks.
# ---------------------------------------------------------------------------
ROWS=(
  # --- method call on a value: the five string predicates named in the ticket ---
  'str_is_alpha|"abc".is_alpha()|true'
  'str_is_alpha_n|"ab1".is_alpha()|false'
  'str_is_digit|"123".is_digit()|true'
  'str_is_digit_n|"12a".is_digit()|false'
  'str_is_alnum|"a1".is_alnum()|true'
  'str_is_ws|"  ".is_whitespace()|true'
  'str_is_numeric|"1.5".is_numeric()|true'
  # ...and its neighbour that ALREADY printed true/false. The pair is the whole
  # bug report: one rule, two renderings, two lines apart.
  'str_is_int|"15".is_int()|true'
  'str_is_int_n|"1.5".is_int()|false'
  # --- more string predicates on the same path ---
  'str_equals|"abc".equals("abc")|true'
  'str_equals_n|"abc".equals("abd")|false'
  'str_is_empty|"".is_empty()|true'
  'str_contains|"abc".contains("b")|true'
  'str_starts|"abc".starts_with("a")|true'
  'str_ends_n|"abc".ends_with("z")|false'
  # --- int predicates ---
  'int_is_even|4.is_even()|true'
  'int_is_odd_n|4.is_odd()|false'
  'int_is_pos|4.is_positive()|true'
  'int_is_neg_n|4.is_negative()|false'
  'int_is_zero|0.is_zero()|true'
  # --- float predicates ---
  'flt_is_nan_n|1.5.is_nan()|false'
  'flt_is_finite|1.5.is_finite()|true'
  'flt_is_inf_n|1.5.is_infinite()|false'
  # --- filesystem predicates ("." is a directory on every platform incl. Windows) ---
  'path_exists|".".exists()|true'
  'path_exists_n|"no_such_path_zz".exists()|false'
  'path_is_dir|".".is_dir()|true'
  'path_is_file_n|".".is_file()|false'
  # --- array methods: the 2026-08 fix already covered these. They are the
  #     REGRESSION half of the table - the one-place rule must keep them right.
  'arr_contains|ns.contains(3)|true'
  'arr_contains_n|ns.contains(42)|false'
  'arr_any|ns.any((n) => n > 8)|true'
  'arr_all_n|ns.all((n) => n > 100)|false'
  'arr_is_empty_n|ns.is_empty()|false'
  'sarr_contains|ss.contains("b")|true'
  # --- Option / Result predicates ---
  'opt_is_some|o.is_some()|true'
  'opt_is_none_n|o.is_none()|false'
  'res_is_ok|r.is_ok()|true'
  'res_is_err_n|r.is_err()|false'
  # --- direct namespace call, DOT spelling (an EXPR_METHOD_CALL) ---
  'ns_dot_json_valid|Json.is_valid(h)|true'
  'ns_dot_json_bool|Json.get_bool(h, "ok")|true'
  'ns_dot_regex|Regex.match("a", "a")|true'
  'ns_dot_args_n|Args.has("--no-such-flag")|false'
  # --- the SAME calls, COLON spelling. A different AST shape (one EXPR_CALL ident)
  #     lowering to a different C symbol - see #369 - so it is a separate spelling
  #     and gets separate rows, not a comment saying it is the same.
  'ns_col_json_valid|Json::is_valid(h)|true'
  'ns_col_json_bool|Json::get_bool(h, "ok")|true'
  'ns_col_args_n|Args::has("--no-such-flag")|false'
  # --- a namespace that is ALSO a registered type, in both spellings. `HashSet` is
  #     both, so the dotted spelling resolves through the set RECEIVER table and the
  #     `::` spelling through the namespace table - two tables for one call, and they
  #     disagreed. HashMap.has needs no registration (hashmap_has is declared bool in
  #     the runtime) and is here as the row that proves the pair is symmetric.
  'ns_dot_set_contains|HashSet.contains(hs, "a")|true'
  'ns_col_set_contains|HashSet::contains(hs, "a")|true'
  'ns_dot_set_contains_n|HashSet.contains(hs, "zz")|false'
  'ns_col_set_contains_n|HashSet::contains(hs, "zz")|false'
  'ns_dot_map_has|HashMap.has(hm, "k")|true'
  'ns_col_map_has|HashMap::has(hm, "k")|true'
  # --- File.exists / File.is_dir / File.is_file. These reach the runtime through
  #     File_exists / File_is_dir / File_is_file, which the slim header declared `int`
  #     while the archive defines them `bool` - so unlike the `.exists()` METHOD rows
  #     above, these DO compile under --release, and on x86-64 they returned garbage
  #     there. Rows in both spellings so the release arm covers them.
  'ns_dot_file_exists|File.exists(".")|true'
  'ns_col_file_exists|File::exists(".")|true'
  'ns_dot_file_is_dir|File.is_dir(".")|true'
  'ns_dot_file_is_file_n|File.is_file(".")|false'
  # --- CONTROL GROUP: bool sources that were ALREADY correct. If one of these
  #     changes, the fix reached further than its own rule.
  'ctl_cmp|3 > 2|true'
  'ctl_cmp_n|2 > 3|false'
  'ctl_fn|big(9)|true'
  'ctl_fn_n|big(1)|false'
  'ctl_lit|true|true'
  'ctl_and_n|true and false|false'
  'ctl_not_n|not true|false'
)

# Shared prelude. Every generated program opens with this, so all four forms are
# talking about the same values.
PRELUDE='fn big(n: int) -> bool { return n > 8 }
fn mkres(n: int) -> Result<int, string> {
    if n > 0 { return Ok(n) }
    return Err("neg")
}
fn main() {
    var ns = [5, 3, 9]
    var ss = ["a", "b"]
    var o = Some(3)
    var r = mkres(5)
    var h = Json.parse("{\"ok\": true}")
    var hs = HashSet.new()
    HashSet.add(hs, "a")
    var hm = HashMap.new()
    HashMap.set_int(hm, "k", 1)'

# --- generate the four programs and their expected output -------------------
gen() {   # gen <form> <wyn-out> <expected-out> [skip-label-regex]
    local form="$1" wf="$2" ef="$3" skip="${4:-}" i=0
    printf '%s\n' "$PRELUDE" > "$wf"
    : > "$ef"
    for row in "${ROWS[@]}"; do
        local label="${row%%|*}" rest="${row#*|}"
        local expr="${rest%|*}" want="${rest##*|}"
        i=$((i + 1))
        if [ -n "$skip" ] && printf '%s' "$label" | grep -qE "$skip"; then continue; fi
        case "$form" in
            A)  # bare print: emit the label on its own line, then the value
                printf '    print("%s")\n    print(%s)\n' "$label" "$expr" >> "$wf"
                printf '%s\n%s\n' "$label" "$want" >> "$ef" ;;
            B)  printf '    print("%s ${%s}")\n' "$label" "$expr" >> "$wf"
                printf '%s %s\n' "$label" "$want" >> "$ef" ;;
            C)  printf '    var v%d = %s\n    print("%s ${v%d}")\n' "$i" "$expr" "$label" "$i" >> "$wf"
                printf '%s %s\n' "$label" "$want" >> "$ef" ;;
            D)  printf '    if %s { print("%s T") } else { print("%s F") }\n' \
                       "$expr" "$label" "$label" >> "$wf"
                if [ "$want" = "true" ]; then printf '%s T\n' "$label" >> "$ef"
                else printf '%s F\n' "$label" >> "$ef"; fi ;;
        esac
    done
    printf '}\n' >> "$wf"
}

# Compare a program's output against its expected block, and say WHICH row differs.
check_out() {   # check_out <label> <got> <expected-file>
    local what="$1" got="$2" ef="$3"
    local want; want=$(cat "$ef")
    if [ "$got" = "$want" ]; then ok "$what"; return 0; fi
    bad "$what"
    diff <(printf '%s\n' "$want") <(printf '%s\n' "$got") | head -20 | sed 's/^/        /'
    return 1
}

for form in A B C D; do
    gen "$form" "$TMP/form_$form.wyn" "$TMP/form_$form.expected"
done

# --- form A..D through `wyn run` --------------------------------------------
for form in A B C D; do
    case "$form" in
        A) desc="bare print(expr)" ;;
        B) desc="interpolated \${expr}" ;;
        C) desc="through a variable" ;;
        D) desc="as an if-condition (values unchanged)" ;;
    esac
    out=$(cd "$TMP" && "$WYN_ABS" run "form_$form.wyn" 2>&1); code=$?
    got=$(printf '%s' "$out" | grep -vE 'Compiled|^Warning|^Building|^Built')
    if [ $code -ne 0 ]; then
        bad "form $form ($desc) did not run"; printf '%s\n' "$out" | head -6 | sed 's/^/        /'
    else
        check_out "form $form: $desc" "$got" "$TMP/form_$form.expected"
    fi
done

# --- the failure mode named exactly ----------------------------------------
# Every line of forms B and C is "<label> <value>". A C-ism leaking is a trailing
# bare 1 or 0. Assert its ABSENCE directly, so a future regression is reported as
# "a 1/0 leaked" and not only as a diff.
leak=0
for form in B C; do
    out=$(cd "$TMP" && "$WYN_ABS" run "form_$form.wyn" 2>&1)
    if printf '%s' "$out" | grep -qE '^[a-z_]+ (1|0)$'; then
        bad "a 1/0 leaked into user output (form $form)"
        printf '%s' "$out" | grep -E '^[a-z_]+ (1|0)$' | head -6 | sed 's/^/        /'
        leak=1
    fi
done
[ "$leak" -eq 0 ] && ok "no bool printed as 1/0 in any spelling"

# --- THE SPELLINGS MUST AGREE WITH EACH OTHER ------------------------------
# Forms A, B and C are three renderings of one value. Comparing them to each other
# (not just to the table) is what catches "spelling X was fixed, spelling Y was not"
# even if someone updates the table to match the broken behaviour.
a_vals=$(cd "$TMP" && "$WYN_ABS" run form_A.wyn 2>/dev/null | grep -vE 'Compiled|^Warning' | awk 'NR%2==0')
b_vals=$(cd "$TMP" && "$WYN_ABS" run form_B.wyn 2>/dev/null | grep -vE 'Compiled|^Warning' | awk '{print $2}')
c_vals=$(cd "$TMP" && "$WYN_ABS" run form_C.wyn 2>/dev/null | grep -vE 'Compiled|^Warning' | awk '{print $2}')
if [ -n "$a_vals" ] && [ "$a_vals" = "$b_vals" ] && [ "$b_vals" = "$c_vals" ]; then
    ok "bare, interpolated and via-variable spellings all agree"
else
    bad "the spellings disagree with each other"
    diff <(printf '%s\n' "$a_vals") <(printf '%s\n' "$b_vals") | head -10 | sed 's/^/     A|B /'
    diff <(printf '%s\n' "$b_vals") <(printf '%s\n' "$c_vals") | head -10 | sed 's/^/     B|C /'
fi

# --- and through the OTHER TWO compile paths -------------------------------
# `wyn run` (system cc or tcc), `wyn build` (a real binary) and `wyn run --release`
# (the ONLY command that compiles against wyn_runtime_slim.h - see CLAUDE.md) are
# three different headers and three different _Generic tables. A cast that is right
# in one is not thereby right in the others. Fresh file names: `wyn run` caches
# `<file>.out` and `--release` keys its cache separately.
cp "$TMP/form_B.wyn" "$TMP/built.wyn"
out=$(cd "$TMP" && "$WYN_ABS" build built.wyn -o built.bin 2>&1); code=$?
if [ $code -eq 0 ] && [ -x "$TMP/built.bin" ]; then
    got=$(cd "$TMP" && ./built.bin 2>&1)
    check_out "wyn build: the same table in a real binary" "$got" "$TMP/form_B.expected"
else
    bad "wyn build failed"; printf '%s\n' "$out" | head -6 | sed 's/^/        /'
fi

# The release arm skips five rows BY NAME, and no others. Not because they are
# uninteresting - they are among the spellings this fix changes - but because they
# cannot be compiled with `--release` AT ALL, on this commit and on its parent alike:
#
#   .exists() .is_file() .is_dir()  ->  _exists / _is_file / _is_dir
#   .any()    .all()                ->  wyn_arr_any / wyn_arr_all
#
# wyn_runtime.h declares all five (the first three via wyn_interface.h) and
# wyn_runtime_slim.h declares none of them, so the release build dies with "call to
# undeclared function". PRE-EXISTING slim-registry gaps, each verified against a
# compiler built from this branch's parent, and they belong to
# run_release_slim_registry_test.sh - not here. (The .any/.all pair is exactly what
# the 2026-08 bool-method fix added, and that gate never ran `--release`, which is
# how the gap stayed invisible.) Excluded by name with this note rather than left to
# fail, and rather than dropping the rows from the `run`/`build` arms where they pass.
gen B "$TMP/rel.wyn" "$TMP/rel.expected" '^(path_|arr_any|arr_all)'
out=$(cd "$TMP" && "$WYN_ABS" run --release rel.wyn 2>&1); code=$?
got=$(printf '%s' "$out" | grep -vE 'Compiled|^Warning|^Building|^Built')
if [ $code -eq 0 ]; then
    check_out "wyn run --release: the same table against wyn_runtime_slim.h" \
              "$got" "$TMP/rel.expected"
else
    bad "wyn run --release failed"; printf '%s\n' "$out" | head -6 | sed 's/^/        /'
fi

# --- the DECLARATION, not just the value -----------------------------------
# `var v = <bool call>` has to be DECLARED `bool`, and this is asserted on the
# generated C rather than on the output, because on clang/gcc the output cannot tell
# the difference: `__auto_type v = (bool)(...)` also yields a bool there. The
# difference is only visible under TinyCC, where both runtime headers do
# `#ifdef __TINYC__ / #define __auto_type long long` - and `wyn run` uses TinyCC
# whenever the prebuilt runtime lib is absent, i.e. on a fresh checkout. No CI job
# compiles with TinyCC, so an output-only assertion would leave the var-decl half of
# this fix unverified and silently backend-dependent. Asserting the emitted
# declaration tests the thing that is actually claimed.
cat > "$TMP/decl.wyn" <<'EOF'
fn main() {
    var h = Json.parse("{\"ok\": true}")
    var dotv = Json.is_valid(h)
    var colv = Json::is_valid(h)
    var methv = "abc".is_alpha()
    print("${dotv} ${colv} ${methv}")
}
EOF
( cd "$TMP" && "$WYN_ABS" build decl.wyn -o decl.bin >/dev/null 2>&1 )
# --debug is what keeps the generated C on disk; build it a second time for the text.
( cd "$TMP" && "$WYN_ABS" build decl.wyn --debug -o decl2.bin >/dev/null 2>&1 )
if [ -f "$TMP/decl.wyn.c" ]; then
    missing=""
    for v in dotv colv methv; do
        grep -qE "^[[:space:]]*bool $v = " "$TMP/decl.wyn.c" || missing="$missing $v"
    done
    if [ -z "$missing" ]; then
        ok "a bool-initialised local is DECLARED bool (not __auto_type) in every spelling"
    else
        bad "these locals were not declared bool:$missing"
        grep -nE "(dotv|colv|methv) = " "$TMP/decl.wyn.c" | head -4 | sed 's/^/        /'
    fi
else
    bad "wyn build --debug did not leave the generated C on disk"
fi

# --- a nondeterministic bool still has to LOOK like a bool -----------------
# Random.bool() cannot be tabled by value, but it can be tabled by SHAPE - and it is
# the row that proves the `::` spelling goes through the same rule as `.`: it was the
# one call that still printed 1 after the emit-site cast, because the `::` checker
# path never consulted the registered return type at all.
cat > "$TMP/rnd.wyn" <<'EOF'
fn main() {
    print("dot ${Random.bool()}")
    print("colon ${Random::bool()}")
    var a = Random.bool()
    var b = Random::bool()
    print("dotvar ${a}")
    print("colonvar ${b}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run rnd.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   [ "$(printf '%s' "$out" | grep -cE '^(dot|colon|dotvar|colonvar) (true|false)$')" = "4" ]; then
    ok "Random.bool / Random::bool render as true/false in both spellings"
else
    bad "a namespace bool still renders as 1/0 in one spelling"
    printf '%s\n' "$out" | head -6 | sed 's/^/        /'
fi

# --- .to_string() on a bool-returning call ---------------------------------
# to_string() is the same _Generic table as print(); asserting it separately keeps
# the two from drifting, since a program can reach either one.
cat > "$TMP/ts.wyn" <<'EOF'
fn main() {
    var h = Json.parse("{\"ok\": true}")
    print("a ${"abc".is_alpha().to_string()}")
    print("b ${Json.is_valid(h).to_string()}")
    print("c ${4.is_even().to_string()}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run ts.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   [ "$(printf '%s' "$out" | grep -cE '^[abc] true$')" = "3" ]; then
    ok ".to_string() on a bool-returning call gives \"true\""
else
    bad ".to_string() on a bool-returning call is wrong"
    printf '%s\n' "$out" | head -5 | sed 's/^/        /'
fi

# --- truthiness, arithmetic and loops are UNTOUCHED ------------------------
# The cast changes the C type of the expression, not its value. This is the second
# control group: anything that consumes a bool NUMERICALLY must behave as before.
cat > "$TMP/truth.wyn" <<'EOF'
fn main() {
    var ns = [5, 3, 9]
    var h = Json.parse("{\"ok\": true}")
    var n = 0
    if "1.5".is_numeric() { n += 1 }
    if "abc".is_digit() { n += 10 }
    if 4.is_even() and ns.contains(3) { n += 100 }
    if Json.is_valid(h) or ns.is_empty() { n += 1000 }
    if not "1.5".is_int() { n += 10000 }
    print("n=${n}")
    var i = 0
    while "9".is_digit() and i < 3 { i += 1 }
    print("i=${i}")
    // a bool consumed as a number keeps its 0/1 value
    print("toint=${"abc".is_alpha().to_int()}")
    var flag = Json::is_valid(h)
    if flag { print("viavar") }
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run truth.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^n=11101$' &&
   printf '%s' "$out" | grep -q '^i=3$' &&
   printf '%s' "$out" | grep -q '^toint=1$' &&
   printf '%s' "$out" | grep -q '^viavar$'; then
    ok "conditions, while-loops, and/or/not and .to_int() behave exactly as before"
else
    bad "a numeric or control-flow use of a bool changed"
    printf '%s\n' "$out" | head -8 | sed 's/^/        /'
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "bool-in-print: $PASS pass, 0 fail"
  exit 0
fi
echo "bool-in-print: $PASS pass, $FAIL fail"
exit 1
