/* test_schema_of.c - gate for src/wyn_schema.c.
 *
 * There is no `ai fn` syntax yet, so there is no .wyn program that can reach
 * schema derivation. The gate is therefore a C unit test, in the shape of
 * tests/tls/test_tls_seam.c: link the module under test directly, drive it, and
 * assert on exact bytes.
 *
 * Exact bytes, not "contains" or "parses": the schema is destined to be frozen
 * into generated C and to key record/replay cassettes later, so its identity IS
 * its byte string. A test that only checked it was valid JSON would pass while
 * field order drifted between builds - the one failure mode that breaks
 * reproducible output.
 *
 * Types are built by hand here rather than by running the checker. That is
 * deliberate: it keeps the gate to seconds, it lets the rejection arms construct
 * types the parser cannot yet spell (a recursive struct), and it proves the
 * module depends on nothing but src/types.h.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wyn_schema.h"

static int failures = 0;
static int checks   = 0;

static void ok(const char* name)
{
    printf("  PASS: %s\n", name);
    checks++;
}

static void bad(const char* name, const char* detail)
{
    printf("  FAIL: %s\n        %s\n", name, detail ? detail : "");
    failures++;
    checks++;
}

/* ------------------------------------------------------------- type builders */

static Token tk(const char* s)
{
    Token t;
    t.type   = TOKEN_IDENT;
    t.start  = s;
    t.length = (int)strlen(s);
    t.line   = 0;
    return t;
}

static Type* mk(TypeKind k)
{
    Type* t = calloc(1, sizeof(Type));
    if (!t) {
        fprintf(stderr, "out of memory building a test type\n");
        exit(2);
    }
    t->kind = k;
    return t;
}

static Type* t_int(void)    { return mk(TYPE_INT); }
static Type* t_float(void)  { return mk(TYPE_FLOAT); }
static Type* t_string(void) { return mk(TYPE_STRING); }
static Type* t_bool(void)   { return mk(TYPE_BOOL); }

static Type* t_arr(Type* elem)
{
    Type* t = mk(TYPE_ARRAY);
    t->array_type.element_type = elem;
    return t;
}

static Type* t_opt(Type* inner)
{
    Type* t = mk(TYPE_OPTIONAL);
    t->optional_type.inner_type = inner;
    return t;
}

/* t_struct("Invoice", "vendor", t_string(), "total_cents", t_int(), NULL)
 * Field order is the argument order, which is the point of arm 6. */
static Type* t_struct(const char* name, ...)
{
    Type* t = mk(TYPE_STRUCT);
    t->struct_type.name = tk(name);
    t->name             = tk(name);

    Token names[32];
    Type* types[32];
    int   n = 0;

    va_list ap;
    va_start(ap, name);
    for (;;) {
        const char* fname = va_arg(ap, const char*);
        if (!fname) break;
        Type* ftype = va_arg(ap, Type*);
        if (n < 32) {
            names[n] = tk(fname);
            types[n] = ftype;
            n++;
        }
    }
    va_end(ap);

    t->struct_type.field_count = n;
    if (n > 0) {
        t->struct_type.field_names = malloc(sizeof(Token) * (size_t)n);
        t->struct_type.field_types = malloc(sizeof(Type*) * (size_t)n);
        if (!t->struct_type.field_names || !t->struct_type.field_types) exit(2);
        for (int i = 0; i < n; i++) {
            t->struct_type.field_names[i] = names[i];
            t->struct_type.field_types[i] = types[i];
        }
    }
    return t;
}

/* t_enum("Currency", "USD", "EUR", "GBP", NULL) - payload-free unless a payload
 * row is registered for it below. */
static Type* t_enum(const char* name, ...)
{
    Type* t = mk(TYPE_ENUM);
    t->name = tk(name);

    Token variants[32];
    int   n = 0;

    va_list ap;
    va_start(ap, name);
    for (;;) {
        const char* v = va_arg(ap, const char*);
        if (!v) break;
        if (n < 32) variants[n++] = tk(v);
    }
    va_end(ap);

    t->enum_type.variant_count = n;
    if (n > 0) {
        t->enum_type.variants = malloc(sizeof(Token) * (size_t)n);
        if (!t->enum_type.variants) exit(2);
        for (int i = 0; i < n; i++) t->enum_type.variants[i] = variants[i];
    }
    return t;
}

