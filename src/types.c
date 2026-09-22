// Phase 1 Task 1.2: Method Signature Table Implementation
#include "types.h"
#include <string.h>
#include <stddef.h>

// Method signature table - maps (receiver_type, method_name) -> return_type
static const MethodSignature method_signatures[] = {
    // String methods
    {"string", "upper", "string", 0},
    {"string", "lower", "string", 0},
    {"string", "trim", "string", 0},
    {"string", "to_string", "string", 0}, // identity - supported by codegen (generic to_string path)
    {"string", "trim_left", "string", 0},
    {"string", "trim_right", "string", 0},
    {"string", "split", "array", 1},     // Returns array of strings
    {"string", "charAt", "string", 1},   // Returns single char as string
    {"string", "capitalize", "string", 0},
    {"string", "title", "string", 0},
    {"string", "reverse", "string", 0},
    {"string", "to_bytes", "array", 0},  // Returns Vec<int>
    {"string", "bytes", "array", 0},
    {"string", "chars", "array", 0},     // Returns Vec<string>
    {"string", "len", "int", 0},
    {"string", "is_empty", "bool", 0},
    {"string", "contains", "bool", 1},
    {"string", "starts_with", "bool", 1},
    {"string", "ends_with", "bool", 1},
    {"string", "index_of", "int", 1},    // Returns -1 if not found
    {"string", "replace", "string", 2},
    {"string", "slice", "string", 2},
    {"string", "substring", "string", 2},
    {"string", "repeat", "string", 1},
    {"string", "pad_left", "string", 2},
    {"string", "pad_right", "string", 2},
    {"string", "lines", "array", 0},     // Returns Vec<string>
    {"string", "words", "array", 0},     // Returns Vec<string>
    {"string", "concat", "string", 1},
    {"string", "replace_all", "string", 2},  // replace_all(old, new)
    {"string", "last_index_of", "int", 1},   // Returns -1 if not found
    {"string", "is_alpha", "bool", 0},       // Check if all alphabetic
    {"string", "is_digit", "bool", 0},       // Check if all numeric
    {"string", "is_alnum", "bool", 0},       // Check if alphanumeric
    {"string", "is_whitespace", "bool", 0},  // Check if all whitespace
    {"string", "char_at", "string", 1},      // Get char at index
    {"string", "equals", "bool", 1},         // String equality
    {"string", "count", "int", 1},           // Count occurrences
    {"string", "is_numeric", "bool", 0},     // Check if numeric (int or float)
    {"string", "to_int", "int", 0},          // Parse string to int
    {"string", "ascii", "int", 0},           // ASCII value of first char
    {"string", "to_float", "float", 0},      // Parse string to float
    {"string", "parse_int", "int", 0},       // Parse string to int (alias)
    {"string", "parse_float", "float", 0},   // Parse string to float (alias)
    {"string", "parse_json", "json", 0},     // Parse JSON string, returns json object
    
    // JSON methods
    {"json", "get_string", "string", 1},     // Get string value by key
    {"json", "get_int", "int", 1},           // Get int value by key
    {"json", "get_float", "float", 1},       // Get float value by key
    {"json", "get_bool", "bool", 1},         // Get bool value by key
    {"json", "free", "void", 0},             // Free JSON object
    // The WRITERS were missing from this table and from dispatch_method below,
    // and a json method absent from BOTH emits nothing at all - so
    // `j.set_int("i", 1)` lowered to an empty statement and `j.stringify()` came
    // back `{}` with the writes silently gone, at exit 0. In expression position
    // the same gap produced `long long v = ;` -> "expected expression". The
    // readers were present, which is why half the surface worked. See the
    // matching entries in dispatch_method for the C functions.
    {"json", "set_string", "void", 2},       // Set string value by key
    {"json", "set_int", "void", 2},          // Set int value by key
    {"json", "set_bool", "void", 2},         // Set bool value by key
    {"json", "stringify", "string", 0},      // Serialize to JSON text
    
    // HTTP methods (URL is a string)
    {"string", "http_get", "string", 0},     // GET request, returns response body
    {"string", "http_post", "string", 1},    // POST request with body
    
    // String formatting
    {"string", "format", "string", -1},      // Variable args: format(arg1, arg2, ...)
    
    // File system methods (path is a string)
    {"string", "exists", "bool", 0},         // Check if path exists
    {"string", "is_file", "bool", 0},        // Check if path is a file
    {"string", "is_dir", "bool", 0},         // Check if path is a directory
    
    // Int methods
    {"int", "to_string", "string", 0},
    {"int", "to_int", "int", 0},    // identity - bool results are typed int (map.contains), keep .to_int() forgiving
    {"int", "to_float", "float", 0},
    {"int", "abs", "int", 0},
    {"int", "pow", "int", 1},
    {"int", "min", "int", 1},
    {"int", "max", "int", 1},
    {"int", "clamp", "int", 2},
    {"int", "is_even", "bool", 0},
    {"int", "is_odd", "bool", 0},
    {"int", "is_positive", "bool", 0},
    {"int", "is_negative", "bool", 0},
    {"int", "is_zero", "bool", 0},
    {"int", "sign", "int", 0},  // Returns -1, 0, or 1
    {"int", "to_binary", "string", 0},
    {"int", "to_hex", "string", 0},
    
    // Float methods
    {"float", "to_string", "string", 0},
    {"float", "to_int", "int", 0},
    {"float", "round", "float", 0},
    {"float", "floor", "float", 0},
    {"float", "ceil", "float", 0},
    {"float", "round_to", "float", 1},
    {"float", "abs", "float", 0},
    {"float", "pow", "float", 1},
    {"float", "sqrt", "float", 0},
    {"float", "min", "float", 1},
    {"float", "max", "float", 1},
    {"float", "clamp", "float", 2},
    {"float", "is_nan", "bool", 0},
    {"float", "is_infinite", "bool", 0},
    {"float", "is_finite", "bool", 0},
    {"float", "is_positive", "bool", 0},
    {"float", "is_negative", "bool", 0},
    {"float", "sin", "float", 0},
    {"float", "cos", "float", 0},
    {"float", "tan", "float", 0},
    {"float", "asin", "float", 0},
    {"float", "acos", "float", 0},
    {"float", "atan", "float", 0},
    {"float", "log", "float", 0},
    {"float", "log10", "float", 0},
    {"float", "log2", "float", 0},
    {"float", "exp", "float", 0},
    {"float", "sign", "float", 0},  // Returns -1.0, 0.0, or 1.0
    
    // Bool methods
    {"bool", "to_string", "string", 0},
    {"bool", "to_int", "int", 0},
    {"bool", "not", "bool", 0},
    {"bool", "and", "bool", 1},
    {"bool", "or", "bool", 1},
    {"bool", "xor", "bool", 1},
    
    // Char methods
    {"char", "to_string", "string", 0},
    {"char", "to_int", "int", 0},
    {"char", "is_alpha", "bool", 0},
    {"char", "is_numeric", "bool", 0},
    {"char", "is_alphanumeric", "bool", 0},
    {"char", "is_whitespace", "bool", 0},
    {"char", "is_uppercase", "bool", 0},
    {"char", "is_lowercase", "bool", 0},
    {"char", "to_upper", "char", 0},
    {"char", "to_lower", "char", 0},
    
    // Array/Vec methods (receiver type will be "array" for now)
    {"array", "len", "int", 0},
    {"array", "is_empty", "bool", 0},
    {"array", "push", "void", 1},
    {"array", "pop", "int", 0},         // Returns last element
    {"array", "get", "int", 1},        // Returns element (type depends on array)
    {"array", "contains", "bool", 1},
    {"array", "index_of", "int", 1},
    {"array", "reverse", "void", 0},   // Mutates in place
    {"array", "sort", "void", 0},      // Mutates in place
    {"array", "sorted", "array", 0},   // Non-mutating sorted copy (Python sorted)
    {"array", "sort_by", "array", 1},  // sort_by(key_fn) - sorted by key, monomorphized
    {"array", "max_by", "int", 1},     // max_by(key_fn) -> element (type depends on array)
    {"array", "min_by", "int", 1},     // min_by(key_fn) -> element (type depends on array)
    {"array", "group_by", "map", 1},   // group_by(key_fn) -> map of key -> [elements]
    {"array", "first", "int", 0},      // Returns first element
    {"array", "last", "int", 0},       // Returns last element
    {"array", "count", "int", 1},      // Count occurrences of value
    {"array", "is_empty", "bool", 0},  // Check if empty
    {"array", "take", "array", 1},     // Returns new array with first n elements
    {"array", "skip", "array", 1},     // Returns new array skipping first n elements
    {"array", "slice", "array", 2},    // Returns new array from start to end
    {"array", "join", "string", 1},    // Join elements with separator
    {"array", "concat", "array", 1},   // Returns new array concatenated with other
    {"array", "map", "array", 1},       // Higher-order: map(fn) -> array
    {"array", "filter", "array", 1},    // Higher-order: filter(fn) -> array
    {"array", "reduce", "int", 2},      // Higher-order: reduce(fn, initial) -> T
    {"array", "find", "optional", 1},   // find(fn) -> Option<T>
    {"array", "find_index", "int", 1},  // find_index(fn) -> int (-1 if not found)
    {"array", "any", "bool", 1},        // any(fn) -> bool
    {"array", "all", "bool", 1},        // all(fn) -> bool
    {"array", "partition", "array", 1}, // partition(fn) -> [array, array]
    {"array", "zip", "array", 1},       // zip(other) -> array of pairs
    {"array", "flatten", "array", 0},   // flatten() -> array
    {"array", "unique", "array", 0},    // unique() -> array
    {"array", "sum", "int", 0},         // sum() -> int (codegen: array_sum)
    {"array", "min", "int", 0},         // min() -> int (codegen: array_min)
    {"array", "max", "int", 0},         // max() -> int (codegen: array_max)
    {"array", "average", "float", 0},   // average() -> float (codegen: array_average)
    {"array", "clear", "void", 0},      // clear() -> void
    {"array", "each", "void", 1},       // each(fn) (codegen: array_each)
    {"array", "every", "bool", 1},      // every(fn) (codegen: array_every)
    {"array", "flat_map", "array", 1},  // flat_map(fn) (codegen: array_flat_map)
    {"array", "insert", "array", 2},    // insert(i, v) (codegen: array_insert)
    {"array", "remove_at", "array", 1}, // remove_at(i) (codegen: array_remove_at)

    // HashMap methods
    {"map", "insert", "void", 2},
    {"map", "set", "void", 2},
    {"map", "get", "string", 1},
    {"map", "get_int", "int", 1},
    {"map", "get_string", "string", 1},
    {"map", "insert", "void", 2},
    {"map", "insert_int", "void", 2},
    {"map", "insert_string", "void", 2},
    {"map", "set_string", "void", 2},
    {"map", "keys", "array", 0},
    {"map", "len", "int", 0},
    {"map", "contains", "int", 1},
    {"map", "set_int", "void", 2},
    {"map", "stringify", "string", 0},
    {"map", "remove", "void", 1},
    {"map", "contains", "bool", 1},
    {"map", "len", "int", 0},
    {"map", "is_empty", "bool", 0},
    {"map", "values", "array", 0},
    {"map", "clear", "void", 0},
    {"map", "get_or_default", "int", 2},  // Returns value or default
    {"map", "update", "void", 2},         // Update value with function (defer - needs lambdas)
    {"map", "merge", "void", 1},          // Merge with another map
    {"map", "entries", "array", 0},       // Returns array of [key, value] pairs
    {"map", "for_each", "void", 1},       // for_each(fn) - iterate with function
    {"map", "filter_keys", "map", 1},     // filter_keys(fn) -> map
    {"map", "map_values", "map", 1},      // map_values(fn) -> map
    
    // HashSet methods
    {"set", "insert", "void", 1},
    {"set", "contains", "bool", 1},
    {"set", "remove", "void", 1},
    {"set", "len", "int", 0},
    {"set", "is_empty", "bool", 0},
    {"set", "clear", "void", 0},
    {"set", "union", "set", 1},
    {"set", "intersection", "set", 1},
    {"set", "difference", "set", 1},
    {"set", "is_subset", "bool", 1},
    {"set", "is_superset", "bool", 1},
    {"set", "is_disjoint", "bool", 1},
    {"set", "symmetric_difference", "set", 1},  // Elements in either but not both
    {"set", "to_array", "array", 0},            // Convert to array
    {"set", "from_array", "set", 1},            // Create from array
    {"set", "filter", "set", 1},                // filter(fn) -> set
    {"set", "map", "set", 1},                   // map(fn) -> set
    {"set", "for_each", "void", 1},             // for_each(fn)
    
    // Option methods
    {"option", "is_some", "bool", 0},
    {"option", "is_none", "bool", 0},
    {"option", "unwrap", "int", 0},    // Type depends on Option<T>
    {"option", "unwrap_or", "int", 1}, // Type depends on Option<T>
    {"option", "expect", "int", 1},    // expect(msg: string) -> T
    {"option", "or_else", "option", 1}, // or_else(fn: () -> Option<T>) -> Option<T>
    {"option", "map", "option", 1},    // Higher-order: map(fn) -> Option<U>
    {"option", "and_then", "option", 1}, // Higher-order: and_then(fn) -> Option<U>
    {"option", "filter", "option", 1}, // Higher-order: filter(fn) -> Option<T>
    
    // Result methods
    {"result", "is_ok", "bool", 0},
    {"result", "is_err", "bool", 0},
    {"result", "unwrap", "int", 0},    // Type depends on Result<T,E>
    {"result", "unwrap_or", "int", 1}, // Type depends on Result<T,E>
    {"result", "expect", "int", 1},    // expect(msg: string) -> T
    {"result", "map_err", "result", 1}, // map_err(fn: E -> F) -> Result<T,F>
    {"result", "or_else", "result", 1}, // or_else(fn: E -> Result<T,F>) -> Result<T,F>
    {"result", "map", "result", 1},    // Higher-order: map(fn) -> Result<U,E>
    {"result", "and_then", "result", 1}, // Higher-order: and_then(fn) -> Result<U,E>
    
    // Sentinel - marks end of table
    {NULL, NULL, NULL, 0}
};

