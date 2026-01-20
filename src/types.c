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
    {"string", "trim_left", "string", 0},
    {"string", "trim_right", "string", 0},
    {"string", "split", "array", 1},     // Returns array of strings
    {"string", "capitalize", "string", 0},
    {"string", "title", "string", 0},
    {"string", "reverse", "string", 0},
    {"string", "to_bytes", "array", 0},  // Returns Vec<int>
    {"string", "chars", "array", 0},     // Returns Vec<string>
    {"string", "len", "int", 0},
    {"string", "is_empty", "bool", 0},
    {"string", "contains", "bool", 1},
    {"string", "starts_with", "bool", 1},
    {"string", "ends_with", "bool", 1},
    {"string", "index_of", "int", 1},    // Returns -1 if not found
    {"string", "replace", "string", 2},
    {"string", "split", "array", 1},     // Returns Vec<string>
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
    
    // Int methods
    {"int", "to_string", "string", 0},
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
    {"array", "first", "optional", 0},   // Returns Option<T>
    {"array", "last", "optional", 0},    // Returns Option<T>
    {"array", "take", "array", 1},     // Returns new array with first n elements
    {"array", "skip", "array", 1},     // Returns new array skipping first n elements
    {"array", "slice", "array", 2},    // Returns new array from start to end
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
    
    // HashMap methods
    {"map", "insert", "void", 2},
    {"map", "get", "int", 1},          // Returns value (type depends on map)
    {"map", "remove", "void", 1},
    {"map", "contains", "bool", 1},
    {"map", "len", "int", 0},
    {"map", "is_empty", "bool", 0},
    {"map", "keys", "array", 0},
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
        if (strcmp(method_name, "chars") == 0 && arg_count == 0) {
            out->c_function = "string_chars"; return true;
        }
        if (strcmp(method_name, "to_bytes") == 0 && arg_count == 0) {
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
        return false;
    }
    
    if (strcmp(receiver_type, "int") == 0) {
        // Integer methods
        if (strcmp(method_name, "to_string") == 0 && arg_count == 0) {
            out->c_function = "int_to_string"; return true;
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
        if (strcmp(method_name, "sin") == 0 && arg_count == 0) {
            out->c_function = "float_sin"; return true;
        }
        if (strcmp(method_name, "cos") == 0 && arg_count == 0) {
            out->c_function = "float_cos"; return true;
        }
        if (strcmp(method_name, "tan") == 0 && arg_count == 0) {
            out->c_function = "float_tan"; return true;
        }
        if (strcmp(method_name, "log") == 0 && arg_count == 0) {
            out->c_function = "float_log"; return true;
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
            out->c_function = "array_reverse";
            out->pass_by_ref = true;
            return true;
        }
        if (strcmp(method_name, "sort") == 0 && arg_count == 0) {
            out->c_function = "array_sort";
            out->pass_by_ref = true;
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
            out->c_function = "array_slice"; return true;
        }
        if (strcmp(method_name, "concat") == 0 && arg_count == 1) {
            out->c_function = "array_concat"; return true;
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
    
    if (strcmp(receiver_type, "map") == 0) {
        // Map methods
        if (strcmp(method_name, "insert") == 0 && arg_count == 2) {
            out->c_function = "map_set";
            return true;
        }
        if (strcmp(method_name, "get") == 0 && arg_count == 1) {
            out->c_function = "map_get"; return true;
        }
        if (strcmp(method_name, "remove") == 0 && arg_count == 1) {
            out->c_function = "map_remove";
            return true;
        }
        if (strcmp(method_name, "contains") == 0 && arg_count == 1) {
            out->c_function = "map_has"; return true;
        }
        if (strcmp(method_name, "len") == 0 && arg_count == 0) {
            out->c_function = "map_len"; return true;
        }
        if (strcmp(method_name, "is_empty") == 0 && arg_count == 0) {
            out->c_function = "map_is_empty"; return true;
        }
        if (strcmp(method_name, "clear") == 0 && arg_count == 0) {
            out->c_function = "map_clear";
            return true;
        }
        if (strcmp(method_name, "get_or_default") == 0 && arg_count == 2) {
            out->c_function = "map_get_or_default"; return true;
        }
        if (strcmp(method_name, "merge") == 0 && arg_count == 1) {
            out->c_function = "map_merge";
            return true;
        }
        return false;
    }
    
    if (strcmp(receiver_type, "set") == 0) {
        // HashSet methods
        if (strcmp(method_name, "insert") == 0 && arg_count == 1) {
            out->c_function = "hashset_add";
            
            return true;
        }
        if (strcmp(method_name, "contains") == 0 && arg_count == 1) {
            out->c_function = "hashset_contains"; return true;
        }
        if (strcmp(method_name, "remove") == 0 && arg_count == 1) {
            out->c_function = "hashset_remove";
            
            return true;
        }
        if (strcmp(method_name, "len") == 0 && arg_count == 0) {
            out->c_function = "set_len"; return true;
        }
        if (strcmp(method_name, "is_empty") == 0 && arg_count == 0) {
            out->c_function = "set_is_empty"; return true;
        }
        if (strcmp(method_name, "clear") == 0 && arg_count == 0) {
            out->c_function = "set_clear";
            return true;
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
            out->c_function = "wyn_optional_is_some"; return true;
        }
        if (strcmp(method_name, "is_none") == 0 && arg_count == 0) {
            out->c_function = "wyn_optional_is_none"; return true;
        }
        if (strcmp(method_name, "unwrap") == 0 && arg_count == 0) {
            out->c_function = "wyn_optional_unwrap"; return true;
        }
        if (strcmp(method_name, "unwrap_or") == 0 && arg_count == 1) {
            out->c_function = "wyn_optional_unwrap_or"; return true;
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
            out->c_function = "wyn_result_is_ok"; return true;
        }
        if (strcmp(method_name, "is_err") == 0 && arg_count == 0) {
            out->c_function = "wyn_result_is_err"; return true;
        }
        if (strcmp(method_name, "unwrap") == 0 && arg_count == 0) {
            out->c_function = "wyn_result_unwrap"; return true;
        }
        if (strcmp(method_name, "unwrap_or") == 0 && arg_count == 1) {
            out->c_function = "wyn_result_unwrap_or"; return true;
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
    
    return false;  // Method not found
}
