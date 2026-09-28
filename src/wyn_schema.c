/* wyn_schema.c - derive a provider-acceptable JSON Schema from a Wyn type.
 *
 * See src/wyn_schema.h for the mapping and the three guarantees. The shape of
 * this file follows from them:
 *
 *   - ONE emitter. Every brace, key and comma is written by emit_*() into a
 *     single growable buffer, in one pass, in source order. There is no second
 *     path that formats an object or an array differently, because two copies of
 *     "how a struct is spelled" is exactly how a schema starts drifting between
 *     the checker's view and codegen's.
 *   - ORDER IS THE DATA. StructType.field_names / EnumType.variants are arrays
 *     filled in declaration order by the checker; walking them by index is the
 *     whole of the determinism guarantee. Nothing here sorts, hashes, or
 *     iterates a map.
 *   - REJECT, NEVER FALL BACK. The TypeKind switch is exhaustive and every
 *     unmapped kind calls fail() with a message that names the offender, says
 *     why the provider cannot take it, and says what to write instead.
 *     -Wswitch keeps it exhaustive as TypeKind grows.
 *
 * Recursion is detected by keeping the struct/enum names currently being
 * expanded on a stack and refusing re-entry. A stack, not a visited set: a type
 * reached twice down two different branches (struct Trip { from: Address,
 * to: Address }) is perfectly fine, and a visited set would reject it.
 */

#include "wyn_schema.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Names and paths only ever appear in messages, so fixed bounds are fine - but
 * they have to be RELATED, not independently chosen. A path is composed from at
 * most two names plus a two-character separator ("Enum::Variant", "Doc.tags"),
 * so the path buffer must hold 2*SCHEMA_NAME_MAX + 3. With PATH == NAME == 128
 * and 256 respectively that was false by two bytes, and glibc's fortified
 * snprintf proved it: gcc -Werror=format-truncation failed the Linux CI job
 * while clang on macOS said nothing. The _Static_assert below is the fix - the
 * numbers cannot drift apart again without failing the build everywhere. */
#define SCHEMA_NAME_MAX  128
#define SCHEMA_PATH_MAX  512
#define SCHEMA_CHAIN_MAX 512
_Static_assert(SCHEMA_PATH_MAX >= 2 * SCHEMA_NAME_MAX + 3,
               "a path is <name> <2-char separator> <name> plus a NUL; "
               "SCHEMA_PATH_MAX must hold that or snprintf can truncate");

/* ------------------------------------------------------------------- buffer */

typedef struct {
    char*  data;
    size_t len;
    size_t cap;
    int    oom;
} Buf;

static void buf_free(Buf* b)
{
    free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}

static void buf_reserve(Buf* b, size_t extra)
{
    if (b->oom) return;
    size_t need = b->len + extra + 1;
    if (need <= b->cap) return;
    size_t cap = b->cap ? b->cap : 256;
    while (cap < need) cap *= 2;
    char* p = realloc(b->data, cap);
    if (!p) {
        b->oom = 1;
        return;
    }
    b->data = p;
    b->cap  = cap;
}

static void emit(Buf* b, const char* s)
{
    size_t n = strlen(s);
    buf_reserve(b, n);
    if (b->oom) return;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

/* Emit `tok`'s lexeme as a JSON string body (no surrounding quotes).
 *
 * Identifiers cannot contain anything that needs escaping, so this looks
 * paranoid - but the bytes come from source text via a (start, length) pair, and
 * an unescaped write is how a key-injection defect gets in. Escaping here costs
 * nothing and closes the question. */
static void emit_json_escaped(Buf* b, const char* s, int len)
{
    static const char* hex = "0123456789abcdef";
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  emit(b, "\\\""); break;
            case '\\': emit(b, "\\\\"); break;
            case '\n': emit(b, "\\n");  break;
            case '\r': emit(b, "\\r");  break;
            case '\t': emit(b, "\\t");  break;
            case '\b': emit(b, "\\b");  break;
            case '\f': emit(b, "\\f");  break;
            default:
                if (c < 0x20) {
                    char esc[7] = { '\\', 'u', '0', '0', hex[(c >> 4) & 0xf], hex[c & 0xf], 0 };
                    emit(b, esc);
                } else {
                    char one[2] = { (char)c, 0 };
                    emit(b, one);
                }
                break;
        }
    }
}