// Lookup method return type given receiver type and method name
const char* lookup_method_return_type(const char* receiver_type, const char* method_name) {
    if (!receiver_type || !method_name) {
        return NULL;
    }
    
    for (int i = 0; method_signatures[i].receiver_type != NULL; i++) {
        if (strcmp(method_signatures[i].receiver_type, receiver_type) == 0 &&
            strcmp(method_signatures[i].method_name, method_name) == 0) {
            return method_signatures[i].return_type;
        }
    }
    
    return NULL;  // Method not found
}

// How close two identifiers are, for every "did you mean" hint: differing chars +
// length difference. WYN_NAME_FAR means "not worth suggesting". One function
// because the value-receiver suggester and the namespace suggester must rank
// candidates the same way - two metrics would make `.uppr` and `Time.millis`
// disagree about what counts as a near miss for no reason a user could see.
#define WYN_NAME_FAR 4
int wyn_name_distance(const char* a, const char* b) {
    if (!a || !b) return WYN_NAME_FAR;
    size_t al = strlen(a), bl = strlen(b);
    int diff = (int)(al > bl ? al - bl : bl - al);
    if (diff > 2) return WYN_NAME_FAR;
    size_t shorter = al < bl ? al : bl;
    int match = 0;
    for (size_t c = 0; c < shorter; c++)
        if (a[c] == b[c]) match++;
    int d = (int)(shorter - match) + diff;
    return d < WYN_NAME_FAR ? d : WYN_NAME_FAR;
}