/* -------------------------------------------------- the host's payload lookup
 *
 * Stands in for the checker's find_enum_definition(): a tiny table keyed by
 * (enum name, variant index). Unregistered variants are payload-free, which is
 * what a plain enum looks like to the walker.
 */

typedef struct {
    const char* enum_name;
    int         variant_index;
    const Type* types[4];
    int         count;
} PayloadRow;

static PayloadRow g_payloads[32];
static int        g_payload_count;
/* Enums the host knows about at all. An enum absent from this list models "the
 * checker has no declaration for it", which must be an error, not a guess. */
static const char* g_known_enums[16];
static int         g_known_enum_count;

static void enum_declare(const char* name)
{
    if (g_known_enum_count < 16) g_known_enums[g_known_enum_count++] = name;
}

static void payload_add(const char* enum_name, int variant_index, ...)
{
    enum_declare(enum_name);
    if (g_payload_count >= 32) exit(2);
    PayloadRow* r     = &g_payloads[g_payload_count++];
    r->enum_name      = enum_name;
    r->variant_index  = variant_index;
    r->count          = 0;

    va_list ap;
    va_start(ap, variant_index);
    for (;;) {
        Type* ty = va_arg(ap, Type*);
        if (!ty) break;
        if (r->count < 4) r->types[r->count++] = ty;
    }
    va_end(ap);
}

static int host_enum_payload(void* ctx, const char* enum_name, int variant_index,
                             WynSchemaPayload* out)
{
    (void)ctx;
    int known = 0;
    for (int i = 0; i < g_known_enum_count; i++)
        if (strcmp(g_known_enums[i], enum_name) == 0) known = 1;
    if (!known) return -1;

    for (int i = 0; i < g_payload_count; i++) {
        if (g_payloads[i].variant_index == variant_index &&
            strcmp(g_payloads[i].enum_name, enum_name) == 0) {
            out->types = g_payloads[i].types;
            out->count = g_payloads[i].count;
            return 0;
        }
    }
    out->types = NULL;
    out->count = 0;
    return 0;
}

static WynSchemaEnv g_env = { host_enum_payload, NULL };

/* ------------------------------------------------------------------ asserters */

static void expect_schema(const char* name, const Type* t, const char* want)
{
    char out[8192];
    char err[512];
    out[0] = '\0';
    err[0] = '\0';
    int n = wyn_schema_of(t, &g_env, out, sizeof(out), err, sizeof(err));
    if (n < 0) {
        char detail[1024];
        snprintf(detail, sizeof(detail), "rejected, but should have derived: %s", err);
        bad(name, detail);
        return;
    }
    if (n != (int)strlen(out)) {
        char detail[256];
        snprintf(detail, sizeof(detail), "returned %d but wrote %zu bytes", n, strlen(out));
        bad(name, detail);
        return;
    }
    if (strcmp(out, want) != 0) {
        char detail[16384];
        snprintf(detail, sizeof(detail), "want: %s\n        got:  %s", want, out);
        bad(name, detail);
        return;
    }
    ok(name);
}

/* A rejection must (a) reject, and (b) say which type and what to do. `needle`
 * pins the type name; `advice` pins that the message is actionable rather than
 * just "unsupported"; `also` pins the REASON, so that an arm cannot be satisfied
 * by some other rejection happening to fire first. That last one is not
 * belt-and-braces: with only (a) and (b), deleting the cycle check left the
 * recursion arms GREEN, because the walk then ran into the depth guard, whose
 * message also names the type and also says "Flatten". */