static void emit_quoted_token(Buf* b, Token t)
{
    emit(b, "\"");
    if (t.start && t.length > 0) emit_json_escaped(b, t.start, t.length);
    emit(b, "\"");
}

/* ---------------------------------------------------------------- walk state */

typedef struct {
    Buf                 buf;
    const WynSchemaEnv* env;
    char*               err;
    size_t              errlen;
    int                 failed;
    /* Names of the structs/enums currently being expanded, innermost last. */
    Token               stack[WYN_SCHEMA_MAX_DEPTH];
    int                 depth;
} Ctx;

static void fail(Ctx* c, const char* fmt, ...)
{
    if (c->failed) return; /* keep the FIRST, innermost cause */
    c->failed = 1;
    if (c->err && c->errlen) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(c->err, c->errlen, fmt, ap);
        va_end(ap);
    }
}

/* A type's own name, for messages. Struct names live on StructType.name; every
 * other kind uses Type.name. */
static Token type_name(const Type* t)
{
    Token none = { TOKEN_IDENT, NULL, 0, 0 };
    if (!t) return none;
    if (t->kind == TYPE_STRUCT && t->struct_type.name.start && t->struct_type.name.length > 0)
        return t->struct_type.name;
    if (t->name.start && t->name.length > 0) return t->name;
    return none;
}

static void name_to_cstr(char* buf, size_t bufsz, Token t)
{
    if (!t.start || t.length <= 0) {
        snprintf(buf, bufsz, "%s", "<anonymous>");
        return;
    }
    token_to_cstr(buf, bufsz, t);
}

static int token_eq(Token a, Token b)
{
    return a.start && b.start && a.length == b.length &&
           memcmp(a.start, b.start, (size_t)a.length) == 0;
}

/* "Alpha -> Beta -> Alpha": the chain from where `name` was first entered back
 * round to itself. Naming the path is the difference between a message you can
 * act on and "recursive type". */
static void cycle_chain(const Ctx* c, Token name, char* out, size_t outlen)
{
    int start = 0;
    for (int i = 0; i < c->depth; i++) {
        if (token_eq(c->stack[i], name)) { start = i; break; }
    }
    size_t used = 0;
    out[0] = '\0';
    for (int i = start; i < c->depth; i++) {
        char one[SCHEMA_NAME_MAX];
        name_to_cstr(one, sizeof(one), c->stack[i]);
        int n = snprintf(out + used, outlen - used, "%s -> ", one);
        if (n < 0 || (size_t)n >= outlen - used) return;
        used += (size_t)n;
    }
    char self[SCHEMA_NAME_MAX];
    name_to_cstr(self, sizeof(self), name);
    snprintf(out + used, outlen - used, "%s", self);
}

static void schema_of_type(Ctx* c, const Type* t, const char* path);

/* True when a struct field's declared type makes it optional, i.e. it is
 * omitted from "required". ONE predicate, used by both the properties pass and
 * the required pass - deciding it twice is how the two lists drift apart. */
static int field_is_optional(const Type* ft)
{
    return ft && ft->kind == TYPE_OPTIONAL;
}

/* {"type":"object","properties":{...},"required":[...],"additionalProperties":false}
 *
 * The one place an object schema is spelled. Struct bodies and the per-variant
 * branches of a data enum's anyOf both come through here, so they cannot
 * disagree about key order or about additionalProperties.
 */