// Nearest known method name on a receiver, for "did you mean" hints when an
// unknown method is rejected. Returns NULL when nothing is within distance 3.
const char* suggest_method_name(const char* receiver_type, const char* method_name) {
    if (!receiver_type || !method_name) return NULL;
    const char* best = NULL;
    int best_dist = WYN_NAME_FAR;
    for (int i = 0; method_signatures[i].receiver_type != NULL; i++) {
        if (strcmp(method_signatures[i].receiver_type, receiver_type) != 0) continue;
        const char* cand = method_signatures[i].method_name;
        int d = wyn_name_distance(method_name, cand);
        if (d > 0 && d < best_dist) { best_dist = d; best = cand; }
    }
    return best;
}

// Get receiver type string from Type for method dispatch
const char* get_receiver_type_string(const Type* type) {
    if (!type) return NULL;
    
    switch (type->kind) {
        case TYPE_STRING: return "string";
        case TYPE_INT: return "int";
        case TYPE_FLOAT: return "float";
        case TYPE_BOOL: return "bool";
        case TYPE_ARRAY: return "array";
        case TYPE_MAP: return "map";
        case TYPE_SET: return "set";
        case TYPE_OPTIONAL: return "option";
        case TYPE_RESULT: return "result";
        case TYPE_JSON: return "json";
        case TYPE_ENUM:
            // Map enum names to method receiver types
            if (type->name.length == 6 && memcmp(type->name.start, "Option", 6) == 0) {
                return "option";
            }
            if (type->name.length == 6 && memcmp(type->name.start, "Result", 6) == 0) {
                return "result";
            }
            return NULL;
        default: return NULL;
    }
}