static void expect_reject(const char* name, const Type* t, const char* needle,
                          const char* advice, const char* also)
{
    char out[8192];
    char err[1024];
    out[0] = '\0';
    err[0] = '\0';
    int n = wyn_schema_of(t, &g_env, out, sizeof(out), err, sizeof(err));
    if (n >= 0) {
        char detail[16384];
        snprintf(detail, sizeof(detail), "accepted, but must be rejected. got: %s", out);
        bad(name, detail);
        return;
    }
    if (err[0] == '\0') {
        bad(name, "rejected with an empty message");
        return;
    }
    if (!strstr(err, needle)) {
        char detail[1200];
        snprintf(detail, sizeof(detail), "message never names '%s': %s", needle, err);
        bad(name, detail);
        return;
    }
    if (advice && !strstr(err, advice)) {
        char detail[1200];
        snprintf(detail, sizeof(detail), "message gives no way out ('%s' absent): %s",
                 advice, err);
        bad(name, detail);
        return;
    }
    if (also && !strstr(err, also)) {
        char detail[1200];
        snprintf(detail, sizeof(detail), "rejected for the wrong reason ('%s' absent): %s",
                 also, err);
        bad(name, detail);
        return;
    }
    /* A rejection must not also half-write a schema: codegen reads `out` only
     * when the return is >= 0, but a partial write there is a loaded gun. */
    if (out[0] != '\0') {
        bad(name, "rejected but still wrote into the output buffer");
        return;
    }
    printf("  PASS: %s\n        -> %s\n", name, err);
    checks++;
}

/* ----------------------------------------------------------------------- arms */

/* Arm 1: the primitives and a flat struct over all four of them. */
static void arm1_flat(void)
{
    puts("Arm 1: primitives + a flat struct");

    expect_schema("int", t_int(), "{\"type\":\"integer\"}");
    expect_schema("float", t_float(), "{\"type\":\"number\"}");
    expect_schema("string", t_string(), "{\"type\":\"string\"}");
    expect_schema("bool", t_bool(), "{\"type\":\"boolean\"}");

    Type* invoice = t_struct("Invoice",
                             "vendor", t_string(),
                             "total_cents", t_int(),
                             "tax_rate", t_float(),
                             "paid", t_bool(),
                             NULL);
    expect_schema("struct of int/float/string/bool", invoice,
                  "{\"type\":\"object\",\"properties\":{"
                  "\"vendor\":{\"type\":\"string\"},"
                  "\"total_cents\":{\"type\":\"integer\"},"
                  "\"tax_rate\":{\"type\":\"number\"},"
                  "\"paid\":{\"type\":\"boolean\"}},"
                  "\"required\":[\"vendor\",\"total_cents\",\"tax_rate\",\"paid\"],"
                  "\"additionalProperties\":false}");
}

/* Arm 2: nesting - a struct in a struct, [int], and [Struct]. */
static void arm2_nesting(void)
{
    puts("Arm 2: nested struct, [int], [struct]");

    Type* addr = t_struct("Address",
                          "city", t_string(),
                          "zip", t_string(),
                          NULL);

    expect_schema("nested struct", t_struct("Customer",
                                            "name", t_string(),
                                            "address", addr,
                                            NULL),
                  "{\"type\":\"object\",\"properties\":{"
                  "\"name\":{\"type\":\"string\"},"
                  "\"address\":{\"type\":\"object\",\"properties\":{"
                  "\"city\":{\"type\":\"string\"},"
                  "\"zip\":{\"type\":\"string\"}},"
                  "\"required\":[\"city\",\"zip\"],"
                  "\"additionalProperties\":false}},"
                  "\"required\":[\"name\",\"address\"],"
                  "\"additionalProperties\":false}");

    expect_schema("[int] at the root", t_arr(t_int()),
                  "{\"type\":\"array\",\"items\":{\"type\":\"integer\"}}");

    expect_schema("[[int]]", t_arr(t_arr(t_int())),
                  "{\"type\":\"array\",\"items\":"
                  "{\"type\":\"array\",\"items\":{\"type\":\"integer\"}}}");

    Type* item = t_struct("LineItem",
                          "description", t_string(),
                          "amount_cents", t_int(),
                          NULL);
    expect_schema("struct with [int] and [struct]",
                  t_struct("Order",
                           "quantities", t_arr(t_int()),
                           "items", t_arr(item),
                           NULL),
                  "{\"type\":\"object\",\"properties\":{"
                  "\"quantities\":{\"type\":\"array\",\"items\":{\"type\":\"integer\"}},"
                  "\"items\":{\"type\":\"array\",\"items\":"
                  "{\"type\":\"object\",\"properties\":{"
                  "\"description\":{\"type\":\"string\"},"
                  "\"amount_cents\":{\"type\":\"integer\"}},"
                  "\"required\":[\"description\",\"amount_cents\"],"
                  "\"additionalProperties\":false}}},"
                  "\"required\":[\"quantities\",\"items\"],"
                  "\"additionalProperties\":false}");

    /* The SAME struct twice in one parent is not recursion. Push/pop, not a
     * visited set - otherwise arm 7's cycle check would reject this. */
    expect_schema("same struct twice is not a cycle",
                  t_struct("Trip",
                           "from", addr,
                           "to", addr,
                           NULL),
                  "{\"type\":\"object\",\"properties\":{"
                  "\"from\":{\"type\":\"object\",\"properties\":{"
                  "\"city\":{\"type\":\"string\"},\"zip\":{\"type\":\"string\"}},"
                  "\"required\":[\"city\",\"zip\"],\"additionalProperties\":false},"
                  "\"to\":{\"type\":\"object\",\"properties\":{"
                  "\"city\":{\"type\":\"string\"},\"zip\":{\"type\":\"string\"}},"
                  "\"required\":[\"city\",\"zip\"],\"additionalProperties\":false}},"
                  "\"required\":[\"from\",\"to\"],"
                  "\"additionalProperties\":false}");
}