typedef struct {
    Token       name;          /* property name */
    const Type* type;          /* NULL when `const_value` is set instead */
    Token       const_value;   /* discriminant: emits {"type":"string","const":"<v>"} */
    int         is_const;
    int         optional;      /* omit from "required" */
} Prop;

static void emit_object(Ctx* c, const Prop* props, int nprops, const char* path)
{
    emit(&c->buf, "{\"type\":\"object\",\"properties\":{");
    for (int i = 0; i < nprops && !c->failed; i++) {
        if (i) emit(&c->buf, ",");
        emit_quoted_token(&c->buf, props[i].name);
        emit(&c->buf, ":");
        if (props[i].is_const) {
            /* `type` as well as `const`: Anthropic's subset accepts a bare
             * `const`, but a strict OpenAI-compatible endpoint (AI_NATIVE_DESIGN
             * §13.2 stage 3) wants every property to carry a type. One extra key
             * makes the same derived schema portable across both. */
            emit(&c->buf, "{\"type\":\"string\",\"const\":");
            emit_quoted_token(&c->buf, props[i].const_value);
            emit(&c->buf, "}");
        } else {
            char child[SCHEMA_PATH_MAX];
            char fname[SCHEMA_NAME_MAX];
            name_to_cstr(fname, sizeof(fname), props[i].name);
            snprintf(child, sizeof(child), "%s.%s", path, fname);
            /* Option<T> contributes T's schema; its optionality is expressed by
             * absence from "required", never by a null union (the provider's
             * strict subset has no room for one). */
            const Type* ft = props[i].type;
            if (field_is_optional(ft)) {
                const Type* inner = ft->optional_type.inner_type;
                if (!inner) {
                    fail(c,
                         "%s: Option with no inner type - the checker resolved "
                         "`T?` without a T, so there is nothing to describe. "
                         "Annotate the field explicitly (e.g. `name: string?`).",
                         child);
                    return;
                }
                if (inner->kind == TYPE_OPTIONAL) {
                    fail(c,
                         "%s: Option<Option<T>> has no JSON Schema - JSON has a "
                         "single null, so the two levels of absence collapse "
                         "into one and the model cannot tell them apart. Use a "
                         "single Option, or an enum with payloads if you need to "
                         "distinguish \"missing\" from \"explicitly empty\".",
                         child);
                    return;
                }
                schema_of_type(c, inner, child);
            } else {
                schema_of_type(c, ft, child);
            }
        }
    }
    if (c->failed) return;
    emit(&c->buf, "},\"required\":[");
    int written = 0;
    for (int i = 0; i < nprops; i++) {
        if (props[i].optional) continue;
        if (written++) emit(&c->buf, ",");
        emit_quoted_token(&c->buf, props[i].name);
    }
    /* "required":[] stays, even when every field is optional: strict structured
     * outputs want the key present. */
    emit(&c->buf, "],\"additionalProperties\":false}");
}