// Dispatch method call based on receiver type and method name
// Returns true if method was found, false otherwise
bool dispatch_method(const char* receiver_type, const char* method_name, int arg_count, MethodDispatch* out) {
    if (!receiver_type || !method_name || !out) return false;
    
    // Default: needs_args = true, pass_by_ref = false
    out->needs_args = true;
    out->pass_by_ref = false;
    
    // Dispatch by receiver type first
    if (strcmp(receiver_type, "string") == 0) {
        // String methods
        if (strcmp(method_name, "len") == 0 && arg_count == 0) {
            out->c_function = "string_len"; return true;
        }
        if (strcmp(method_name, "is_empty") == 0 && arg_count == 0) {
            out->c_function = "string_is_empty"; return true;
        }
        if (strcmp(method_name, "upper") == 0 && arg_count == 0) {
            out->c_function = "string_upper"; return true;
        }
        if (strcmp(method_name, "lower") == 0 && arg_count == 0) {
            out->c_function = "string_lower"; return true;
        }
        if (strcmp(method_name, "capitalize") == 0 && arg_count == 0) {
            out->c_function = "string_capitalize"; return true;
        }
        if (strcmp(method_name, "reverse") == 0 && arg_count == 0) {
            out->c_function = "string_reverse"; return true;
        }
        if (strcmp(method_name, "title") == 0 && arg_count == 0) {
            out->c_function = "string_title"; return true;
        }
        if (strcmp(method_name, "trim") == 0 && arg_count == 0) {
            out->c_function = "string_trim"; return true;
        }
        if (strcmp(method_name, "trim_left") == 0 && arg_count == 0) {
            out->c_function = "string_trim_left"; return true;
        }
        if (strcmp(method_name, "trim_right") == 0 && arg_count == 0) {
            out->c_function = "string_trim_right"; return true;
        }
        if (strcmp(method_name, "split") == 0 && arg_count == 1) {
            out->c_function = "string_split"; return true;
        }
        if (strcmp(method_name, "charAt") == 0 && arg_count == 1) {
            out->c_function = "wyn_string_charat"; return true;
        }
        if (strcmp(method_name, "chars") == 0 && arg_count == 0) {
            out->c_function = "string_chars"; return true;
        }
        if (strcmp(method_name, "to_bytes") == 0 && arg_count == 0) {
        }
        if (strcmp(method_name, "bytes") == 0 && arg_count == 0) {
            out->c_function = "string_to_bytes"; return true;
            out->c_function = "string_to_bytes"; return true;
        }
        if (strcmp(method_name, "pad_left") == 0 && arg_count == 2) {
            out->c_function = "string_pad_left"; return true;
        }
        if (strcmp(method_name, "pad_right") == 0 && arg_count == 2) {
            out->c_function = "string_pad_right"; return true;
        }
        if (strcmp(method_name, "contains") == 0 && arg_count == 1) {
            out->c_function = "string_contains"; return true;
        }
        if (strcmp(method_name, "starts_with") == 0 && arg_count == 1) {
            out->c_function = "string_starts_with"; return true;
        }
        if (strcmp(method_name, "ends_with") == 0 && arg_count == 1) {
            out->c_function = "string_ends_with"; return true;
        }
        if (strcmp(method_name, "index_of") == 0 && arg_count == 1) {
            out->c_function = "string_index_of"; return true;
        }
        if (strcmp(method_name, "replace") == 0 && arg_count == 2) {
            out->c_function = "string_replace"; return true;
        }
        if (strcmp(method_name, "slice") == 0 && arg_count == 2) {
            out->c_function = "string_slice"; return true;
        }
        if (strcmp(method_name, "substring") == 0 && arg_count == 2) {
            out->c_function = "string_substring"; return true;
        }
        if (strcmp(method_name, "repeat") == 0 && arg_count == 1) {
            out->c_function = "string_repeat"; return true;
        }
        if (strcmp(method_name, "lines") == 0 && arg_count == 0) {
            out->c_function = "string_lines"; return true;
        }
        if (strcmp(method_name, "words") == 0 && arg_count == 0) {
            out->c_function = "string_words"; return true;
        }
        if (strcmp(method_name, "replace_all") == 0 && arg_count == 2) {
            out->c_function = "string_replace_all"; return true;
        }
        if (strcmp(method_name, "last_index_of") == 0 && arg_count == 1) {
            out->c_function = "string_last_index_of"; return true;
        }
        if (strcmp(method_name, "concat") == 0 && arg_count == 1) {
            out->c_function = "string_concat"; return true;
        }
        if (strcmp(method_name, "is_alpha") == 0 && arg_count == 0) {
            out->c_function = "string_is_alpha"; return true;
        }
        if (strcmp(method_name, "is_digit") == 0 && arg_count == 0) {
            out->c_function = "string_is_digit"; return true;
        }
        if (strcmp(method_name, "is_alnum") == 0 && arg_count == 0) {
            out->c_function = "string_is_alnum"; return true;
        }
        if (strcmp(method_name, "is_whitespace") == 0 && arg_count == 0) {
            out->c_function = "string_is_whitespace"; return true;
        }
        if (strcmp(method_name, "char_at") == 0 && arg_count == 1) {
            out->c_function = "string_char_at"; return true;
        }
        if (strcmp(method_name, "equals") == 0 && arg_count == 1) {
            out->c_function = "string_equals"; return true;
        }
        if (strcmp(method_name, "count") == 0 && arg_count == 1) {
            out->c_function = "string_count"; return true;
        }
        if (strcmp(method_name, "is_numeric") == 0 && arg_count == 0) {
            out->c_function = "string_is_numeric"; return true;
        }
        if (strcmp(method_name, "parse_int") == 0 && arg_count == 0) {
            out->c_function = "str_parse_int"; return true;
        }
        if (strcmp(method_name, "parse_float") == 0 && arg_count == 0) {
            out->c_function = "str_parse_float"; return true;
        }
        if (strcmp(method_name, "to_int") == 0 && arg_count == 0) {
            out->c_function = "str_parse_int"; return true;
        }
        if (strcmp(method_name, "ascii") == 0 && arg_count == 0) {
            out->c_function = "str_ascii"; return true;
        }
        if (strcmp(method_name, "to_float") == 0 && arg_count == 0) {
            out->c_function = "str_parse_float"; return true;
        }
        // JSON parsing method
        if (strcmp(method_name, "parse_json") == 0 && arg_count == 0) {
            out->c_function = "json_parse"; return true;
        }
        // HTTP methods (URL is a string)
        if (strcmp(method_name, "http_get") == 0 && arg_count == 0) {
            out->c_function = "http_get"; return true;
        }
        if (strcmp(method_name, "http_post") == 0 && arg_count == 1) {
            out->c_function = "http_post"; return true;
        }
        // String formatting (variable args)
        if (strcmp(method_name, "format") == 0) {
            out->c_function = "string_format"; return true;
        }
        // File system methods (path is a string)
        if (strcmp(method_name, "exists") == 0 && arg_count == 0) {
            out->c_function = "_exists"; return true;
        }
        if (strcmp(method_name, "is_file") == 0 && arg_count == 0) {
            out->c_function = "_is_file"; return true;
        }
        if (strcmp(method_name, "is_dir") == 0 && arg_count == 0) {
            out->c_function = "_is_dir"; return true;
        }
        if (strcmp(method_name, "mkdir") == 0 && arg_count == 0) {
            out->c_function = "file_mkdir"; return true;
        }
        if (strcmp(method_name, "rmdir") == 0 && arg_count == 0) {
            out->c_function = "file_rmdir"; return true;
        }
        if (strcmp(method_name, "file_size") == 0 && arg_count == 0) {
            out->c_function = "file_size"; return true;
        }
        if (strcmp(method_name, "delete") == 0 && arg_count == 0) {
            out->c_function = "file_delete"; return true;
        }
        if (strcmp(method_name, "split_at") == 0 && arg_count == 2) {
            out->c_function = "split_get"; return true;
        }
        if (strcmp(method_name, "split_count") == 0 && arg_count == 1) {
            out->c_function = "split_count"; return true;
        }
        if (strcmp(method_name, "to_int") == 0 && arg_count == 0) {
            out->c_function = "str_to_int"; return true;
        }
        if (strcmp(method_name, "to_float") == 0 && arg_count == 0) {
            out->c_function = "str_to_float"; return true;
        }
        return false;
    }
    
    if (strcmp(receiver_type, "json") == 0) {
        // JSON object methods
        if (strcmp(method_name, "get_string") == 0 && arg_count == 1) {
            out->c_function = "json_get_string"; return true;
        }
        if (strcmp(method_name, "get_int") == 0 && arg_count == 1) {
            out->c_function = "json_get_int"; return true;
        }
        if (strcmp(method_name, "get_float") == 0 && arg_count == 1) {
            out->c_function = "json_get_float"; return true;
        }
        if (strcmp(method_name, "get_bool") == 0 && arg_count == 1) {
            out->c_function = "json_get_bool"; return true;
        }
        if (strcmp(method_name, "free") == 0 && arg_count == 0) {
            out->c_function = "json_free"; return true;
        }
        // The writers. Capital-J `Json_set_*` / `Json_stringify` are the wrappers
        // that take a `WynJson*`, which is what TYPE_JSON lowers to - the same
        // functions the WORKING namespace spelling (`Json.set_int(j, ..)`) already
        // used, so the two spellings now agree by construction.
        //
        // has/keys are deliberately NOT here: `Json_has` and `Json_keys` take a
        // `long long` index into json_nodes[], the OTHER half of Json's documented
        // two-representation split, so wiring them to a WynJson* receiver would
        // trade a missing call for a type confusion. They stay namespace-only.
        if (strcmp(method_name, "set_string") == 0 && arg_count == 2) {
            out->c_function = "Json_set_string"; return true;
        }
        if (strcmp(method_name, "set_int") == 0 && arg_count == 2) {
            out->c_function = "Json_set_int"; return true;
        }
        if (strcmp(method_name, "set_bool") == 0 && arg_count == 2) {
            out->c_function = "Json_set_bool"; return true;
        }
        if (strcmp(method_name, "stringify") == 0 && arg_count == 0) {
            out->c_function = "Json_stringify"; return true;
        }
        return false;
    }

    if (strcmp(receiver_type, "int") == 0) {
        // Integer methods
        if (strcmp(method_name, "to_string") == 0 && arg_count == 0) {
            out->c_function = "int_to_string"; return true;
        }
        // Identity, so bool-in-int-clothing results (map.contains() and other
        // comparisons are typed int) accept .to_int() at build like the checker
        // does at check. Before this, `map.contains(k).to_int()` passed `wyn
        // check` and then died in codegen ("Unknown method 'to_int' for type
        // 'int'").
        if (strcmp(method_name, "to_int") == 0 && arg_count == 0) {
            out->c_function = "int_to_int"; return true;
        }
        if (strcmp(method_name, "to_float") == 0 && arg_count == 0) {
            out->c_function = "int_to_float"; return true;
        }
        if (strcmp(method_name, "abs") == 0 && arg_count == 0) {
            out->c_function = "int_abs"; return true;
        }
        if (strcmp(method_name, "pow") == 0 && arg_count == 1) {
            out->c_function = "int_pow"; return true;
        }
        if (strcmp(method_name, "min") == 0 && arg_count == 1) {
            out->c_function = "int_min"; return true;
        }
        if (strcmp(method_name, "max") == 0 && arg_count == 1) {
            out->c_function = "int_max"; return true;
        }
        if (strcmp(method_name, "clamp") == 0 && arg_count == 2) {
            out->c_function = "int_clamp"; return true;
        }
        if (strcmp(method_name, "times") == 0 && arg_count == 1) {
            out->c_function = "int_times"; return true;
        }
        if (strcmp(method_name, "is_even") == 0 && arg_count == 0) {
            out->c_function = "int_is_even"; return true;
        }
        if (strcmp(method_name, "is_odd") == 0 && arg_count == 0) {
            out->c_function = "int_is_odd"; return true;
        }
        if (strcmp(method_name, "is_positive") == 0 && arg_count == 0) {
            out->c_function = "int_is_positive"; return true;
        }
        if (strcmp(method_name, "is_negative") == 0 && arg_count == 0) {
            out->c_function = "int_is_negative"; return true;
        }
        if (strcmp(method_name, "is_zero") == 0 && arg_count == 0) {
            out->c_function = "int_is_zero"; return true;
        }
        if (strcmp(method_name, "sign") == 0 && arg_count == 0) {
            out->c_function = "int_sign"; return true;
        }
        if (strcmp(method_name, "to_binary") == 0 && arg_count == 0) {
            out->c_function = "int_to_binary"; return true;
        }
        if (strcmp(method_name, "to_hex") == 0 && arg_count == 0) {
            out->c_function = "int_to_hex"; return true;
        }
        return false;
    }
    
    if (strcmp(receiver_type, "bool") == 0) {
        // Bool methods
        if (strcmp(method_name, "to_string") == 0 && arg_count == 0) {
            out->c_function = "bool_to_string"; return true;
        }
        if (strcmp(method_name, "to_int") == 0 && arg_count == 0) {
            out->c_function = "bool_to_int"; return true;
        }
        if (strcmp(method_name, "not") == 0 && arg_count == 0) {
            out->c_function = "bool_not"; return true;
        }
        if (strcmp(method_name, "and") == 0 && arg_count == 1) {
            out->c_function = "bool_and"; return true;
        }
        if (strcmp(method_name, "or") == 0 && arg_count == 1) {
            out->c_function = "bool_or"; return true;
        }
        if (strcmp(method_name, "xor") == 0 && arg_count == 1) {
            out->c_function = "bool_xor"; return true;
        }
        return false;
    }
    
    if (strcmp(receiver_type, "char") == 0) {
        // Char methods
        if (strcmp(method_name, "to_string") == 0 && arg_count == 0) {
            out->c_function = "char_to_string"; return true;
        }
        if (strcmp(method_name, "to_int") == 0 && arg_count == 0) {
            out->c_function = "char_to_int"; return true;
        }
        if (strcmp(method_name, "is_alpha") == 0 && arg_count == 0) {
            out->c_function = "char_is_alpha"; return true;
        }
        if (strcmp(method_name, "is_numeric") == 0 && arg_count == 0) {
            out->c_function = "char_is_numeric"; return true;
        }
        if (strcmp(method_name, "is_alphanumeric") == 0 && arg_count == 0) {
            out->c_function = "char_is_alphanumeric"; return true;
        }
        if (strcmp(method_name, "is_whitespace") == 0 && arg_count == 0) {
            out->c_function = "char_is_whitespace"; return true;
        }
        if (strcmp(method_name, "is_uppercase") == 0 && arg_count == 0) {
            out->c_function = "char_is_uppercase"; return true;
        }
        if (strcmp(method_name, "is_lowercase") == 0 && arg_count == 0) {
            out->c_function = "char_is_lowercase"; return true;
        }
        if (strcmp(method_name, "to_upper") == 0 && arg_count == 0) {
            out->c_function = "char_to_upper"; return true;
        }
        if (strcmp(method_name, "to_lower") == 0 && arg_count == 0) {
            out->c_function = "char_to_lower"; return true;
        }
        return false;
    }
    
    if (strcmp(receiver_type, "float") == 0) {
        // Float methods
        if (strcmp(method_name, "to_string") == 0 && arg_count == 0) {
            out->c_function = "float_to_string"; return true;
        }
        if (strcmp(method_name, "to_int") == 0 && arg_count == 0) {
            out->c_function = "float_to_int"; return true;
        }
        if (strcmp(method_name, "round") == 0 && arg_count == 0) {
            out->c_function = "float_round"; return true;
        }
        if (strcmp(method_name, "round_to") == 0 && arg_count == 1) {
            out->c_function = "float_round_to"; return true;
        }
        if (strcmp(method_name, "floor") == 0 && arg_count == 0) {
            out->c_function = "float_floor"; return true;
        }
        if (strcmp(method_name, "ceil") == 0 && arg_count == 0) {
            out->c_function = "float_ceil"; return true;
        }
        if (strcmp(method_name, "abs") == 0 && arg_count == 0) {
            out->c_function = "float_abs"; return true;
        }
        if (strcmp(method_name, "pow") == 0 && arg_count == 1) {
            out->c_function = "float_pow"; return true;
        }
        if (strcmp(method_name, "sqrt") == 0 && arg_count == 0) {
            out->c_function = "float_sqrt"; return true;
        }
        if (strcmp(method_name, "min") == 0 && arg_count == 1) {
            out->c_function = "float_min"; return true;
        }
        if (strcmp(method_name, "max") == 0 && arg_count == 1) {
            out->c_function = "float_max"; return true;
        }
        if (strcmp(method_name, "clamp") == 0 && arg_count == 2) {
            out->c_function = "float_clamp"; return true;
        }
        if (strcmp(method_name, "is_nan") == 0 && arg_count == 0) {
            out->c_function = "float_is_nan"; return true;
        }
        if (strcmp(method_name, "is_infinite") == 0 && arg_count == 0) {
            out->c_function = "float_is_infinite"; return true;
        }
        if (strcmp(method_name, "is_finite") == 0 && arg_count == 0) {
            out->c_function = "float_is_finite"; return true;
        }
        if (strcmp(method_name, "is_positive") == 0 && arg_count == 0) {
            out->c_function = "float_is_positive"; return true;
        }
        if (strcmp(method_name, "is_negative") == 0 && arg_count == 0) {
            out->c_function = "float_is_negative"; return true;
        }
        if (strcmp(method_name, "sign") == 0 && arg_count == 0) {
            out->c_function = "float_sign"; return true;
        }
        if (strcmp(method_name, "sin") == 0 && arg_count == 0) {
            out->c_function = "float_sin"; return true;
        }
        if (strcmp(method_name, "cos") == 0 && arg_count == 0) {
            out->c_function = "float_cos"; return true;
        }
        if (strcmp(method_name, "tan") == 0 && arg_count == 0) {
            out->c_function = "float_tan"; return true;
        }
        if (strcmp(method_name, "asin") == 0 && arg_count == 0) {
            out->c_function = "float_asin"; return true;
        }
        if (strcmp(method_name, "acos") == 0 && arg_count == 0) {
            out->c_function = "float_acos"; return true;
        }
        if (strcmp(method_name, "atan") == 0 && arg_count == 0) {
            out->c_function = "float_atan"; return true;
        }
        if (strcmp(method_name, "log") == 0 && arg_count == 0) {
            out->c_function = "float_log"; return true;
        }
        if (strcmp(method_name, "log10") == 0 && arg_count == 0) {
            out->c_function = "float_log10"; return true;
        }
        if (strcmp(method_name, "log2") == 0 && arg_count == 0) {
            out->c_function = "float_log2"; return true;
        }
        if (strcmp(method_name, "exp") == 0 && arg_count == 0) {
            out->c_function = "float_exp"; return true;
        }
        return false;
    }
    
    if (strcmp(receiver_type, "array") == 0) {
        // Array methods
        if (strcmp(method_name, "len") == 0 && arg_count == 0) {
            out->c_function = "array_len"; return true;
        }
        if (strcmp(method_name, "is_empty") == 0 && arg_count == 0) {
            out->c_function = "array_is_empty"; return true;
        }
        if (strcmp(method_name, "count") == 0 && arg_count == 1) {
            out->c_function = "array_count"; return true;
        }
        if (strcmp(method_name, "contains") == 0 && arg_count == 1) {
            out->c_function = "array_contains"; return true;
        }
        if (strcmp(method_name, "push") == 0 && arg_count == 1) {
            out->c_function = "array_push"; 
            out->pass_by_ref = true;
            return true;
        }
        if (strcmp(method_name, "pop") == 0 && arg_count == 0) {
            out->c_function = "array_pop";
            out->pass_by_ref = true;
            return true;
        }
        if (strcmp(method_name, "get") == 0 && arg_count == 1) {
            out->c_function = "array_get"; return true;
        }
        if (strcmp(method_name, "index_of") == 0 && arg_count == 1) {
            out->c_function = "array_index_of"; return true;
        }
        if (strcmp(method_name, "reverse") == 0 && arg_count == 0) {
            out->c_function = "array_reverse_copy";
            out->pass_by_ref = false;
            return true;
        }
        if (strcmp(method_name, "sort") == 0 && arg_count == 0) {
            out->c_function = "array_sort_copy";
            out->pass_by_ref = false;
            return true;
        }
        if (strcmp(method_name, "first") == 0 && arg_count == 0) {
            out->c_function = "array_first"; return true;
        }
        if (strcmp(method_name, "last") == 0 && arg_count == 0) {
            out->c_function = "array_last"; return true;
        }
        if (strcmp(method_name, "take") == 0 && arg_count == 1) {
            out->c_function = "array_take"; return true;
        }
        if (strcmp(method_name, "skip") == 0 && arg_count == 1) {
            out->c_function = "array_skip"; return true;
        }
        if (strcmp(method_name, "slice") == 0 && arg_count == 2) {
            out->c_function = "wyn_array_slice_range"; return true;
        }
        if (strcmp(method_name, "slice") == 0 && arg_count == 1) {
            out->c_function = "wyn_array_slice_from"; return true;
        }
        if (strcmp(method_name, "join") == 0 && arg_count == 1) {
            out->c_function = "array_join_str"; return true;
        }
        if (strcmp(method_name, "concat") == 0 && arg_count == 1) {
            out->c_function = "array_concat"; return true;
        }
        if (strcmp(method_name, "clear") == 0 && arg_count == 0) {
            out->c_function = "array_clear";
            out->pass_by_ref = true;
            return true;
        }
        if (strcmp(method_name, "min") == 0 && arg_count == 0) {
            out->c_function = "array_min"; return true;
        }
        if (strcmp(method_name, "max") == 0 && arg_count == 0) {
            out->c_function = "array_max"; return true;
        }
        if (strcmp(method_name, "sum") == 0 && arg_count == 0) {
            out->c_function = "array_sum"; return true;
        }
        if (strcmp(method_name, "average") == 0 && arg_count == 0) {
            out->c_function = "array_average"; return true;
        }
        if (strcmp(method_name, "each") == 0 && arg_count == 1) {
            out->c_function = "array_each"; return true;
        }
        if (strcmp(method_name, "every") == 0 && arg_count == 1) {
            out->c_function = "array_every"; return true;
        }
        if (strcmp(method_name, "find") == 0 && arg_count == 1) {
            out->c_function = "array_find_fn"; return true;
        }
        if (strcmp(method_name, "flat_map") == 0 && arg_count == 1) {
            out->c_function = "array_flat_map"; return true;
        }
        if (strcmp(method_name, "remove") == 0 && arg_count == 1) {
            out->c_function = "array_remove_value";
            out->pass_by_ref = true;
            return true;
        }
        if (strcmp(method_name, "remove_at") == 0 && arg_count == 1) {
            out->c_function = "array_remove_at";
            out->pass_by_ref = true;
            return true;
        }
        if (strcmp(method_name, "insert") == 0 && arg_count == 2) {
            out->c_function = "array_insert";
            out->pass_by_ref = true;
            return true;
        }
        if (strcmp(method_name, "map") == 0 && arg_count == 1) {
            out->c_function = "wyn_array_map"; return true;
        }
        if (strcmp(method_name, "filter") == 0 && arg_count == 1) {
            out->c_function = "wyn_array_filter"; return true;
        }
        if (strcmp(method_name, "reduce") == 0 && arg_count == 2) {
            out->c_function = "wyn_array_reduce"; return true;
        }
        return false;
    }
    
    if (strcmp(receiver_type, "arena") == 0) {
        // Arena methods
        if (strcmp(method_name, "alloc") == 0 && arg_count == 1) {
            out->c_function = "wyn_arena_alloc_int"; return true;
        }
        if (strcmp(method_name, "clear") == 0 && arg_count == 0) {
            out->c_function = "wyn_arena_clear"; return true;
        }
        if (strcmp(method_name, "free") == 0 && arg_count == 0) {
            out->c_function = "wyn_arena_free"; return true;
        }
        return false;
    }
    
    if (strcmp(receiver_type, "map") == 0) {
        // HashMap methods
        if (strcmp(method_name, "insert") == 0 && arg_count == 2) {
            out->c_function = "hashmap_insert_int"; return true;
        }
        if (strcmp(method_name, "set") == 0 && arg_count == 2) {
            out->c_function = "hashmap_insert_int"; return true;
        }
        if (strcmp(method_name, "has") == 0 && arg_count == 1) {
            out->c_function = "hashmap_has"; return true;
        }
        if (strcmp(method_name, "contains") == 0 && arg_count == 1) {
            out->c_function = "hashmap_has"; return true;
        }
        if (strcmp(method_name, "get") == 0 && arg_count == 1) {
            out->c_function = "hashmap_get_int"; return true;
        }
        if (strcmp(method_name, "remove") == 0 && arg_count == 1) {
            out->c_function = "hashmap_remove"; return true;
        }
        if (strcmp(method_name, "len") == 0 && arg_count == 0) {
            out->c_function = "hashmap_len"; return true;
        }
        if (strcmp(method_name, "is_empty") == 0 && arg_count == 0) {
            out->c_function = "wyn_hashmap_is_empty"; return true;
        }
        if (strcmp(method_name, "clear") == 0 && arg_count == 0) {
            out->c_function = "wyn_hashmap_clear"; return true;
        }
        if (strcmp(method_name, "free") == 0 && arg_count == 0) {
            out->c_function = "hashmap_free"; return true;
        }
        return false;
    }
    
    if (strcmp(receiver_type, "set") == 0) {
        // HashSet methods
        if (strcmp(method_name, "add") == 0 && arg_count == 1) {
            out->c_function = "hashset_add"; return true;
        }
        if (strcmp(method_name, "insert") == 0 && arg_count == 1) {
            out->c_function = "hashset_add"; return true;
        }
        if (strcmp(method_name, "add_int") == 0 && arg_count == 1) {
            out->c_function = "wyn_hashset_add_int"; return true;
        }
        if (strcmp(method_name, "contains") == 0 && arg_count == 1) {
            out->c_function = "hashset_contains"; return true;
        }
        if (strcmp(method_name, "contains_int") == 0 && arg_count == 1) {
            out->c_function = "wyn_hashset_contains_int"; return true;
        }
        if (strcmp(method_name, "remove") == 0 && arg_count == 1) {
            out->c_function = "hashset_remove"; return true;
        }
        if (strcmp(method_name, "len") == 0 && arg_count == 0) {
            out->c_function = "wyn_hashset_len"; return true;
        }
        if (strcmp(method_name, "is_empty") == 0 && arg_count == 0) {
            out->c_function = "wyn_hashset_is_empty"; return true;
        }
        if (strcmp(method_name, "clear") == 0 && arg_count == 0) {
            out->c_function = "wyn_hashset_clear"; return true;
        }
        if (strcmp(method_name, "union") == 0 && arg_count == 1) {
            out->c_function = "set_union"; return true;
        }
        if (strcmp(method_name, "intersection") == 0 && arg_count == 1) {
            out->c_function = "set_intersection"; return true;
        }
        if (strcmp(method_name, "difference") == 0 && arg_count == 1) {
            out->c_function = "set_difference"; return true;
        }
        if (strcmp(method_name, "is_subset") == 0 && arg_count == 1) {
            out->c_function = "set_is_subset"; return true;
        }
        if (strcmp(method_name, "is_superset") == 0 && arg_count == 1) {
            out->c_function = "set_is_superset"; return true;
        }
        if (strcmp(method_name, "is_disjoint") == 0 && arg_count == 1) {
            out->c_function = "set_is_disjoint"; return true;
        }
        return false;
    }
    
    if (strcmp(receiver_type, "option") == 0) {
        // Option methods
        if (strcmp(method_name, "is_some") == 0 && arg_count == 0) {
            out->c_function = "Option_is_some"; return true;
        }
        if (strcmp(method_name, "is_none") == 0 && arg_count == 0) {
            out->c_function = "Option_is_none"; return true;
        }
        if (strcmp(method_name, "unwrap") == 0 && arg_count == 0) {
            out->c_function = "Option_unwrap"; return true;
        }
        if (strcmp(method_name, "unwrap_or") == 0 && arg_count == 1) {
            out->c_function = "Option_unwrap_or"; return true;
        }
        if (strcmp(method_name, "expect") == 0 && arg_count == 1) {
            out->c_function = "wyn_optional_expect"; return true;
        }
        if (strcmp(method_name, "or_else") == 0 && arg_count == 1) {
            out->c_function = "wyn_optional_or_else"; return true;
        }
        if (strcmp(method_name, "map") == 0 && arg_count == 1) {
            out->c_function = "wyn_optional_map"; return true;
        }
        if (strcmp(method_name, "and_then") == 0 && arg_count == 1) {
            out->c_function = "wyn_optional_and_then"; return true;
        }
        if (strcmp(method_name, "filter") == 0 && arg_count == 1) {
            out->c_function = "wyn_optional_filter"; return true;
        }
        return false;
    }
    
    if (strcmp(receiver_type, "result") == 0) {
        // Result methods
        if (strcmp(method_name, "is_ok") == 0 && arg_count == 0) {
            out->c_function = "Result_is_ok"; return true;
        }
        if (strcmp(method_name, "is_err") == 0 && arg_count == 0) {
            out->c_function = "Result_is_err"; return true;
        }
        if (strcmp(method_name, "unwrap") == 0 && arg_count == 0) {
            out->c_function = "Result_unwrap"; return true;
        }
        if (strcmp(method_name, "unwrap_or") == 0 && arg_count == 1) {
            out->c_function = "Result_unwrap_or"; return true;
        }
        if (strcmp(method_name, "expect") == 0 && arg_count == 1) {
            out->c_function = "wyn_result_expect"; return true;
        }
        if (strcmp(method_name, "map_err") == 0 && arg_count == 1) {
            out->c_function = "wyn_result_map_err"; return true;
        }
        if (strcmp(method_name, "or_else") == 0 && arg_count == 1) {
            out->c_function = "wyn_result_or_else"; return true;
        }
        if (strcmp(method_name, "map") == 0 && arg_count == 1) {
            out->c_function = "wyn_result_map"; return true;
        }
        if (strcmp(method_name, "and_then") == 0 && arg_count == 1) {
            out->c_function = "wyn_result_and_then"; return true;
        }
        return false;
    }
    
    // JSON object methods
    if (strcmp(receiver_type, "json") == 0 || strcmp(receiver_type, "WynJson") == 0) {
        if (strcmp(method_name, "get_string") == 0 && arg_count == 1) {
            out->c_function = "json_get_string"; return true;
        }
        if (strcmp(method_name, "get_int") == 0 && arg_count == 1) {
            out->c_function = "json_get_int"; return true;
        }
        if (strcmp(method_name, "free") == 0 && arg_count == 0) {
            out->c_function = "json_free"; return true;
        }
        return false;
    }
    
    return false;  // Method not found
}

