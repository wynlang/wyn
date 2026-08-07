#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char* key;
    char* str_value;
    int int_value;
    int is_int;
} JsonPair;

// Pairs GROW. This was a fixed 32-entry array and json_set_string/json_set_int silently
// did nothing once it was full, so the 33rd key onwards vanished with no error and
// json_stringify emitted a document quietly missing fields.
struct WynJson {
    JsonPair* pairs;
    int count;
    int cap;
};

static int json_reserve(WynJson* json) {
    if (json->count < json->cap) return 1;
    int ncap = json->cap ? json->cap * 2 : 16;
    JsonPair* np = realloc(json->pairs, sizeof(JsonPair) * (size_t)ncap);
    if (!np) return 0;
    json->pairs = np;
    json->cap = ncap;
    return 1;
}

static void skip_whitespace(const char** p) {
    while (**p && isspace(**p)) (*p)++;
}

static char* parse_string(const char** p) {
    if (**p != '"') return NULL;
    (*p)++; // Skip opening quote
    
    const char* start = *p;
    while (**p && **p != '"') (*p)++;
    
    int len = *p - start;
    char* str = malloc(len + 1);
    memcpy(str, start, len);
    str[len] = '\0';
    
    if (**p == '"') (*p)++; // Skip closing quote
    return str;
}

WynJson* json_parse(const char* text) {
    WynJson* json = calloc(1, sizeof(WynJson));
    const char* p = text;
    
    skip_whitespace(&p);
    if (*p != '{') return json;
    p++; // Skip {
    
    while (*p && *p != '}') {
        skip_whitespace(&p);
        if (*p == '}') break;
        
        // Parse key
        char* key = parse_string(&p);
        if (!key) break;
        
        skip_whitespace(&p);
        if (*p != ':') { free(key); break; }
        p++; // Skip :
        
        skip_whitespace(&p);
        
        // Parse value. json_reserve() must come BEFORE any pairs[count] write now that the
        // array is heap-allocated: it used to be inline storage, so an unreserved write was
        // merely a bounded overflow rather than a null dereference.
        if (*p == '"') {
            // String value
            char* value = parse_string(&p);
            if (!json_reserve(json)) { free(key); free(value); break; }
            json->pairs[json->count].key = key;
            json->pairs[json->count].str_value = value;
            json->pairs[json->count].int_value = 0;
            json->pairs[json->count].is_int = 0;
            json->count++;
        } else if (isdigit(*p) || *p == '-') {
            // Int value
            int value = atoi(p);
            while (*p && (isdigit(*p) || *p == '-')) p++;
            if (!json_reserve(json)) { free(key); break; }
            json->pairs[json->count].key = key;
            json->pairs[json->count].str_value = NULL;
            json->pairs[json->count].int_value = value;
            json->pairs[json->count].is_int = 1;
            json->count++;
        } else {
            // Neither a string nor a number: `key` would otherwise leak, and without a
            // guaranteed advance the loop could spin on the same byte.
            free(key);
            break;
        }
        
        skip_whitespace(&p);
        if (*p == ',') p++;
    }
    
    return json;
}

char* json_get_string(WynJson* json, const char* key) {
    for (int i = 0; i < json->count; i++) {
        if (strcmp(json->pairs[i].key, key) == 0 && !json->pairs[i].is_int) {
            return json->pairs[i].str_value;
        }
    }
    return NULL;
}

int json_get_int(WynJson* json, const char* key) {
    for (int i = 0; i < json->count; i++) {
        if (strcmp(json->pairs[i].key, key) == 0 && json->pairs[i].is_int) {
            return json->pairs[i].int_value;
        }
    }
    return 0;
}

void json_free(WynJson* json) {
    if (!json) return;
    for (int i = 0; i < json->count; i++) {
        free(json->pairs[i].key);
        if (!json->pairs[i].is_int && json->pairs[i].str_value) {
            free(json->pairs[i].str_value);
        }
    }
    free(json->pairs);   // the pair array is heap-allocated now, not inline
    free(json);
}

WynJson* json_new() {
    WynJson* json = calloc(1, sizeof(WynJson));
    return json;
}

void json_set_string(WynJson* json, const char* key, const char* value) {
    if (!json_reserve(json)) return;
    json->pairs[json->count].key = strdup(key);
    json->pairs[json->count].str_value = strdup(value ? value : "");
    json->pairs[json->count].int_value = 0;
    json->pairs[json->count].is_int = 0;
    json->count++;
}

void json_set_int(WynJson* json, const char* key, int value) {
    if (!json_reserve(json)) return;
    json->pairs[json->count].key = strdup(key);
    json->pairs[json->count].str_value = NULL;
    json->pairs[json->count].int_value = value;
    json->pairs[json->count].is_int = 1;
    json->count++;
}

// A growable output buffer, so a document larger than the old fixed 4096 bytes is not
// truncated mid-string into an unterminated document.
typedef struct { char* data; size_t len, cap; } JsonOut;

static int json_out_reserve(JsonOut* o, size_t extra) {
    if (o->len + extra + 1 <= o->cap) return 1;
    size_t ncap = o->cap ? o->cap : 256;
    while (ncap < o->len + extra + 1) ncap *= 2;
    char* nd = realloc(o->data, ncap);
    if (!nd) return 0;
    o->data = nd;
    o->cap = ncap;
    return 1;
}

static void json_out_raw(JsonOut* o, const char* s) {
    size_t n = strlen(s);
    if (!json_out_reserve(o, n)) return;
    memcpy(o->data + o->len, s, n);
    o->len += n;
    o->data[o->len] = 0;
}

// Emit a JSON string literal with the escapes RFC 8259 requires. Without this, any value
// containing a quote produced invalid JSON: Json.set(j, "name", "a\"b") stringified to
// {"name": "a"b"}, which let a caller-supplied value inject arbitrary keys ("role":"admin")
// and which this library's own parser then hung on. Control characters below 0x20 must be
// escaped too or the output is not parseable by anything.
static void json_out_quoted(JsonOut* o, const char* s) {
    if (!s) s = "";
    json_out_raw(o, "\"");
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        switch (*p) {
            case '"':  json_out_raw(o, "\\\""); break;
            case '\\': json_out_raw(o, "\\\\"); break;
            case '\n': json_out_raw(o, "\\n");  break;
            case '\r': json_out_raw(o, "\\r");  break;
            case '\t': json_out_raw(o, "\\t");  break;
            case '\b': json_out_raw(o, "\\b");  break;
            case '\f': json_out_raw(o, "\\f");  break;
            default:
                if (*p < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof(esc), "\\u%04x", (unsigned)*p);
                    json_out_raw(o, esc);
                } else {
                    char one[2] = {(char)*p, 0};
                    json_out_raw(o, one);
                }
        }
    }
    json_out_raw(o, "\"");
}

char* json_stringify(WynJson* json) {
    JsonOut o = {0};
    json_out_raw(&o, "{");
    for (int i = 0; i < json->count; i++) {
        if (i > 0) json_out_raw(&o, ", ");
        json_out_quoted(&o, json->pairs[i].key);
        json_out_raw(&o, ": ");
        if (json->pairs[i].is_int) {
            char num[32];
            snprintf(num, sizeof(num), "%d", json->pairs[i].int_value);
            json_out_raw(&o, num);
        } else {
            json_out_quoted(&o, json->pairs[i].str_value);
        }
    }
    json_out_raw(&o, "}");
    if (!o.data) return strdup("{}");
    return o.data;
}