static void schema_of_struct(Ctx* c, const Type* t, const char* path)
{
    Token nm = type_name(t);
    char  nmbuf[SCHEMA_NAME_MAX];
    name_to_cstr(nmbuf, sizeof(nmbuf), nm);

    if (t->struct_type.field_count <= 0 || !t->struct_type.field_names ||
        !t->struct_type.field_types) {
        fail(c,
             "%s: type '%s' exposes no fields the schema can see - an opaque or "
             "unresolved annotation (`ptr`, a C FFI handle, or a name the "
             "checker could not resolve) has no JSON representation. Give it "
             "fields of int/float/string/bool/[T]/Option<T>/struct/enum, or "
             "return a type that does.",
             path, nmbuf);
        return;
    }

    for (int i = 0; i < c->depth; i++) {
        if (token_eq(c->stack[i], nm)) {
            char chain[SCHEMA_CHAIN_MAX];
            cycle_chain(c, nm, chain, sizeof(chain));
            fail(c,
                 "%s: recursive type '%s' cannot be derived - the provider's "
                 "structured-output subset does not accept recursive schemas, so "
                 "a type that contains itself (cycle: %s) has no schema at all. "
                 "Flatten it: return a list whose records carry an id and a "
                 "parent_id, or return the nested part as a string and parse it "
                 "yourself.",
                 path, nmbuf, chain);
            return;
        }
    }
    if (c->depth >= WYN_SCHEMA_MAX_DEPTH) {
        fail(c,
             "%s: type nesting exceeds %d levels, which the provider's schema "
             "subset is not a place for. Flatten the shape into fewer levels.",
             path, WYN_SCHEMA_MAX_DEPTH);
        return;
    }
    c->stack[c->depth++] = nm;

    int   n = t->struct_type.field_count;
    Prop* props = calloc((size_t)n, sizeof(Prop));
    if (!props) {
        c->buf.oom = 1;
        c->depth--;
        return;
    }
    for (int i = 0; i < n; i++) {
        props[i].name     = t->struct_type.field_names[i];
        props[i].type     = t->struct_type.field_types[i];
        props[i].optional = field_is_optional(props[i].type);
    }
    /* The path base is the struct's own name, so a message reads "Doc.tags"
     * rather than the whole route taken to reach Doc. */
    emit_object(c, props, n, (nm.start && nm.length > 0) ? nmbuf : path);
    free(props);
    c->depth--;
}

/* A plain enum is {"enum":[...]}; one with payloads is a discriminated anyOf.
 * Which it is depends on payload types the Type model does not carry, so the
 * host answers (see WynSchemaEnv in the header) - and "the host does not know
 * this enum" is an error, never an assumption that it is payload-free. */