// Lookup return type for module functions (e.g., "Crypto_sha256" -> "string")
const char* lookup_module_fn_return_type(const char* fn_name) {
    // These match the checker's return type registry
    struct { const char* name; const char* ret; } fns[] = {
        {"Crypto_sha256", "string"}, {"Crypto_md5", "string"},
        {"Crypto_hmac_sha256", "string"}, {"Crypto_hmac_sha256_hex", "string"},
        {"Crypto_random_bytes", "string"},
        {"Encoding_base64_encode", "string"}, {"Encoding_base64_decode", "string"},
        {"Encoding_hex_encode", "string"}, {"Encoding_hex_decode", "string"},
        {"Base64_encode", "string"}, {"Base64_decode", "string"},
        {"Json_stringify", "string"}, {"Json_to_pretty_string", "string"},
        {"Json_get", "string"}, {"Json_keys", "array"},
        {"Os_platform", "string"}, {"Os_arch", "string"},
        {"Os_hostname", "string"}, {"Os_home_dir", "string"}, {"Os_temp_dir", "string"},
        {"Uuid_generate", "string"}, {"Uuid_v4", "string"}, {"Process_exec_capture", "string"},
        {"Csv_get", "string"}, {"Csv_get_field", "string"}, {"Csv_header", "string"},
        {"Toml_get", "string"}, {"Toml_parse", "int"}, {"Toml_parse_file", "int"},
        {"Bcrypt_hash", "string"}, {"Bcrypt_verify", "bool"},
        {"Path_basename", "string"}, {"Path_dirname", "string"}, {"Path_extension", "string"}, {"Path_join", "string"},
        {"DateTime_to_iso", "string"}, {"DateTime_format_duration", "string"},
        {"Regex_replace", "string"}, {"Regex_find_all", "string"},
        {"Regex_match", "bool"},
        {"File_read", "string"}, {"File_temp_file", "string"}, {"File_read_line", "string"},
        {"System_exec", "string"}, {"System_env", "string"},
        {"System_shell_escape", "string"},
        {"System_arg", "string"},
        {"Net_resolve", "string"}, {"Url_encode", "string"}, {"Url_decode", "string"},
        {"StringBuilder_to_string", "string"},
        {"Template_render", "string"},
        {"Template_render_string", "string"},
        {"Args_get", "string"},
        {"Args_has", "bool"},
        {"Args_positional", "array"},
        {"Random_string", "string"}, {"Random_hex", "string"}, {"Random_uuid", "string"},
        {"Random_bool", "bool"}, {"Random_choice_str", "string"},
        {"Web_render", "string"},
        {NULL, NULL}
    };
    for (int i = 0; fns[i].name; i++) {
        if (strcmp(fns[i].name, fn_name) == 0) return fns[i].ret;
    }
    return NULL;
}

