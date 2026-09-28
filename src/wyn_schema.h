#ifndef WYN_SCHEMA_H
#define WYN_SCHEMA_H

/* wyn_schema - derive a provider-acceptable JSON Schema from a Wyn type.
 *
 * This is the core of `ai fn` (AI_NATIVE_DESIGN.md §3(b)): the checker already
 * knows the full return type, so the schema that a structured-outputs endpoint
 * enforces can be DERIVED rather than hand-written. Two jobs, one walk:
 *
 *   1. emit the schema, and
 *   2. REFUSE types the provider subset cannot express, at compile time.
 *
 * (2) is the part a library cannot give you. The subset does not accept
 * recursive schemas, so a self-referential type has no schema at all; and it
 * requires `additionalProperties:false` on every object, so a map with
 * arbitrary keys has none either. (`$ref`/`$defs` themselves ARE accepted - it
 * is recursion specifically that is not, so the rejection message must say
 * "recursive", not "no $ref".) Those are build errors at the declaration, not
 * 400s in production.
 *
 * Three properties this module guarantees, because callers depend on them:
 *
 *   - DETERMINISM. Object keys follow DECLARATION order, never hash or
 *     alphabetical order, and the emitter's own key order is fixed. The schema
 *     is frozen into generated C as a string literal and will later key
 *     record/replay cassettes, so a byte that moves between builds breaks
 *     reproducible output and silently invalidates every cassette.
 *   - TOTALITY. Every TypeKind is either mapped or rejected with a message that
 *     names the offending type, says why the provider cannot take it, and says
 *     what to write instead. There is no silent fallback.
 *   - NO TRUNCATION. A schema that does not fit is an error, never a short
 *     write: a truncated schema emitted into C is invalid JSON the compiler
 *     would have blessed.
 *
 * The mapping (AI_NATIVE_DESIGN.md §3(b)):
 *
 *   int          -> {"type":"integer"}
 *   float        -> {"type":"number"}
 *   string       -> {"type":"string"}
 *   bool         -> {"type":"boolean"}
 *   [T]          -> {"type":"array","items":<T>}
 *   Option<T>    -> <T>, and the field drops out of the enclosing "required"
 *   struct       -> {"type":"object","properties":{...},"required":[...],
 *                    "additionalProperties":false}
 *   enum, plain  -> {"enum":["A","B"]}
 *   enum+payload -> {"anyOf":[ <one object per variant> ]}, each object carrying
 *                   a "type" discriminant whose schema is
 *                   {"type":"string","const":"<VariantName>"}. Payload fields
 *                   are positional: exactly one payload is named "value",
 *                   otherwise they are "value0".."valueN-1".
 *
 * `required` lists every non-Option field, because the provider's strict mode
 * makes `required` and `additionalProperties:false` mandatory - which is why
 * optionality has to be expressed by OMISSION from `required` rather than by a
 * `"null"` union.
 */

#include <stddef.h>

#include "types.h"

/* Recursion is rejected by name, but a legal type can still nest deeply. This
 * bounds the walk (and therefore the cycle stack) so a pathological annotation
 * cannot exhaust the C stack. */
#define WYN_SCHEMA_MAX_DEPTH 32

/* The payload types of ONE enum variant, in declaration order. */
typedef struct {
    const Type** types; /* NULL iff count == 0 */
    int          count; /* 0 for a payload-free variant */
} WynSchemaPayload;

/* The one thing the Type model does not record.
 *
 * `Type` gives an enum's variant NAMES (EnumType.variants) but not its payload
 * types - those live on the AST (`EnumStmt.variant_types`, as unresolved
 * annotation *expressions*) and resolving an annotation to a Type is the
 * checker's job, not ours. Rather than grow a second copy of that resolution
 * here, the walker asks the host. Return 0 having filled *out, or -1 when the
 * enum is unknown (which the walker turns into a build error, never into a
 * guess that the enum is payload-free).
 */
typedef int (*WynSchemaEnumPayloadFn)(void* ctx, const char* enum_name,
                                      int variant_index, WynSchemaPayload* out);

typedef struct {
    WynSchemaEnumPayloadFn enum_payload;
    void*                  ctx;
} WynSchemaEnv;

/* Derive the schema for `t` into `out`.
 *
 * `env` may be NULL only when `t` provably contains no enum; an enum reached
 * with no resolver is an error, not an assumption.
 *
 * Returns the number of bytes written (excluding the NUL) on success, or -1 on
 * failure, in which case `err` holds a single-line, user-facing message. A
 * schema too large for `out` is a failure and `out` is left untouched: see
 * wyn_schema_of_alloc when the size is not known in advance.
 */
int wyn_schema_of(const Type* t, const WynSchemaEnv* env,
                  char* out, size_t outlen, char* err, size_t errlen);

/* Same derivation, heap-allocated - the shape codegen wants when it is about to
 * freeze the schema into a C string literal of unknown length. Returns a
 * malloc'd NUL-terminated string the caller frees, or NULL with `err` filled.
 * When `out_len` is non-NULL it receives strlen of the result. */
char* wyn_schema_of_alloc(const Type* t, const WynSchemaEnv* env,
                          size_t* out_len, char* err, size_t errlen);

#endif /* WYN_SCHEMA_H */