static void schema_of_enum(Ctx* c, const Type* t, const char* path)
{
    Token nm = type_name(t);
    char  nmbuf[SCHEMA_NAME_MAX];
    name_to_cstr(nmbuf, sizeof(nmbuf), nm);

    int nv = t->enum_type.variant_count;
    if (nv <= 0 || !t->enum_type.variants) {
        fail(c,
             "%s: enum '%s' has no variants, so there is nothing for the model "
             "to choose. Give it at least one variant.",
             path, nmbuf);
        return;
    }

    if (!c->env || !c->env->enum_payload) {
        fail(c,
             "%s: enum '%s' cannot be derived here - no variant-payload "
             "resolver is installed (internal: WynSchemaEnv.enum_payload is "
             "NULL and the Type model does not record enum payloads).",
             path, nmbuf);
        return;
    }

    for (int i = 0; i < c->depth; i++) {
        if (token_eq(c->stack[i], nm)) {
            char chain[SCHEMA_CHAIN_MAX];
            cycle_chain(c, nm, chain, sizeof(chain));
            fail(c,
                 "%s: recursive type '%s' cannot be derived - the provider's "
                 "structured-output subset does not accept recursive schemas, so "
                 "an enum whose payload reaches itself (cycle: %s) has no schema "
                 "at all. Flatten it: return a list whose records carry an id "
                 "and a parent_id, or return the nested part as a string and "
                 "parse it yourself.",
                 path, nmbuf, chain);
            return;
        }
    }
    if (c->depth >= WYN_SCHEMA_MAX_DEPTH) {
        fail(c,
             "%s: type nesting exceeds %d levels, which the provider's schema "
             "subset is not a place for. Flatten the shape into fewer levels.",
             path, WYN_SCHEMA_MAX_DEPTH);
        return;
    }

    /* Two passes over the variants so the cheap {"enum":[...]} form is chosen
     * only when EVERY variant is payload-free. A mixed enum is an anyOf whose
     * bare variants are objects carrying just the discriminant - one shape for
     * the whole type, which the generated parser depends on. */
    WynSchemaPayload* payloads = calloc((size_t)nv, sizeof(WynSchemaPayload));
    if (!payloads) {
        c->buf.oom = 1;
        return;
    }
    int any_payload = 0;
    for (int v = 0; v < nv; v++) {
        if (c->env->enum_payload(c->env->ctx, nmbuf, v, &payloads[v]) != 0) {
            fail(c,
                 "%s: enum '%s' has no declaration the compiler can see, so its "
                 "variant payloads are unknown and deriving a schema would have "
                 "to guess. Declare the enum in this program, or import the "
                 "module that declares it.",
                 path, nmbuf);
            free(payloads);
            return;
        }
        if (payloads[v].count > 0) any_payload = 1;
    }

    if (!any_payload) {
        emit(&c->buf, "{\"enum\":[");
        for (int v = 0; v < nv; v++) {
            if (v) emit(&c->buf, ",");
            emit_quoted_token(&c->buf, t->enum_type.variants[v]);
        }
        emit(&c->buf, "]}");
        free(payloads);
        return;
    }

    c->stack[c->depth++] = nm;
    emit(&c->buf, "{\"anyOf\":[");
    for (int v = 0; v < nv && !c->failed; v++) {
        if (v) emit(&c->buf, ",");
        int np = payloads[v].count;
        if (np < 0) np = 0;

        /* Discriminant first, then the positional payload slots. Exactly one
         * payload is "value"; more than one is "value0".."valueN-1". Naming the
         * common case "value" is for the model reading the schema, which is the
         * only reader whose comprehension is not free. */
        Prop* props = calloc((size_t)np + 1, sizeof(Prop));
        char (*slots)[16] = calloc((size_t)np + 1, sizeof(*slots));
        if (!props || !slots) {
            free(props);
            free(slots);
            c->buf.oom = 1;
            break;
        }
        static const char kDiscriminant[] = "type";
        props[0].name        = (Token){ TOKEN_IDENT, kDiscriminant, 4, 0 };
        props[0].is_const    = 1;
        props[0].const_value = t->enum_type.variants[v];

        for (int i = 0; i < np; i++) {
            if (np == 1)
                snprintf(slots[i], sizeof(slots[i]), "value");
            else
                snprintf(slots[i], sizeof(slots[i]), "value%d", i);
            props[i + 1].name = (Token){ TOKEN_IDENT, slots[i], (int)strlen(slots[i]), 0 };
            props[i + 1].type = payloads[v].types ? payloads[v].types[i] : NULL;
            /* A positional slot is always required: the discriminated object has
             * no meaningful "absent" for it, so an Option payload contributes
             * its inner schema and stays required. */
            props[i + 1].optional = 0;
        }

        char vpath[SCHEMA_PATH_MAX];
        char vname[SCHEMA_NAME_MAX];
        name_to_cstr(vname, sizeof(vname), t->enum_type.variants[v]);
        snprintf(vpath, sizeof(vpath), "%s::%s", nmbuf, vname);
        emit_object(c, props, np + 1, vpath);

        free(props);
        free(slots);
    }
    if (!c->failed) emit(&c->buf, "]}");
    c->depth--;
    free(payloads);
}