/* Arm 3: Option<T> is IN properties and OUT of required. */
static void arm3_option(void)
{
    puts("Arm 3: Option<T> - present in properties, absent from required");

    expect_schema("one optional field among required ones",
                  t_struct("Profile",
                           "id", t_int(),
                           "nickname", t_opt(t_string()),
                           "age", t_int(),
                           NULL),
                  "{\"type\":\"object\",\"properties\":{"
                  "\"id\":{\"type\":\"integer\"},"
                  "\"nickname\":{\"type\":\"string\"},"
                  "\"age\":{\"type\":\"integer\"}},"
                  "\"required\":[\"id\",\"age\"],"
                  "\"additionalProperties\":false}");

    /* Every field optional -> an EMPTY required array, not a missing key: the
     * provider's strict mode requires the key to be present. */
    expect_schema("all fields optional -> required:[]",
                  t_struct("AllOpt",
                           "a", t_opt(t_int()),
                           "b", t_opt(t_string()),
                           NULL),
                  "{\"type\":\"object\",\"properties\":{"
                  "\"a\":{\"type\":\"integer\"},"
                  "\"b\":{\"type\":\"string\"}},"
                  "\"required\":[],"
                  "\"additionalProperties\":false}");

    /* Option of a compound: the inner schema is emitted whole, the field still
     * drops out of required. */
    expect_schema("Option<[int]> and Option<struct>",
                  t_struct("Maybe",
                           "tags", t_opt(t_arr(t_string())),
                           "addr", t_opt(t_struct("Address", "city", t_string(), NULL)),
                           "n", t_int(),
                           NULL),
                  "{\"type\":\"object\",\"properties\":{"
                  "\"tags\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}},"
                  "\"addr\":{\"type\":\"object\",\"properties\":{"
                  "\"city\":{\"type\":\"string\"}},"
                  "\"required\":[\"city\"],\"additionalProperties\":false},"
                  "\"n\":{\"type\":\"integer\"}},"
                  "\"required\":[\"n\"],"
                  "\"additionalProperties\":false}");

    /* [Option<T>] has no "required" to drop out of, so the option is
     * meaningless there and the element schema is just T's. Pinned so the
     * behaviour is a decision, not an accident. */
    expect_schema("[Option<int>] degrades to [int]", t_arr(t_opt(t_int())),
                  "{\"type\":\"array\",\"items\":{\"type\":\"integer\"}}");
}

/* Arm 4: a payload-free enum, in declaration order. */
static void arm4_plain_enum(void)
{
    puts("Arm 4: payload-free enum");

    enum_declare("Currency");
    /* USD, EUR, GBP is neither alphabetical nor reverse-alphabetical, so the
     * expected string can only match if declaration order is preserved. */
    expect_schema("plain enum", t_enum("Currency", "USD", "EUR", "GBP", NULL),
                  "{\"enum\":[\"USD\",\"EUR\",\"GBP\"]}");

    enum_declare("Status");
    expect_schema("plain enum as a struct field",
                  t_struct("Ticket",
                           "title", t_string(),
                           "status", t_enum("Status", "Open", "Closed", NULL),
                           NULL),
                  "{\"type\":\"object\",\"properties\":{"
                  "\"title\":{\"type\":\"string\"},"
                  "\"status\":{\"enum\":[\"Open\",\"Closed\"]}},"
                  "\"required\":[\"title\",\"status\"],"
                  "\"additionalProperties\":false}");
}