// ===========================================================================
// Builtin stdlib namespaces: one lowering, and the check-time "does it exist?"
// ===========================================================================
//
// THE PROBLEM THIS SOLVES
//
// `Time.no_such_method_xyz()` used to pass `wyn check` on all 31 namespaces and
// then fail in clang on a symbol the programmer never wrote. The checker could not
// reject it because its namespace return-type tables are deliberately partial - 37
// of the 217 distinct `Namespace.method` calls in this repo's own .wyn corpus are
// absent from them, so "not in a table" has never meant "does not exist".
//
// What DOES decide is the C symbol the call lowers to: codegen emits it, and the C
// compiler resolves it against the runtime headers. So the lowering is the
// authority, and it lives here - once. codegen_expr.c used to carry it as a
// 26-branch if-chain over namespace names; it now calls wyn_namespace_c_symbol(),
// because a second copy of this mapping is exactly how `HashMap.set_int` came to
// pass the checker and fail the C compile (the runtime spells it
// hashmap_insert_int).
//
// The existence answer is TRI-STATE on purpose. If the runtime headers cannot be
// read (an unusual install), or if the index knows no symbol at all under a
// namespace's prefix, the checker stays permissive - the same behaviour as before
// this existed. Being unable to prove a call wrong must never mean rejecting it.

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

