#!/bin/bash
# Json has ONE object model: the json_nodes[] arena, reached through a long long
# handle. Every Wyn-visible spelling -- `Json.parse`/`Json.new`/`Json.set_*`/
# `Json.get_*`/`Json.stringify`, the method forms on a json receiver, and the
# lowercase `json_*` aliases -- lowers to it.
#
# Before the unification there were TWO models and the checker let them mix:
#   * `Json.parse` returned a `long long` index into json_nodes[];
#   * `Json.new`/`Json.set_*`/`Json.stringify` took a `WynJson*` (json.c pairs[]).
# So the most obvious one-liner in the language, Json.stringify(Json.parse(s)),
# handed the integer 0 to a function that dereferenced it -> SIGSEGV; anything
# built with Json.new() could not be read back (get_string -> "", keys -> 0
# entries); and Json.parse could not report failure, because the handle is a
# counter: garbage gave 0 and so did the FIRST successful parse.
#
# Arms below are one property each. A crash arm asserts the EXIT STATUS, not just
# the output: a process killed by SIGSEGV prints nothing, which is
# indistinguishable from "this arm expected no output".
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# Run a program and require (a) a clean exit -- NOT a signal -- and (b) exact
# stdout. rc>=128 is a signal death (139 = SIGSEGV, 134 = SIGABRT); it is
# reported separately so a crash never reads as a mere output mismatch.
# stderr is kept OUT of the comparison (`wyn run` writes a "Compiled in NNNms"
# line there) but is quoted on failure.
expect_run() {
    local name="$1"; local src="$2"; local want="$3"
    local f="$TMP/$(echo "$name" | tr -cd 'a-zA-Z0-9').wyn"
    printf '%s' "$src" > "$f"
    local out rc
    out=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" run "$f" 2>"$TMP/err.txt"); rc=$?
    if [ $rc -ge 128 ]; then
        bad "$name (KILLED BY SIGNAL rc=$rc -- process crashed)"; return
    fi
    if [ $rc -ne 0 ]; then
        bad "$name (rc=$rc) [$(tail -3 "$TMP/err.txt" | tr '\n' '|')]"; return
    fi
    if [ "$out" = "$want" ]; then ok "$name"
    else bad "$name (output) want=[$(echo "$want" | tr '\n' '|')] got=[$(echo "$out" | tr '\n' '|')]"; fi
}

# ---------------------------------------------------------------------------
# 1. parse -> stringify round-trip. THE ticket's acceptance one-liner. This is
#    the arm that used to die of SIGSEGV, so it is deliberately the first.
# ---------------------------------------------------------------------------
expect_run "parse->get_int->stringify round-trip" \
'fn main() -> int {
    j = Json.parse("{\"a\":1}")
    print(Json.get_int(j, "a"))
    print(Json.stringify(j))
    return 0
}
' '1
{"a": 1}'

# ---------------------------------------------------------------------------
# 2. Build, then READ BACK. One model means a written key is a readable key.
# ---------------------------------------------------------------------------
# `Json.has` prints 1, not true: the namespace spelling is registered int-typed and
# existing programs compare it to 0. Asserted as it behaves, not as one would wish.
expect_run "build->read-back (get_string/get_int/keys/has)" \
'fn main() -> int {
    j = Json.new()
    Json.set_string(j, "name", "Alice")
    Json.set_int(j, "age", 30)
    print(Json.get_string(j, "name"))
    print(Json.get_int(j, "age"))
    print(len(Json.keys(j)))
    print(Json.has(j, "name"))
    print(Json.has(j, "nope"))
    print(Json.stringify(j))
    return 0
}
' 'Alice
30
2
1
0
{"name": "Alice", "age": 30}'