/* Arm 5: an enum WITH payloads -> anyOf + a const discriminant. */
static void arm5_data_enum(void)
{
    puts("Arm 5: enum with payloads -> anyOf + discriminant");

    /* enum Shape { Circle(float), Rect(float, float), Origin } - one single
     * payload, one multi payload, one payload-free, mixed in one enum. */
    payload_add("Shape", 0, t_float(), NULL);
    payload_add("Shape", 1, t_float(), t_float(), NULL);
    expect_schema("data enum: 1-payload, 2-payload and bare variants",
                  t_enum("Shape", "Circle", "Rect", "Origin", NULL),
                  "{\"anyOf\":["
                  "{\"type\":\"object\",\"properties\":{"
                  "\"type\":{\"type\":\"string\",\"const\":\"Circle\"},"
                  "\"value\":{\"type\":\"number\"}},"
                  "\"required\":[\"type\",\"value\"],"
                  "\"additionalProperties\":false},"
                  "{\"type\":\"object\",\"properties\":{"
                  "\"type\":{\"type\":\"string\",\"const\":\"Rect\"},"
                  "\"value0\":{\"type\":\"number\"},"
                  "\"value1\":{\"type\":\"number\"}},"
                  "\"required\":[\"type\",\"value0\",\"value1\"],"
                  "\"additionalProperties\":false},"
                  "{\"type\":\"object\",\"properties\":{"
                  "\"type\":{\"type\":\"string\",\"const\":\"Origin\"}},"
                  "\"required\":[\"type\"],"
                  "\"additionalProperties\":false}]}");

    /* A struct payload nests, and an Option payload is still REQUIRED: the
     * discriminated object has no meaningful "absent" for a positional slot. */
    payload_add("Event", 0, t_struct("Address", "city", t_string(), NULL), NULL);
    payload_add("Event", 1, t_opt(t_int()), NULL);
    expect_schema("data enum with struct and Option payloads",
                  t_enum("Event", "Moved", "Aged", NULL),
                  "{\"anyOf\":["
                  "{\"type\":\"object\",\"properties\":{"
                  "\"type\":{\"type\":\"string\",\"const\":\"Moved\"},"
                  "\"value\":{\"type\":\"object\",\"properties\":{"
                  "\"city\":{\"type\":\"string\"}},"
                  "\"required\":[\"city\"],\"additionalProperties\":false}},"
                  "\"required\":[\"type\",\"value\"],"
                  "\"additionalProperties\":false},"
                  "{\"type\":\"object\",\"properties\":{"
                  "\"type\":{\"type\":\"string\",\"const\":\"Aged\"},"
                  "\"value\":{\"type\":\"integer\"}},"
                  "\"required\":[\"type\",\"value\"],"
                  "\"additionalProperties\":false}]}");
}