static void schema_of_type(Ctx* c, const Type* t, const char* path)
{
    if (c->failed || c->buf.oom) return;

    if (!t) {
        fail(c,
             "%s: the checker resolved no type here - that is what an opaque or "
             "unknown annotation (`ptr`, or a name that does not resolve) looks "
             "like at schema derivation. Annotate it with a type the schema can "
             "express: int/float/string/bool/[T]/Option<T>/struct/enum.",
             path);
        return;
    }

    char nmbuf[SCHEMA_NAME_MAX];
    name_to_cstr(nmbuf, sizeof(nmbuf), type_name(t));

    switch (t->kind) {
        case TYPE_INT:    emit(&c->buf, "{\"type\":\"integer\"}"); return;
        case TYPE_FLOAT:  emit(&c->buf, "{\"type\":\"number\"}");  return;
        case TYPE_STRING: emit(&c->buf, "{\"type\":\"string\"}");  return;
        case TYPE_BOOL:   emit(&c->buf, "{\"type\":\"boolean\"}"); return;

        case TYPE_ARRAY: {
            const Type* el = t->array_type.element_type;
            if (!el) {
                fail(c,
                     "%s: array with no element type - `[]` alone does not say "
                     "what the model should produce. Annotate the element type "
                     "(e.g. `[int]`, `[LineItem]`).",
                     path);
                return;
            }
            char child[SCHEMA_PATH_MAX];
            snprintf(child, sizeof(child), "%s[]", path);
            emit(&c->buf, "{\"type\":\"array\",\"items\":");
            /* An Option INSIDE an array has no "required" list to drop out of,
             * so it degrades to the inner schema. Nothing is lost: a JSON array
             * cannot have a missing element, only a null one, and null is not
             * in the strict subset. */
            if (el->kind == TYPE_OPTIONAL && el->optional_type.inner_type)
                el = el->optional_type.inner_type;
            schema_of_type(c, el, child);
            if (!c->failed) emit(&c->buf, "}");
            return;
        }

        case TYPE_OPTIONAL: {
            /* Reached only at the root, or nested somewhere with no enclosing
             * "required" (a struct field is handled in emit_object, which is
             * what makes the field drop out of "required"). Emit T's schema. */
            const Type* inner = t->optional_type.inner_type;
            if (!inner) {
                fail(c,
                     "%s: Option with no inner type - the checker resolved `T?` "
                     "without a T, so there is nothing to describe. Annotate it "
                     "explicitly (e.g. `string?`).",
                     path);
                return;
            }
            if (inner->kind == TYPE_OPTIONAL) {
                fail(c,
                     "%s: Option<Option<T>> has no JSON Schema - JSON has a "
                     "single null, so the two levels of absence collapse into "
                     "one and the model cannot tell them apart. Use a single "
                     "Option, or an enum with payloads if you need to "
                     "distinguish \"missing\" from \"explicitly empty\".",
                     path);
                return;
            }
            schema_of_type(c, inner, path);
            return;
        }

        case TYPE_STRUCT: schema_of_struct(c, t, path); return;
        case TYPE_ENUM:   schema_of_enum(c, t, path);   return;

        /* ---- no schema in the provider's strict subset -------------------- */

        case TYPE_MAP:
            fail(c,
                 "%s: a map type has no JSON Schema in the provider's strict "
                 "subset - an object with arbitrary keys needs "
                 "\"additionalProperties\", and structured outputs require "
                 "\"additionalProperties\":false. Return an array of two-field "
                 "records instead (e.g. `struct Pair { key: string, value: int "
                 "}` and `[Pair]`).",
                 path);
            return;

        case TYPE_SET:
            fail(c,
                 "%s: a set type has no JSON Schema in the provider's strict "
                 "subset - JSON has no set, and \"uniqueItems\" is not in the "
                 "accepted subset. Return `[T]` and de-duplicate after the "
                 "call.",
                 path);
            return;

        case TYPE_FUNCTION:
            fail(c,
                 "%s: a function type has no JSON Schema - a model returns data, "
                 "not code. Return the value the function would produce, or a "
                 "payload-free enum naming the operation you want performed.",
                 path);
            return;

        case TYPE_UNION:
            fail(c,
                 "%s: a union type has no JSON Schema in the provider's strict "
                 "subset as written. Declare an enum with payloads instead - it "
                 "derives a discriminated \"anyOf\", which the provider does "
                 "accept.",
                 path);
            return;

        case TYPE_RESULT:
            fail(c,
                 "%s: a nested Result has no JSON Schema here - an `ai fn`'s "
                 "declared return T is already delivered as `Result<T, "
                 "AiError>`, so the model never produces the Ok/Err wrapper. "
                 "Declare the inner type directly and let failure arrive as "
                 "AiError.",
                 path);
            return;

        case TYPE_GENERIC:
            fail(c,
                 "%s: unresolved generic type parameter '%s' - the schema is "
                 "frozen at compile time, so derivation needs a concrete type. "
                 "Write the concrete struct or enum you expect (e.g. `Invoice` "
                 "instead of `%s`).",
                 path, nmbuf, nmbuf);
            return;

        case TYPE_JSON:
            fail(c,
                 "%s: an untyped json value has no JSON Schema - structured "
                 "outputs need concrete fields, which is the entire point of "
                 "deriving the schema. Declare a struct with the fields you "
                 "expect, or return `string` and parse it yourself.",
                 path);
            return;

        case TYPE_CHANNEL:
            fail(c,
                 "%s: a channel handle has no JSON Schema - a channel is a "
                 "local runtime object, not data a model can produce. Return "
                 "the values you would send over it.",
                 path);
            return;

        case TYPE_VOID:
            fail(c,
                 "%s: void cannot be an `ai fn` return type - there is nothing "
                 "for the model to produce. Return a struct, an enum, or a "
                 "primitive (int/float/string/bool).",
                 path);
            return;
    }

    /* No default: above, so -Wswitch flags a new TypeKind at compile time
     * instead of it silently reaching here. This is the belt to that braces. */
    fail(c,
         "%s: type kind %d has no JSON Schema mapping yet. Return a struct, an "
         "enum, or a primitive (int/float/string/bool), or add the mapping to "
         "src/wyn_schema.c.",
         path, (int)t->kind);
}