# ---------------------------------------------------------------------------
# 3. A value of EACH type survives a build -> stringify -> parse -> read cycle:
#    int, float, string, bool, null. set_bool must serialise `true`, not `1` -- the
#    pairs writer routed it through json_set_int, so the document lost the type.
#    The typed reads go through the text form of each value (Json.get) as well as
#    the typed getter, so "42" and "true" prove the NODE TYPE, not just the number.
# ---------------------------------------------------------------------------
expect_run "every scalar type round-trips (int/float/string/bool/null)" \
'fn main() -> int {
    j = Json.new()
    Json.set_int(j, "i", 42)
    Json.set_float(j, "f", 1.5)
    Json.set_string(j, "s", "hi")
    Json.set_bool(j, "b", true)
    Json.set_null(j, "n")
    text = Json.stringify(j)
    print(text)
    k = Json.parse(text)
    print(Json.get_int(k, "i"))
    print(Json.get_float(k, "f"))
    print(Json.get_string(k, "s"))
    print("bool-as-text=[${Json.get(k, "b")}]")
    print("null-as-text=[${Json.get(k, "n")}]")
    print(Json.stringify(k))
    return 0
}
' '{"i": 42, "f": 1.5, "s": "hi", "b": true, "n": null}
42
1.5
hi
bool-as-text=[true]
null-as-text=[]
{"i": 42, "f": 1.5, "s": "hi", "b": true, "n": null}'

# ---------------------------------------------------------------------------
# 4. Nested objects and arrays: get_object / get_array / array_len /
#    array_get / node_str, and stringify must re-emit the nesting.
# ---------------------------------------------------------------------------
expect_run "nested objects and arrays" \
'fn main() -> int {
    j = Json.parse("{\"user\":{\"name\":\"Bo\",\"age\":7},\"tags\":[\"x\",\"y\",\"z\"]}")
    u = Json.get_object(j, "user")
    print(Json.get_string(u, "name"))
    print(Json.get_int(u, "age"))
    a = Json.get_array(j, "tags")
    print(Json.array_len(a))
    print(Json.node_str(Json.array_get(a, 1)))
    print(Json.stringify(j))
    return 0
}
' 'Bo
7
3
y
{"user": {"name": "Bo", "age": 7}, "tags": ["x", "y", "z"]}'

# ---------------------------------------------------------------------------
# 5. Invalid JSON is DETECTABLE. Four shapes of garbage, none of which may look
#    like a document. Json.is_valid is the check; the handle is < 0.
# ---------------------------------------------------------------------------
expect_run "invalid JSON detected (garbage/truncated/empty/bad-literal)" \
'fn check(text: string) -> string {
    d = Json.parse(text)
    if Json.is_valid(d) {
        return "valid"
    }
    return "invalid"
}
fn main() -> int {
    print(check("not json at all"))
    print(check("{\"x\":"))
    print(check(""))
    print(check("   "))
    print(check("{\"a\":tru}"))
    print(check("{\"a\":1} trailing"))
    print(check("{\"a\":1,}"))
    print(check("{a:1}"))
    print(check("[1,2"))
    print(check("{\"unterminated\": \"abc"))
    return 0
}
' 'invalid
invalid
invalid
invalid
invalid
invalid
invalid
invalid
invalid
invalid'

# ---------------------------------------------------------------------------
# 6. A VALID parse is never mistaken for a failure -- including the very FIRST
#    parse a program performs, which used to return the same 0 as a failure.
#    The whole point: is_valid must be true even for the first handle.
# ---------------------------------------------------------------------------
expect_run "first valid parse is not a failure" \
'fn verdict(d: int) -> string {
    if Json.is_valid(d) {
        return "valid"
    }
    return "invalid"
}
fn main() -> int {
    first = Json.parse("{\"a\":1}")
    print(verdict(first))
    print(Json.get_int(first, "a"))
    junk = Json.parse("}{")
    print(verdict(junk))
    after = Json.parse("{\"b\":2}")
    print(verdict(after))
    print(Json.get_int(after, "b"))
    print(Json.get_int(first, "a"))
    // Every shape of valid JSON document, including the ones whose first byte
    // is not "{": an over-eager validity check would reject these.
    print(verdict(Json.parse("{}")))
    print(verdict(Json.parse("[]")))
    print(verdict(Json.parse("[1, 2, 3]")))
    print(verdict(Json.parse("  {\"a\" : 1}  ")))
    print(verdict(Json.parse("{\"a\":null,\"b\":false,\"c\":-1.5e3}")))
    print(verdict(Json.parse("{\"a\":{\"b\":[{\"c\":1}]}}")))
    return 0
}
' 'valid
1
invalid
valid
2
1
valid
valid
valid
valid
valid
valid'