/* Arm 6: determinism - byte-identical across calls, and DECLARATION order. */
static void arm6_determinism(void)
{
    puts("Arm 6: determinism + declaration order");

    /* zebra, apple, mango is deliberately neither alphabetical nor reverse: a
     * sort in either direction reorders it, so the expected string catches
     * both. And the nested struct's fields (yak, ant) are ordered the same way,
     * so sorting only the inner object is caught too. */
    Type* t = t_struct("Ordered",
                       "zebra", t_int(),
                       "apple", t_string(),
                       "mango", t_struct("Inner",
                                         "yak", t_bool(),
                                         "ant", t_float(),
                                         NULL),
                       NULL);
    const char* want =
        "{\"type\":\"object\",\"properties\":{"
        "\"zebra\":{\"type\":\"integer\"},"
        "\"apple\":{\"type\":\"string\"},"
        "\"mango\":{\"type\":\"object\",\"properties\":{"
        "\"yak\":{\"type\":\"boolean\"},"
        "\"ant\":{\"type\":\"number\"}},"
        "\"required\":[\"yak\",\"ant\"],"
        "\"additionalProperties\":false}},"
        "\"required\":[\"zebra\",\"apple\",\"mango\"],"
        "\"additionalProperties\":false}";

    expect_schema("declaration order, not alphabetical", t, want);

    /* Same type, derived again: byte-identical. This is the cassette-keying
     * property - two builds of one program must agree to the byte. */
    char a[8192], b[8192], err[512];
    int na = wyn_schema_of(t, &g_env, a, sizeof(a), err, sizeof(err));
    int nb = wyn_schema_of(t, &g_env, b, sizeof(b), err, sizeof(err));
    if (na < 0 || nb < 0) {
        bad("deriving twice succeeds twice", err);
    } else if (na != nb || memcmp(a, b, (size_t)na) != 0) {
        bad("two derivations are byte-identical", "the second run differed");
    } else {
        ok("two derivations are byte-identical");
    }

    /* Enum variant order is declaration order too. */
    enum_declare("Weekday");
    expect_schema("enum variants in declaration order",
                  t_enum("Weekday", "Wed", "Mon", "Tue", NULL),
                  "{\"enum\":[\"Wed\",\"Mon\",\"Tue\"]}");

    /* ...and so is anyOf variant order. */
    payload_add("Reading", 0, t_float(), NULL);
    payload_add("Reading", 2, t_string(), NULL);
    expect_schema("anyOf branches in declaration order",
                  t_enum("Reading", "Zap", "Amp", "Mid", NULL),
                  "{\"anyOf\":["
                  "{\"type\":\"object\",\"properties\":{"
                  "\"type\":{\"type\":\"string\",\"const\":\"Zap\"},\"value\":{\"type\":\"number\"}},"
                  "\"required\":[\"type\",\"value\"],\"additionalProperties\":false},"
                  "{\"type\":\"object\",\"properties\":{\"type\":{\"type\":\"string\",\"const\":\"Amp\"}},"
                  "\"required\":[\"type\"],\"additionalProperties\":false},"
                  "{\"type\":\"object\",\"properties\":{"
                  "\"type\":{\"type\":\"string\",\"const\":\"Mid\"},\"value\":{\"type\":\"string\"}},"
                  "\"required\":[\"type\",\"value\"],\"additionalProperties\":false}]}");
}

/* Arm 7: the rejections. The provider subset has no $ref and no
 * additionalProperties, so these types have no schema at all - and learning
 * that at `make check` instead of from a 400 is the point of the feature. */