/* ------------------------------------------------------------------ entry */

static char* derive(const Type* t, const WynSchemaEnv* env, size_t* out_len,
                    char* err, size_t errlen)
{
    if (err && errlen) err[0] = '\0';

    Ctx c;
    memset(&c, 0, sizeof(c));
    c.env    = env;
    c.err    = err;
    c.errlen = errlen;

    Token nm = type_name(t);
    char  root[SCHEMA_PATH_MAX];
    if (nm.start && nm.length > 0)
        name_to_cstr(root, sizeof(root), nm);
    else
        snprintf(root, sizeof(root), "%s", "return type");

    schema_of_type(&c, t, root);

    if (c.buf.oom) {
        if (err && errlen)
            snprintf(err, errlen, "out of memory deriving the schema for '%s'", root);
        buf_free(&c.buf);
        return NULL;
    }
    if (c.failed) {
        buf_free(&c.buf);
        return NULL;
    }
    if (!c.buf.data) {
        /* Unreachable: every successful path emits at least one brace. Handled
         * rather than asserted so a future mapping that emits nothing surfaces
         * as an error instead of a NULL deref in codegen. */
        if (err && errlen)
            snprintf(err, errlen, "internal: derived an empty schema for '%s'", root);
        return NULL;
    }
    if (out_len) *out_len = c.buf.len;
    return c.buf.data;
}

int wyn_schema_of(const Type* t, const WynSchemaEnv* env,
                  char* out, size_t outlen, char* err, size_t errlen)
{
    size_t len  = 0;
    char*  text = derive(t, env, &len, err, errlen);
    if (!text) return -1;

    /* Truncation is a failure, never a short write: a half-schema frozen into
     * generated C is invalid JSON that the compiler would have blessed. */
    if (!out || outlen == 0 || len + 1 > outlen) {
        if (err && errlen)
            snprintf(err, errlen,
                     "the derived schema needs %zu bytes but the output buffer "
                     "is %zu - internal limit, not a user error; use "
                     "wyn_schema_of_alloc.",
                     len + 1, outlen);
        free(text);
        return -1;
    }
    memcpy(out, text, len + 1);
    free(text);
    return (int)len;
}

char* wyn_schema_of_alloc(const Type* t, const WynSchemaEnv* env,
                          size_t* out_len, char* err, size_t errlen)
{
    return derive(t, env, out_len, err, errlen);
}
