#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_PAIRS 32

typedef struct {
    char* key;
    char* str_value;
    int int_value;
    int is_int;
} JsonPair;

struct WynJson {
    JsonPair pairs[MAX_PAIRS];
    int count;
};

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
        
        // Parse value
        if (*p == '"') {
            // String value
            char* value = parse_string(&p);
            json->pairs[json->count].key = key;
            json->pairs[json->count].str_value = value;
            json->pairs[json->count].is_int = 0;
            json->count++;
        } else if (isdigit(*p) || *p == '-') {
            // Int value
            int value = atoi(p);
            while (*p && (isdigit(*p) || *p == '-')) p++;
            json->pairs[json->count].key = key;
            json->pairs[json->count].int_value = value;
            json->pairs[json->count].is_int = 1;
            json->count++;
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
    for (int i = 0; i < json->count; i++) {
        free(json->pairs[i].key);
        if (!json->pairs[i].is_int && json->pairs[i].str_value) {
            free(json->pairs[i].str_value);
        }
    }
    free(json);
}

WynJson* json_new() {
    WynJson* json = calloc(1, sizeof(WynJson));
    return json;
}

void json_set_string(WynJson* json, const char* key, const char* value) {
    if (json->count < MAX_PAIRS) {
        json->pairs[json->count].key = strdup(key);
        json->pairs[json->count].str_value = strdup(value);
        json->pairs[json->count].is_int = 0;
        json->count++;
    }
}

void json_set_int(WynJson* json, const char* key, int value) {
    if (json->count < MAX_PAIRS) {
        json->pairs[json->count].key = strdup(key);
        json->pairs[json->count].int_value = value;
        json->pairs[json->count].is_int = 1;
        json->count++;
    }
}

char* json_stringify(WynJson* json) {
    char* buf = malloc(4096);
    int pos = 0;
    pos += snprintf(buf + pos, 4096 - pos, "{");
    for (int i = 0; i < json->count; i++) {
        if (i > 0) pos += snprintf(buf + pos, 4096 - pos, ", ");
        if (json->pairs[i].is_int) {
            pos += snprintf(buf + pos, 4096 - pos, "\"%s\": %d", json->pairs[i].key, json->pairs[i].int_value);
        } else {
            pos += snprintf(buf + pos, 4096 - pos, "\"%s\": \"%s\"", json->pairs[i].key, json->pairs[i].str_value);
        }
    }
    snprintf(buf + pos, 4096 - pos, "}");
    return buf;
}