static void arm7_rejections(void)
{
    puts("Arm 7: rejections");

    /* --- recursion, three shapes ----------------------------------------- */

    /* struct Tree { label: string, parent: Tree } */
    Type* direct = t_struct("Tree", "label", t_string(), NULL);
    direct->struct_type.field_count = 2;
    direct->struct_type.field_names =
        realloc(direct->struct_type.field_names, sizeof(Token) * 2);
    direct->struct_type.field_types =
        realloc(direct->struct_type.field_types, sizeof(Type*) * 2);
    direct->struct_type.field_names[1] = tk("parent");
    direct->struct_type.field_types[1] = direct;
    expect_reject("recursive struct, directly", direct, "Tree", "Flatten",
                  "cycle: Tree -> Tree");

    /* struct Node { name: string, children: [Node] } - the shape people
     * actually write, and the one a depth-1 check would miss. */
    Type* viaarr = t_struct("Node", "name", t_string(), NULL);
    viaarr->struct_type.field_count = 2;
    viaarr->struct_type.field_names =
        realloc(viaarr->struct_type.field_names, sizeof(Token) * 2);
    viaarr->struct_type.field_types =
        realloc(viaarr->struct_type.field_types, sizeof(Type*) * 2);
    viaarr->struct_type.field_names[1] = tk("children");
    viaarr->struct_type.field_types[1] = t_arr(viaarr);
    expect_reject("recursive struct through [T]", viaarr, "Node", "Flatten",
                  "cycle: Node -> Node");

    /* struct A { b: B }  struct B { a: Option<A> } - mutual, through an
     * Option, so neither name alone looks recursive. */
    Type* a = t_struct("Alpha", "b", NULL, NULL);
    Type* b = t_struct("Beta", "a", t_opt(a), NULL);
    a->struct_type.field_types[0] = b;
    expect_reject("mutually recursive structs", a, "Alpha", "Flatten",
                  "cycle: Alpha -> Beta -> Alpha");

    /* enum Expr { Lit(int), Neg(Expr) } - recursion through an enum payload. */
    Type* rec_enum = t_enum("Expr", "Lit", "Neg", NULL);
    payload_add("Expr", 0, t_int(), NULL);
    payload_add("Expr", 1, rec_enum, NULL);
    expect_reject("recursive enum payload", rec_enum, "Expr", "Flatten",
                  "cycle: Expr -> Expr");

    /* --- types with no JSON Schema in the strict subset ------------------- */

    Type* map = calloc(1, sizeof(Type));
    if (!map) exit(2);
    map->kind                = TYPE_MAP;
    map->map_type.key_type   = t_string();
    map->map_type.value_type = t_int();
    expect_reject("map field", t_struct("Doc", "tags", map, NULL), "Doc.tags",
                  "Pair", "additionalProperties");

    Type* fn = calloc(1, sizeof(Type));
    if (!fn) exit(2);
    fn->kind                 = TYPE_FUNCTION;
    fn->fn_type.return_type  = t_int();
    expect_reject("function field", t_struct("Handler", "cb", fn, NULL),
                  "Handler.cb", "Return", "function type");

    Type* set = mk(TYPE_SET);
    expect_reject("set field", t_struct("Bag", "seen", set, NULL), "Bag.seen",
                  "de-duplicate", "set type");

    Type* json = mk(TYPE_JSON);
    expect_reject("untyped json field", t_struct("Blob", "raw", json, NULL),
                  "Blob.raw", "struct", "untyped json");

    Type* res = mk(TYPE_RESULT);
    res->result_type.ok_type  = t_int();
    res->result_type.err_type = t_string();
    expect_reject("nested Result field", t_struct("Wrapped", "inner", res, NULL),
                  "Wrapped.inner", "AiError", "nested Result");

    Type* gen = mk(TYPE_GENERIC);
    gen->name = tk("T");
    expect_reject("unresolved generic", gen, "T", "concrete",
                  "generic type parameter");

    Type* ch = mk(TYPE_CHANNEL);
    expect_reject("channel field", t_struct("Pipe", "ch", ch, NULL), "Pipe.ch",
                  "send", "channel");

    expect_reject("void return", mk(TYPE_VOID), "void", "Return", "nothing for the model");

    /* Option<Option<T>>: JSON has one null, so the two levels collapse. */
    expect_reject("double Option", t_struct("Twice", "v", t_opt(t_opt(t_int())), NULL),
                  "Twice.v", "single Option", "Option<Option<T>>");

    /* An opaque or unresolved annotation - where `ptr` and an unknown type name
     * land - reaches the walker as a fieldless struct or as no type at all. */
    Type* opaque = t_struct("Handle", NULL);
    expect_reject("fieldless/opaque struct", t_struct("Holder", "h", opaque, NULL),
                  "Handle", "ptr", "no fields");

    Type* nulled = t_struct("Broken", "p", NULL, NULL);
    expect_reject("field whose type the checker left NULL", nulled, "Broken.p",
                  "ptr", "resolved no type");

    /* Nesting bound. Not recursion - 40 DISTINCT structs, one inside the next -
     * so the cycle check cannot catch it and only the depth guard can. Without
     * an arm here the guard was mutation-survivable, i.e. deletable: the walk
     * would then recurse 40 deep on the C stack for this type and arbitrarily
     * deep for a generated one. */
    {
        char  names[48][16];
        Type* deep = t_int();
        for (int i = 0; i < 40; i++) {
            snprintf(names[i], sizeof(names[i]), "Lvl%d", i);
            deep = t_struct(names[i], "next", deep, NULL);
        }
        expect_reject("nesting deeper than the bound", deep, "Lvl", "Flatten",
                      "nesting exceeds");
    }

    /* An enum the host has never heard of must not be guessed at. */
    expect_reject("enum unknown to the host", t_enum("Ghost", "A", "B", NULL),
                  "Ghost", "declar", "would have to guess");

    /* ...and neither may an enum be derived with no resolver installed. */
    {
        char out[512], err[512];
        out[0] = '\0';
        err[0] = '\0';
        Type* e = t_enum("Currency", "USD", "EUR", NULL);
        if (wyn_schema_of(e, NULL, out, sizeof(out), err, sizeof(err)) >= 0)
            bad("enum with no resolver is refused", "it was accepted");
        else if (!strstr(err, "Currency"))
            bad("enum with no resolver is refused", err);
        else
            ok("enum with no resolver is refused");
    }
}