extern bool is_builtin_module(const char* name);
extern const char* builtin_module_name_at(int index);
extern const char* wyn_installation_root(void);   // main.c

// Namespaces whose C symbols are spelled with a LOWERCASE prefix. Everything else
// uses the namespace's own name (`File.read_all` -> `File_read_all`), which is also
// what the module fall-through in codegen emits.
static const struct { const char* ns; const char* prefix; } wyn_ns_prefixes[] = {
    {"Regex",   "regex_"},
    {"HashMap", "hashmap_"},
    {"HashSet", "hashset_"},
    {"Random",  "random_"},
    {NULL, NULL}
};

// Methods whose C symbol is not <prefix><method>. Each one is a rename in the
// runtime, and each one was a real bug before it was listed: the blanket mangling
// emitted a symbol that did not exist.
static const struct { const char* ns; const char* method; const char* sym; } wyn_ns_renames[] = {
    // Http's simple string API is lowercase; its server API is not.
    {"Http", "get",        "http_get"},
    {"Http", "post",       "http_post"},
    {"Http", "put",        "http_put"},
    {"Http", "delete",     "http_delete"},
    {"Http", "set_header", "http_set_header"},
    // HashMap: the runtime spells the setters hashmap_insert_*, and `get` defaults
    // to the string flavour.
    {"HashMap", "get",        "hashmap_get_string"},
    {"HashMap", "set",        "hashmap_set"},
    {"HashMap", "has",        "hashmap_has"},
    {"HashMap", "set_int",    "hashmap_insert_int"},
    {"HashMap", "set_string", "hashmap_insert_string"},
    {"HashMap", "set_float",  "hashmap_insert_float"},
    {"HashMap", "set_bool",   "hashmap_insert_bool"},
    // Task.try_recv returns int? in Wyn, so it lowers to the Option-returning shim
    // built on the pointer out-param form that Wyn cannot express.
    {"Task", "try_recv", "Task_try_recv_opt"},
    // String.char(65) -> "A"
    {"String", "char", "String_char_from_int"},
    {NULL, NULL, NULL}
};

static const char* wyn_ns_prefix_for(const char* ns) {
    for (int i = 0; wyn_ns_prefixes[i].ns; i++)
        if (strcmp(wyn_ns_prefixes[i].ns, ns) == 0) return wyn_ns_prefixes[i].prefix;
    return NULL;   // caller uses "<ns>_"
}

int wyn_namespace_c_symbol(const char* ns, const char* method, char* out, size_t out_sz) {
    if (!ns || !method || !out || out_sz == 0) return 0;
    if (!is_builtin_module(ns)) return 0;
    for (int i = 0; wyn_ns_renames[i].ns; i++) {
        if (strcmp(wyn_ns_renames[i].ns, ns) == 0 &&
            strcmp(wyn_ns_renames[i].method, method) == 0) {
            snprintf(out, out_sz, "%s", wyn_ns_renames[i].sym);
            return 1;
        }
    }
    const char* pfx = wyn_ns_prefix_for(ns);
    if (pfx) snprintf(out, out_sz, "%s%s", pfx, method);
    else     snprintf(out, out_sz, "%s_%s", ns, method);
    return 1;
}