# ---------------------------------------------------------------------------
# 7. Unicode and escapes survive a round-trip in BOTH directions: a value the
#    writer has to escape, read back verbatim, and a \uXXXX the reader decodes.
# ---------------------------------------------------------------------------
expect_run "unicode and escapes round-trip" \
'fn main() -> int {
    j = Json.new()
    Json.set_string(j, "q", "a\"b")
    Json.set_string(j, "nl", "x\ny")
    text = Json.stringify(j)
    print(text)
    k = Json.parse(text)
    print(Json.get_string(k, "q"))
    u = Json.parse("{\"e\":\"caf\\u00e9 \\u2713\"}")
    print(Json.get_string(u, "e"))
    print(Json.stringify(u))
    return 0
}
' '{"q": "a\"b", "nl": "x\ny"}
a"b
café ✓
{"e": "café ✓"}'

# ---------------------------------------------------------------------------
# 8. The METHOD form on a parsed handle. `doc.get_string(k)` used to lower to
#    json_get_string(WynJson*) and dereference the integer handle. Same for
#    "...".parse_json(). No crash, and the right answer.
# ---------------------------------------------------------------------------
expect_run "method form on a parsed handle" \
'fn main() -> int {
    doc = Json.parse("{\"name\":\"Wyn\",\"n\":3}")
    print(doc.get_string("name"))
    print(doc.get_int("n"))
    p = "{\"k\":\"v\"}".parse_json()
    print(p.get_string("k"))
    print(p.stringify())
    return 0
}
' 'Wyn
3
v
{"k": "v"}'

# ---------------------------------------------------------------------------
# 9. The lowercase aliases are the SAME model, not a second one: a document
#    parsed with json_parse reads through Json.get_* and vice versa. This is
#    the mix that used to segfault.
# ---------------------------------------------------------------------------
expect_run "lowercase aliases share the one model" \
'fn main() -> int {
    d = json_parse("{\"name\":\"Ada\",\"age\":36}")
    print(json_get_string(d, "name"))
    print(json_get_int(d, "age"))
    print(Json.get_string(d, "name"))
    print(json_stringify(d))
    b = json_new()
    json_set_string(b, "k", "v")
    print(Json.get_string(b, "k"))
    print(json_stringify(b))
    json_free(d)
    return 0
}
' 'Ada
36
Ada
{"name": "Ada", "age": 36}
v
{"k": "v"}'

# ---------------------------------------------------------------------------
# 10. Many live documents at once, each independently readable, and a parse
#     failure in the middle must not disturb the ones already parsed.
# ---------------------------------------------------------------------------
expect_run "independent live documents survive a failed parse" \
'fn main() -> int {
    a = Json.parse("{\"v\":1}")
    junk = Json.parse("[[[[")
    b = Json.parse("{\"v\":2}")
    print(Json.get_int(a, "v"))
    if Json.is_valid(junk) {
        print("junk WAS ACCEPTED")
    } else {
        print("junk rejected")
    }
    print(Json.get_int(b, "v"))
    total = 0
    for i in 0..50 {
        d = Json.parse("{\"n\":7}")
        total = total + Json.get_int(d, "n")
    }
    print(total)
    return 0
}
' '1
junk rejected
2
350'

# ---------------------------------------------------------------------------
# 11. A literal truncated at the very end of the buffer. `true`/`false`/`null`
#     used to be recognised from their FIRST BYTE and then advanced blindly
#     (`*p == 't'` ... `p += 4`), so "tru" walked the pointer past the NUL
#     terminator. Behaviourally these are just invalid documents, which is what
#     this arm pins; the out-of-bounds READ is only provable under the ASan
#     runtime job (same honest limit as tests/regression/test_json_multiple_docs.wyn
#     records for its miss-handle arm). Mutating the strict strncmp out does NOT
#     redden this arm - stated plainly rather than left looking verified.
# ---------------------------------------------------------------------------
expect_run "truncated literals are invalid and do not crash" \
'fn check(text: string) -> string {
    d = Json.parse(text)
    if Json.is_valid(d) {
        return "valid"
    }
    return "invalid"
}
fn main() -> int {
    print(check("tru"))
    print(check("fals"))
    print(check("nul"))
    print(check("n"))
    print(check("{\"a\":tru"))
    return 0
}
' 'invalid
invalid
invalid
invalid
invalid'

echo ""; echo "json-model: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