/* Arm 7b: names are JSON-escaped.
 *
 * No Wyn identifier can contain a quote, so nothing the parser accepts reaches
 * this. It is still a real property and still worth an arm: the emitter copies
 * bytes out of source text through a (start, length) pair, and "we validated it
 * upstream" is the premise every key-injection defect is built on. One arm here
 * means the escaping cannot be deleted as dead weight. */
static void arm7b_escaping(void)
{
    puts("Arm 7b: names are JSON-escaped");

    Type* t = t_struct("Weird",
                       "a\"b", t_int(),
                       "c\\d", t_string(),
                       "e\nf", t_bool(),
                       NULL);
    expect_schema("a field name that needs escaping", t,
                  "{\"type\":\"object\",\"properties\":{"
                  "\"a\\\"b\":{\"type\":\"integer\"},"
                  "\"c\\\\d\":{\"type\":\"string\"},"
                  "\"e\\nf\":{\"type\":\"boolean\"}},"
                  "\"required\":[\"a\\\"b\",\"c\\\\d\",\"e\\nf\"],"
                  "\"additionalProperties\":false}");

    enum_declare("Odd");
    expect_schema("an enum variant that needs escaping",
                  t_enum("Odd", "x\"y", "z", NULL),
                  "{\"enum\":[\"x\\\"y\",\"z\"]}");
}

/* Arm 8: the buffer contract. A truncated schema must never be reported as a
 * success - it would be frozen into generated C as invalid JSON. */
static void arm8_buffer(void)
{
    puts("Arm 8: no silent truncation");

    Type* t = t_struct("Invoice",
                       "vendor", t_string(),
                       "total_cents", t_int(),
                       NULL);

    char tiny[16];
    char err[512];
    memset(tiny, 'X', sizeof(tiny));
    err[0] = '\0';
    if (wyn_schema_of(t, &g_env, tiny, sizeof(tiny), err, sizeof(err)) >= 0) {
        bad("a schema that does not fit is an error", "short write reported as success");
    } else if (!strstr(err, "16")) {
        char detail[600];
        snprintf(detail, sizeof(detail), "message does not state the buffer size: %s", err);
        bad("a schema that does not fit is an error", detail);
    } else {
        ok("a schema that does not fit is an error");
    }

    /* The alloc entry point is the same derivation without the size guess -
     * codegen's shape, since it cannot know the length in advance. */
    size_t len  = 0;
    char*  heap = wyn_schema_of_alloc(t, &g_env, &len, err, sizeof(err));
    if (!heap) {
        bad("wyn_schema_of_alloc derives the same bytes", err);
    } else {
        char buf[8192];
        int  n = wyn_schema_of(t, &g_env, buf, sizeof(buf), err, sizeof(err));
        if (n < 0 || len != (size_t)n || strcmp(heap, buf) != 0)
            bad("wyn_schema_of_alloc derives the same bytes", heap);
        else
            ok("wyn_schema_of_alloc derives the same bytes");
        free(heap);
    }

    /* A rejection through the alloc path returns NULL, not a partial string. */
    Type* v = mk(TYPE_VOID);
    err[0]  = '\0';
    char* nope = wyn_schema_of_alloc(v, &g_env, &len, err, sizeof(err));
    if (nope) {
        bad("wyn_schema_of_alloc refuses what wyn_schema_of refuses", nope);
        free(nope);
    } else if (err[0] == '\0') {
        bad("wyn_schema_of_alloc refuses what wyn_schema_of refuses", "no message");
    } else {
        ok("wyn_schema_of_alloc refuses what wyn_schema_of refuses");
    }

    /* A NULL type is a bug in the caller, not a crash. */
    err[0] = '\0';
    char out[64];
    if (wyn_schema_of(NULL, &g_env, out, sizeof(out), err, sizeof(err)) >= 0)
        bad("a NULL type is refused", "it was accepted");
    else
        ok("a NULL type is refused");
}

int main(void)
{
    puts("wyn_schema: deriving JSON Schema from Wyn types");
    arm1_flat();
    arm2_nesting();
    arm3_option();
    arm4_plain_enum();
    arm5_data_enum();
    arm6_determinism();
    arm7_rejections();
    arm7b_escaping();
    arm8_buffer();

    printf("\n%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