// --- the runtime declaration index ----------------------------------------
// Every `identifier(` in the translation unit a compiled program forms:
// src/wyn_runtime.h plus the project headers it includes. Built at most once, and
// only when a namespace call has already failed every return-type lookup - so an
// ordinary compile never reads a byte of it.

static char** wyn_rt_syms = NULL;
static int wyn_rt_sym_count = 0;
static int wyn_rt_sym_cap = 0;
static int wyn_rt_index_state = 0;   // 0 unloaded, 1 loaded, -1 unavailable

static void wyn_rt_index_add(const char* name, size_t len) {
    if (len == 0 || len > 190) return;
    if (wyn_rt_sym_count == wyn_rt_sym_cap) {
        int ncap = wyn_rt_sym_cap ? wyn_rt_sym_cap * 2 : 512;
        char** grown = (char**)realloc(wyn_rt_syms, (size_t)ncap * sizeof(char*));
        if (!grown) return;
        wyn_rt_syms = grown; wyn_rt_sym_cap = ncap;
    }
    char* copy = (char*)malloc(len + 1);
    if (!copy) return;
    memcpy(copy, name, len); copy[len] = '\0';
    wyn_rt_syms[wyn_rt_sym_count++] = copy;
}

// Scan one header. `collect_includes` is set for the top-level runtime header only:
// the compiled program sees its `#include "x.h"` files too, and four namespaces
// (HashMap, HashSet, Json, Gui among them) are declared exclusively in those.
static void wyn_rt_index_file(const char* root, const char* rel, int collect_includes,
                              char includes[][64], int* ninc) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/src/%s", root, rel);
    FILE* f = fopen(path, "r");
    if (!f) return;
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        if (collect_includes && *ninc < 64) {
            const char* inc = strstr(line, "#include \"");
            if (inc) {
                inc += 10;
                const char* endq = strchr(inc, '"');
                if (endq && endq - inc > 0 && endq - inc < 63) {
                    size_t n = (size_t)(endq - inc);
                    memcpy(includes[*ninc], inc, n);
                    includes[*ninc][n] = '\0';
                    (*ninc)++;
                }
            }
        }
        for (const char* p = line; *p; ) {
            if (!(*p == '_' || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))) { p++; continue; }
            const char* start = p;
            while (*p == '_' || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                   (*p >= '0' && *p <= '9')) p++;
            const char* q = p;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '(') wyn_rt_index_add(start, (size_t)(p - start));
        }
    }
    fclose(f);
}

static void wyn_rt_index_load(void) {
    if (wyn_rt_index_state != 0) return;
    const char* root = wyn_installation_root();
    if (!root || !root[0]) { wyn_rt_index_state = -1; return; }
    char includes[64][64];
    int ninc = 0;
    wyn_rt_index_file(root, "wyn_runtime.h", 1, includes, &ninc);
    if (wyn_rt_sym_count == 0) { wyn_rt_index_state = -1; return; }
    for (int i = 0; i < ninc; i++)
        wyn_rt_index_file(root, includes[i], 0, includes, &ninc);
    wyn_rt_index_state = 1;
}

// 1 declared, 0 not declared, -1 the index is unavailable (stay permissive).
static int wyn_runtime_declares(const char* sym) {
    wyn_rt_index_load();
    if (wyn_rt_index_state != 1) return -1;
    for (int i = 0; i < wyn_rt_sym_count; i++)
        if (strcmp(wyn_rt_syms[i], sym) == 0) return 1;
    return 0;
}

// How many symbols the index holds under this namespace's prefix. Zero means the
// index cannot speak for the namespace at all (a user module named `math` shadowing
// the builtin list, a header this build does not ship), and the checker must then
// stay permissive rather than reject every call to it.
static int wyn_ns_declared_count(const char* ns) {
    char pfx[128];
    const char* p = wyn_ns_prefix_for(ns);
    if (p) snprintf(pfx, sizeof(pfx), "%s", p);
    else   snprintf(pfx, sizeof(pfx), "%s_", ns);
    size_t pl = strlen(pfx);
    int n = 0;
    for (int i = 0; i < wyn_rt_sym_count; i++)
        if (strncmp(wyn_rt_syms[i], pfx, pl) == 0 && wyn_rt_syms[i][pl]) n++;
    return n;
}

int wyn_namespace_method_unknown(const char* ns, const char* method) {
    char sym[320];
    if (!wyn_namespace_c_symbol(ns, method, sym, sizeof(sym))) return 0;
    if (wyn_runtime_declares(sym) != 0) return 0;      // declared, or index unavailable
    if (wyn_ns_declared_count(ns) == 0) return 0;      // index knows nothing here
    return 1;
}

// Does the runtime header this compiler ships DECLARE the C symbol this namespace
// call lowers to?  1 yes, 0 no, -1 the declaration index is unavailable.
//
// Same three primitives as wyn_namespace_method_unknown() above -
// wyn_namespace_c_symbol() for the symbol, the shared index for the answer - so
// there is no second lookup to drift out of step with the check-time rule.
//
// TRI-STATE, and the third state matters. It exists so a caller can tell the two
// reasons a `call to undeclared function 'Time_now_millis'` can reach a user apart:
//   0 -> Wyn genuinely does not have that function; the user misspelled something.
//   1 -> Wyn HAS it and this compiler's --release header forgot to declare it,
//        which is a compiler bug and must not be reported as a typo.
//  -1 -> we cannot tell (unusual install, headers unreadable): say nothing new.
// Fills sym_out with the C symbol when given, so the caller can name it.
int wyn_namespace_method_declared(const char* ns, const char* method,
                                  char* sym_out, size_t sym_sz) {
    char sym[320];
    if (!wyn_namespace_c_symbol(ns, method, sym, sizeof(sym))) return -1;
    if (sym_out && sym_sz) snprintf(sym_out, sym_sz, "%s", sym);
    return wyn_runtime_declares(sym);
}

// "Did you mean" for a rejected namespace method, spelled as the user would type
// it (`DateTime.millis()`). Returns 1 when it filled `out`.
//
// The right name in the WRONG namespace is tried first, because it is the stronger
// signal and the likelier typo: `Time.millis()` is a real mistake with a real
// answer (DateTime.millis), and an exact method-name match elsewhere is not a
// guess. Only then a near miss inside the namespace the user named
// (`File.read_al` -> `File.read_all`).
int wyn_suggest_namespace_method(const char* ns, const char* method, char* out, size_t out_sz) {
    if (!ns || !method || !out || out_sz == 0) return 0;
    wyn_rt_index_load();
    if (wyn_rt_index_state != 1) return 0;

    for (int i = 0; ; i++) {
        const char* other = builtin_module_name_at(i);
        if (!other) break;
        if (strcmp(other, ns) == 0) continue;
        char sym[320];
        if (!wyn_namespace_c_symbol(other, method, sym, sizeof(sym))) continue;
        if (wyn_runtime_declares(sym) == 1) {
            snprintf(out, out_sz, "%s.%s()", other, method);
            return 1;
        }
    }

    char pfx[128];
    const char* p = wyn_ns_prefix_for(ns);
    if (p) snprintf(pfx, sizeof(pfx), "%s", p);
    else   snprintf(pfx, sizeof(pfx), "%s_", ns);
    size_t pl = strlen(pfx);
    const char* best = NULL;
    int best_dist = WYN_NAME_FAR;
    for (int i = 0; i < wyn_rt_sym_count; i++) {
        if (strncmp(wyn_rt_syms[i], pfx, pl) != 0 || !wyn_rt_syms[i][pl]) continue;
        const char* cand = wyn_rt_syms[i] + pl;
        int d = wyn_name_distance(method, cand);
        if (d > 0 && d < best_dist) { best_dist = d; best = cand; }
    }
    if (best) { snprintf(out, out_sz, "%s.%s()", ns, best); return 1; }
    return 0;
}
