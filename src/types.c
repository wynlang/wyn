// Phase 1 Task 1.2: Method Signature Table Implementation
#include "types.h"
#include <string.h>
#include <stddef.h>

// Method signature table - maps (receiver_type, method_name) -> return_type
static const MethodSignature method_signatures[] = {
    // String methods
    {"string", "upper", "string", ""},
    {"string", "lower", "string", ""},
    {"string", "trim", "string", ""},
    {"string", "to_string", "string", ""}, // identity - supported by codegen (generic to_string path)
    {"string", "trim_left", "string", ""},
    {"string", "trim_right", "string", ""},
    {"string", "split", "array", "string"},     // Returns array of strings
    {"string", "charAt", "string", "int"},   // Returns single char as string
    {"string", "capitalize", "string", ""},
    {"string", "title", "string", ""},
    {"string", "reverse", "string", ""},
    {"string", "to_bytes", "array", ""},  // Returns Vec<int>
    {"string", "bytes", "array", ""},
    {"string", "chars", "array", ""},     // Returns Vec<string>
    {"string", "len", "int", ""},
    {"string", "is_empty", "bool", ""},
    {"string", "contains", "bool", "string"},
    {"string", "starts_with", "bool", "string"},
    {"string", "ends_with", "bool", "string"},
    {"string", "index_of", "int", "string"},    // Returns -1 if not found
    {"string", "replace", "string", "string, string"},
    {"string", "slice", "string", "int, int"},
    {"string", "substring", "string", "int, int"},
    {"string", "repeat", "string", "int"},
    {"string", "pad_left", "string", "int, string"},
    {"string", "pad_right", "string", "int, string"},
    {"string", "lines", "array", ""},     // Returns Vec<string>
    {"string", "words", "array", ""},     // Returns Vec<string>
    {"string", "concat", "string", "string"},
    {"string", "replace_all", "string", "string, string"},  // replace_all(old, new)
    {"string", "last_index_of", "int", "string"},   // Returns -1 if not found
    {"string", "is_alpha", "bool", ""},       // Check if all alphabetic
    {"string", "is_digit", "bool", ""},       // Check if all numeric
    {"string", "is_alnum", "bool", ""},       // Check if alphanumeric
    {"string", "is_whitespace", "bool", ""},  // Check if all whitespace
    {"string", "char_at", "string", "int"},      // Get char at index
    {"string", "equals", "bool", "string"},         // String equality
    {"string", "count", "int", "string"},           // Count occurrences
    // is_numeric means "looks like a DECIMAL number, int or float" - so
    // "1.5".is_numeric() is true and that is correct, 1.5 IS a number. It is
    // NOT the predicate that tells you `.to_int()` is safe; that one is
    // is_int(), which is literally to_int_checked().is_ok(). Gating a to_int on
    // is_numeric() was the V-18 trap: the true answer still panicked.
    {"string", "is_numeric", "bool", ""},     // Check if numeric (int or float)
    {"string", "is_int", "bool", ""},         // to_int_checked().is_ok() - the predicate that gates to_int
    {"string", "to_int", "int", ""},          // Parse string to int (PANICS on garbage)
    // The catchable parses. V-18: before these there was no
    // string->number that could not abort the process, so no CLI could read
    // untrusted input. Uppercase return types are resolved as builtin types by
    // name in checker.c's table mapper, so no per-name special case is needed.
    {"string", "to_int_checked", "ResultInt", ""},      // -> Result<int, string>
    {"string", "to_float_checked", "ResultFloat", ""},  // -> Result<float, string>
    {"string", "ascii", "int", ""},           // ASCII value of first char
    {"string", "to_float", "float", ""},      // Parse string to float
    {"string", "parse_int", "int", ""},       // Parse string to int (alias)
    {"string", "parse_float", "float", ""},   // Parse string to float (alias)
    {"string", "parse_json", "json", ""},     // Parse JSON string, returns json object
    
    // JSON methods
    {"json", "get_string", "string", "string"},     // Get string value by key
    {"json", "get_int", "int", "string"},           // Get int value by key
    {"json", "get_float", "float", "string"},       // Get float value by key
    {"json", "get_bool", "bool", "string"},         // Get bool value by key
    {"json", "free", "void", ""},             // Free JSON object
    // The WRITERS were missing from this table and from dispatch_method below,
    // and a json method absent from BOTH emits nothing at all - so
    // `j.set_int("i", 1)` lowered to an empty statement and `j.stringify()` came
    // back `{}` with the writes silently gone, at exit 0. In expression position
    // the same gap produced `long long v = ;` -> "expected expression". The
    // readers were present, which is why half the surface worked. See the
    // matching entries in dispatch_method for the C functions.
    {"json", "set_string", "void", "string, string"},       // Set string value by key
    {"json", "set_int", "void", "string, int"},          // Set int value by key
    {"json", "set_float", "void", "string, float"},        // Set float value by key
    {"json", "set_bool", "void", "string, bool"},         // Set bool value by key
    {"json", "set_null", "void", "string"},         // Set an explicit JSON null
    {"json", "set", "void", "string, string"},              // Alias of set_string
    {"json", "stringify", "string", ""},      // Serialize to JSON text
    {"json", "to_pretty_string", "string", ""},
    // Reachable now that Json has ONE representation. These were namespace-only
    // because Json_has/Json_keys/Json_array_* took a handle while a json RECEIVER
    // was a WynJson*, so wiring them up would have traded a missing call for a type
    // confusion. Both halves are handles now.
    {"json", "get", "string", "string"},            // Any scalar value as text
    {"json", "get_array", "json", "string"},        // Child array node handle
    {"json", "get_object", "json", "string"},       // Child object node handle
    // `int`, not `bool`: the namespace spelling Json.has is registered int-typed and
    // existing tests compare it to 0 (`Json.has(d, "k") == 0`). The two spellings must
    // agree, and changing the shipped 1/0 output is a separate decision.
    {"json", "has", "int", "string"},
    {"json", "keys", "array", ""},
    {"json", "array_len", "int", ""},
    {"json", "array_get", "json", "int"},
    {"json", "node_str", "string", ""},
    {"json", "is_valid", "bool", ""},

    // HTTP methods (URL is a string)
    {"string", "http_get", "string", ""},     // GET request, returns response body
    {"string", "http_post", "string", "string"},    // POST request with body
    
    // String formatting
    {"string", "format", "string", "..."},      // Variable args: format(arg1, arg2, ...)
    
    // File system methods (path is a string)
    {"string", "exists", "bool", ""},         // Check if path exists
    {"string", "is_file", "bool", ""},        // Check if path is a file
    {"string", "is_dir", "bool", ""},         // Check if path is a directory
    
    // Int methods
    {"int", "to_string", "string", ""},
    {"int", "to_int", "int", ""},    // identity - bool results are typed int (map.contains), keep .to_int() forgiving
    {"int", "to_float", "float", ""},
    {"int", "abs", "int", ""},
    {"int", "pow", "int", "int"},
    {"int", "min", "int", "int"},
    {"int", "max", "int", "int"},
    {"int", "clamp", "int", "int, int"},
    {"int", "is_even", "bool", ""},
    {"int", "is_odd", "bool", ""},
    {"int", "is_positive", "bool", ""},
    {"int", "is_negative", "bool", ""},
    {"int", "is_zero", "bool", ""},
    {"int", "sign", "int", ""},  // Returns -1, 0, or 1
    {"int", "to_binary", "string", ""},
    {"int", "to_hex", "string", ""},
    
    // Float methods
    {"float", "to_string", "string", ""},
    {"float", "to_int", "int", ""},
    {"float", "round", "float", ""},
    {"float", "floor", "float", ""},
    {"float", "ceil", "float", ""},
    {"float", "round_to", "float", "int"},
    {"float", "abs", "float", ""},
    {"float", "pow", "float", "float"},
    {"float", "sqrt", "float", ""},
    {"float", "min", "float", "float"},
    {"float", "max", "float", "float"},
    {"float", "clamp", "float", "float, float"},
    {"float", "is_nan", "bool", ""},
    {"float", "is_infinite", "bool", ""},
    {"float", "is_finite", "bool", ""},
    {"float", "is_positive", "bool", ""},
    {"float", "is_negative", "bool", ""},
    {"float", "sin", "float", ""},
    {"float", "cos", "float", ""},
    {"float", "tan", "float", ""},
    {"float", "asin", "float", ""},
    {"float", "acos", "float", ""},
    {"float", "atan", "float", ""},
    {"float", "log", "float", ""},
    {"float", "log10", "float", ""},
    {"float", "log2", "float", ""},
    {"float", "exp", "float", ""},
    {"float", "sign", "float", ""},  // Returns -1.0, 0.0, or 1.0
    
    // Bool methods
    {"bool", "to_string", "string", ""},
    {"bool", "to_int", "int", ""},
    {"bool", "not", "bool", ""},
    {"bool", "and", "bool", "bool"},
    {"bool", "or", "bool", "bool"},
    {"bool", "xor", "bool", "bool"},
    
    // Char methods
    // The `char` RECEIVER rows lived here and were unreachable by construction: checker.c
    // maps the `char` annotation straight to builtin_int ("char is int in Wyn"), so a
    // char-typed value resolves against the INT receiver table and never reaches a row
    // keyed "char". Measured: `var c: char = 97; c.is_uppercase()` gives "Unknown method
    // 'is_uppercase' for type 'int'". Ten rows, all dead.
    //
    // Removed rather than implemented: giving Wyn a real `char` type is a language change,
    // and the alternative already works - a single character is a 1-length string, which
    // is what `"abc"[0]` returns, and the string receiver has is_alpha/upper/lower.
    // `c.to_string()` and `c.to_int()` keep working via the int rows.
    
    // Array/Vec methods (receiver type will be "array" for now)
    {"array", "len", "int", ""},
    {"array", "is_empty", "bool", ""},
    {"array", "push", "void", "int"},
    {"array", "pop", "int", ""},         // Returns last element
    {"array", "get", "int", "int"},        // Returns element (type depends on array)
    {"array", "contains", "bool", "int"},
    {"array", "index_of", "int", "int"},
    {"array", "reverse", "void", ""},   // Mutates in place
    {"array", "sort", "void", ""},      // Mutates in place
    {"array", "sorted", "array", ""},   // Non-mutating sorted copy (Python sorted)
    {"array", "sort_by", "array", "fn(int)->int"},  // sort_by(key_fn) - sorted by key, monomorphized
    {"array", "max_by", "int", "fn(int)->int"},     // max_by(key_fn) -> element (type depends on array)
    {"array", "min_by", "int", "fn(int)->int"},     // min_by(key_fn) -> element (type depends on array)
    {"array", "group_by", "map", "fn(int)->int"},   // group_by(key_fn) -> map of key -> [elements]
    {"array", "first", "int", ""},      // Returns first element
    {"array", "last", "int", ""},       // Returns last element
    {"array", "count", "int", "int"},      // Count occurrences of value
    {"array", "is_empty", "bool", ""},  // Check if empty
    {"array", "take", "array", "int"},     // Returns new array with first n elements
    {"array", "skip", "array", "int"},     // Returns new array skipping first n elements
    {"array", "slice", "array", "int, int"},    // Returns new array from start to end
    {"array", "join", "string", "string"},    // Join elements with separator
    {"array", "concat", "array", "array"},   // Returns new array concatenated with other
    {"array", "map", "array", "fn(int)->int"},       // Higher-order: map(fn) -> array
    {"array", "filter", "array", "fn(int)->bool"},    // Higher-order: filter(fn) -> array
    {"array", "reduce", "int", "fn(int,int)->int, int"},      // Higher-order: reduce(fn, initial) -> T
    {"array", "any", "bool", "fn(int)->bool"},        // any(fn) -> bool
    {"array", "all", "bool", "fn(int)->bool"},        // all(fn) -> bool
    // find / find_index / partition / zip were advertised here and none of the four can
    // be called, in debug or under --release - the call-generating gate reaches them now
    // that the table carries argument types, and it says:
    //   a.find(f)         "array has no method 'find'"  (did you mean .min()?)
    //   a.find_index(f)   "Unknown method 'find_index' for type 'array'"
    //   a.partition(f)    "Unknown method 'partition' for type 'array'"
    //   a.zip(other)      "Unknown method 'zip' for type 'array'"
    // find_index, partition and zip have no lowering in codegen at all. `find` is the
    // interesting one: array_find_fn IS in both runtime headers and dispatch_method maps
    // to it, but the row's return type is spelled "optional", which the checker's
    // table-to-Type mapper has no arm for, so the call falls through to the
    // unknown-method rule. Repointing it is not a fix either: array_find_fn returns a
    // bare `long long`, so typing the call as OptionInt would hand `.is_some()` a
    // non-Option value - the same trap that retired the Option combinator rows (V-37).
    // Removed rather than repointed, on that precedent. `a.filter(f)` returns the
    // matching elements today and covers the common need.
    {"array", "flatten", "array", ""},   // flatten() -> array
    {"array", "unique", "array", ""},    // unique() -> array
    {"array", "sum", "int", ""},         // sum() -> int (codegen: array_sum)
    {"array", "min", "int", ""},         // min() -> int (codegen: array_min)
    {"array", "max", "int", ""},         // max() -> int (codegen: array_max)
    {"array", "average", "float", ""},   // average() -> float (codegen: array_average)
    {"array", "clear", "void", ""},      // clear() -> void
    {"array", "each", "void", "fn(int)->int"},       // each(fn) (codegen: array_each)
    {"array", "every", "bool", "fn(int)->bool"},      // every(fn) (codegen: array_every)
    {"array", "flat_map", "array", "fn(int)->[int]"},  // flat_map(fn) (codegen: array_flat_map)
    // `void`, not `array`. Both mutate IN PLACE - `void array_insert(WynArray*, int, int)`
    // and `void array_remove_at(WynArray*, int)` - so the row promising an array made the
    // gate bind the result (`v = a.insert(1, 9)`) and codegen died with an internal error.
    // As statements, which is how they are actually used, both have always worked.
    {"array", "insert", "void", "int, int"},    // insert(i, v) (codegen: array_insert)
    {"array", "remove_at", "void", "int"}, // remove_at(i) (codegen: array_remove_at)

    // HashMap methods
    {"map", "insert", "void", "string, int"},
    {"map", "set", "void", "string, int"},
    {"map", "get", "string", "string"},
    {"map", "get_int", "int", "string"},
    {"map", "get_string", "string", "string"},
    {"map", "insert", "void", "string, int"},
    {"map", "insert_int", "void", "string, int"},
    {"map", "insert_string", "void", "string, string"},
    {"map", "set_string", "void", "string, string"},
    {"map", "keys", "array", ""},
    {"map", "len", "int", ""},
    {"map", "contains", "int", "string"},
    {"map", "set_int", "void", "string, int"},
    // #426: set_float / set_bool were the two MISSING siblings of a four-name family.
    // codegen handles all four in ONE branch (`set_int`/`set_string`/`set_float`/
    // `set_bool` -> hashmap_insert_<X>) and only two of the four were registered here.
    // Verified working against the previous build before these rows were written:
    // `m.set_float("b", 2.5)` then reading "b" answers 2.5, and `m.set_bool("b", false)`
    // answers false with len 2 - so they describe a real lowering, not a hoped-for one.
    // Added rather than special-cased in the checker, because the whole family belongs
    // in one place; an incomplete list is what let #426's rule reject working code.
    {"map", "set_float", "void", "string, float"},
    {"map", "set_bool", "void", "string, bool"},
    {"map", "stringify", "string", ""},
    {"map", "remove", "void", "string"},
    // (a second {"map","contains","bool",1} row lived here and was DEAD - lookup is
    //  first-match-wins and the int row above shadows it. The int typing is load-bearing:
    //  callers pass m.contains(k) to assert_eq_int. Converging it to bool is a breaking
    //  change and is tracked separately, not smuggled in here.)
    {"map", "len", "int", ""},
    {"map", "is_empty", "bool", ""},
    {"map", "values", "array", ""},
    {"map", "clear", "void", ""},
    // get_or_default / update / merge / for_each / filter_keys / map_values were
    // advertised here and NONE of the six has a lowering anywhere in codegen - grep for
    // the spelling and the only hit is the row itself. Every one answers
    // "Unknown method '<name>' for type 'map'". `update` had even shipped with its own
    // confession in the comment ("defer - needs lambdas"), which is a row saying out loud
    // that it is not implemented. Removed, on the same precedent as `map.entries`: an
    // advertising row with nothing behind it is the defect, and the removal is what makes
    // the unknown-method message (with its available-methods hint) the whole answer.
    // Iterate a map with `for k in m.keys()` today.

    // HashSet methods
    {"set", "insert", "void", "string"},
    {"set", "contains", "bool", "string"},
    // V-38 (#391): `contains_int` is a real int-family method now (src/hashset.c), so it
    // must declare the same `bool` return its `contains` sibling does - without this row
    // it fell through to the int default and `print(s.contains_int(9))` printed `1` where
    // `print(s.contains(9))` prints `true`.
    //
    // The 4th column is param_types (#393), a STRING, not an argument count: `"int"` is
    // "one int argument". A bare `1` does NOT compile here, and a bare `0` DOES - as a
    // null pointer constant, silently meaning "no param_types" rather than "no arguments".
    //
    // The element-taking rows keep `"string"`: this table is keyed on the receiver-type
    // STRING "set", so it cannot see a set's element type at all. wyn_set_elem_fn()
    // re-points the emitted call at the element-typed C variant instead.
    {"set", "contains_int", "bool", "int"},
    {"set", "remove", "void", "string"},
    {"set", "len", "int", ""},
    {"set", "is_empty", "bool", ""},
    {"set", "clear", "void", ""},
    {"set", "union", "set", "set"},
    {"set", "intersection", "set", "set"},
    {"set", "difference", "set", "set"},
    {"set", "is_subset", "bool", "set"},
    {"set", "is_superset", "bool", "set"},
    {"set", "is_disjoint", "bool", "set"},
    // symmetric_difference / from_array / filter / map / for_each were advertised here and
    // none of the five has a lowering: `s.filter(f)` answers "Unknown method 'filter' for
    // type 'set'" and then dies with "compilation failed (internal codegen error)". The
    // four set-algebra rows above (union/intersection/difference plus the three
    // predicates) DO work - set_union and friends are real functions - which is what made
    // this half look implemented. Removed (#393), same precedent as `set.to_array`. This
    // lane does NOT restore them: it gives the set an element type, not new methods.
    
    // Option methods
    {"option", "is_some", "bool", ""},
    {"option", "is_none", "bool", ""},
    {"option", "unwrap", "int", ""},    // Type depends on Option<T>
    {"option", "unwrap_or", "int", "int"}, // Type depends on Option<T>
    {"option", "to_string", "string", ""},  // #386

    // Result methods
    {"result", "is_ok", "bool", ""},
    {"result", "is_err", "bool", ""},
    {"result", "unwrap", "int", ""},    // Type depends on Result<T,E>
    {"result", "unwrap_or", "int", "int"}, // Type depends on Result<T,E>
    {"result", "unwrap_err", "string", ""}, // #386 - the Err of every family is a string
    {"result", "to_string", "string", ""},  // #386
    // #386: `to_string` (both families) and `unwrap_err` (Result) WORK in the runtime and
    // were missing from this table, so it under-reported what the language has. The rows
    // above are that record.
    //
    // WHAT THE ROWS ACTUALLY BUY, measured rather than assumed: they put these three
    // methods under run_registry_reachable_test.sh, which compiles and RUNS a real call
    // per row in BOTH build modes. Before the rows, nothing in that gate exercised them.
    // That is the whole effect. Specifically NOT what they do, both mutation-checked by
    // altering the row and rebuilding:
    //   - they do not decide the TYPE (changing `unwrap_err`'s return to "int" here reds
    //     no arm of any gate - see below for where the typing really comes from);
    //   - they do not feed the typo suggestion for these receivers. An Option/Result typo
    //     is answered by the NAMESPACE route ("unknown method 'ResultInt.unwrap_er' on
    //     namespace 'ResultInt'"), which does not read this table - deleting the row
    //     changes that message not at all.
    //
    // READ THIS BEFORE ASSUMING THEY ARE WHAT MAKES THEM TYPE: an earlier attempt
    // added exactly these rows, had no measurable effect, and was reverted. Measured again
    // while adding them - with the rows in place and nothing else changed, all of these
    // still failed:
    //
    //     s = g().to_string(); print(s.len())        # Unknown method 'len' for type 'int'
    //     fn f(o: string?) -> string { return o.to_string() }   # Expected string, got int
    //
    // The reason is that every realistic Option/Result receiver is the monomorphic
    // TYPE_STRUCT family ("OptionString"), and get_receiver_type_string() has no
    // TYPE_STRUCT case, so it answers NULL and this table is never consulted for them.
    // The typing therefore lives in the checker's method-call path, resolved explicitly
    // ahead of the table lookup - the same route #413 used for unwrap_or, and for the
    // same reason. Fixing get_receiver_type_string() instead would route Option through
    // this table, where `{"option","unwrap","int",""}` would type
    // `Option<string>.unwrap()` as int and break working code.
    //
    // `unwrap_err` needed only the row: it already types correctly through the
    // `<Family>_unwrap_err` symbol route, with the family's own err type. The checker was
    // NOT given a second answer for it - one was written and mutation-tested, and altering
    // it changed no arm of any gate, so it was removed rather than shipped unverified.
    //
    // V-37's rows for the ten combinators (option map/and_then/filter/expect/or_else,
    // result map/and_then/map_err/expect/or_else) are deliberately NOT restored here. A
    // row carries ONE concrete return type per receiver, and `map` CHANGES the family -
    // `int?.map(fn(x: int) -> string {..})` is an Option<string> - so any single answer
    // written here would be wrong. #392 types them in checker.c from the callback's
    // return type instead.
    //
    // NOTE ON THIS COLUMN: the 4th field is param_types, a STRING (#393), not an argument
    // count. `""` is zero arguments. A bare `0` still COMPILES - as a null pointer
    // constant - and silently means "no param_types", which is not the same thing.

    // Sentinel - marks end of table
    {NULL, NULL, NULL, NULL}
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

// #425: the declared ARGUMENT TYPES of a method, as the comma-separated spec string in
// the 4th column ("int, string"), or NULL when the method is unknown. Empty string means
// "takes no arguments" and is NOT the same answer as NULL.
//
// #393 added that column and proved every row is CALLABLE. It could not prove the
// declared types are the lowering's types - mutating `pad_left`'s row to "string, string"
// still compiled, because reachability asks whether a call can be made, not whether the
// types are right. So this is the column's first real reader, and the rule built on it is
// deliberately coarse for that reason (see wyn_arg_category in checker.c).
const char* lookup_method_param_types(const char* receiver_type, const char* method_name) {
    if (!receiver_type || !method_name) return NULL;
    for (int i = 0; method_signatures[i].receiver_type != NULL; i++) {
        if (strcmp(method_signatures[i].receiver_type, receiver_type) == 0 &&
            strcmp(method_signatures[i].method_name, method_name) == 0) {
            // First match wins, matching lookup_method_return_type - the table
            // registers some rows twice and the two lookups must agree on which one
            // they are describing.
            return method_signatures[i].param_types;
        }
    }
    return NULL;
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

// The ONE table for methods on a json receiver: `doc.get_string(k)` must lower to
// exactly what the namespace spelling `Json.get_string(doc, k)` lowers to. Every
// entry is a capital-J handle function, because Json has one representation - a
// long long index into the runtime's node arena.
//
// This is deliberately a single function called from both dispatch sites. There were
// two separate json tables in this file with DIFFERENT contents, one of them wired to
// the retired WynJson* pairs model, and the readers-vs-writers split between them is
// how `a.set_int(..)` came to emit nothing (#312) while `doc.get_string(..)`
// dereferenced an integer handle. Add a method here and both spellings get it.
static bool wyn_json_method_c_function(const char* method_name, int arg_count, MethodDispatch* out) {
    struct { const char* m; int argc; const char* fn; } json_methods[] = {
        // readers
        {"get",              1, "Json_get"},
        {"get_string",       1, "Json_get_string"},
        {"get_int",          1, "Json_get_int"},
        {"get_float",        1, "Json_get_float"},
        {"get_bool",         1, "Json_get_bool"},
        {"get_array",        1, "Json_get_array"},
        {"get_object",       1, "Json_get_object"},
        {"has",              1, "Json_has"},
        {"keys",             0, "Json_keys"},
        {"array_len",        0, "Json_array_len"},
        {"array_get",        1, "Json_array_get"},
        {"node_str",         0, "Json_node_str"},
        {"is_valid",         0, "Json_is_valid"},
        // writers
        {"set",              2, "Json_set_string"},
        {"set_string",       2, "Json_set_string"},
        {"set_int",          2, "Json_set_int"},
        {"set_float",        2, "Json_set_float"},
        {"set_bool",         2, "Json_set_bool"},
        {"set_null",         1, "Json_set_null"},
        // whole-document
        {"stringify",        0, "Json_stringify"},
        {"to_pretty_string", 0, "Json_to_pretty_string"},
        {"free",             0, "Json_free"},
    };
    for (size_t i = 0; i < sizeof(json_methods) / sizeof(json_methods[0]); i++) {
        if (strcmp(method_name, json_methods[i].m) == 0 && arg_count == json_methods[i].argc) {
            out->c_function = json_methods[i].fn;
            return true;
        }
    }
    return false;
}

// V-36: does this Json method take the HANDLE as its first argument? Every entry in the
// table above does - Json has one representation, a long long arena index - and
// `Json_parse` is the only Json runtime function taking a `const char*`, which is why it
// is absent from that table and so answers false here.
//
// Asked of the table itself rather than answered with a list of Json method names in the
// checker: a Json method added above is then covered by the check-time rule without
// anyone remembering to add it twice. The argc loop spans the table's whole range (0-2),
// because the question is about the method, not about one call's arity.
bool wyn_json_method_takes_handle(const char* method_name) {
    MethodDispatch d;
    for (int argc = 0; argc <= 2; argc++)
        if (wyn_json_method_c_function(method_name, argc, &d)) return true;
    return false;
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
        // Both spellings, one lowering. `to_bytes` had an EMPTY body here and `bytes`
        // carried the assignment TWICE (the second line unreachable) - a botched edit, and
        // the reason `"abc".to_bytes()` was refused while `"abc".bytes()` worked, even
        // though string_to_bytes has been in the runtime archive all along.
        if (strcmp(method_name, "to_bytes") == 0 && arg_count == 0) {
            out->c_function = "string_to_bytes"; return true;
        }
        if (strcmp(method_name, "bytes") == 0 && arg_count == 0) {
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
        if (strcmp(method_name, "is_int") == 0 && arg_count == 0) {
            out->c_function = "str_is_int"; return true;
        }
        if (strcmp(method_name, "to_int_checked") == 0 && arg_count == 0) {
            out->c_function = "str_to_int_checked"; return true;
        }
        if (strcmp(method_name, "to_float_checked") == 0 && arg_count == 0) {
            out->c_function = "str_to_float_checked"; return true;
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
        // JSON parsing method. Json_parse, not the retired json.c entry point:
        // `"...".parse_json()` yields TYPE_JSON, and every reader on a TYPE_JSON
        // receiver is a handle function, so producing a WynJson* here handed a
        // pointer to code that treated it as an index.
        if (strcmp(method_name, "parse_json") == 0 && arg_count == 0) {
            out->c_function = "Json_parse"; return true;
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
        return wyn_json_method_c_function(method_name, arg_count, out);
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
        // HashSet methods. The four ELEMENT-taking names below (add / insert /
        // contains / remove, plus the two _int spellings) come back with the
        // STRING-set C function; the caller re-points them at the element-typed
        // variant through wyn_set_elem_fn(), because this table is keyed on a
        // receiver-type STRING and so cannot see the element type. See
        // wyn_set_elem_method() at the bottom of this file.
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
        // V-37: expect / or_else / map / and_then / filter lowered to wyn_optional_*
        // here. Those take WynOptional* - the retired heap-boxed model - while codegen
        // emits the OptionInt/OptionString value-struct family, so the lowering could
        // never link. Dropped with the signature rows that advertised them; the checker
        // now rejects the calls with a message naming what Option does have.
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
        // V-37, the Result half: these lowered to wyn_result_*, which take WynResult* -
        // the retired heap-boxed model - while codegen emits the ResultInt/ResultString
        // value-struct family. See the note in the option branch above.
        return false;
    }
    
    // JSON object methods. This used to be a SECOND, shorter copy of the json table
    // above with three entries wired to the other representation's functions
    // (json_get_string(WynJson*) against a handle) - two tables for one receiver, so
    // whichever ran first decided whether `doc.get_string(k)` read a document or
    // dereferenced an integer. One authority now.
    if (strcmp(receiver_type, "json") == 0) {
        return wyn_json_method_c_function(method_name, arg_count, out);
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
        {"Json_get_string", "string"}, {"Json_node_str", "string"},
        {"Json_get_float", "float"}, {"Json_is_valid", "bool"},
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
        // HashSet.contains is `bool` in the set RECEIVER table above
        // ({"set","contains","bool"}), and `HashSet` is BOTH a namespace and a
        // registered type - so the dotted spelling reads the receiver table and the
        // `::` spelling reads this one. Without this entry `HashSet.contains(s,"a")`
        // printed `true` and `HashSet::contains(s,"a")` printed `1`. (HashMap.has
        // needs no entry: hashmap_has is declared `bool` in the runtime, so both
        // spellings already agree. hashset_contains is declared `int`.)
        {"HashSet_contains", "bool"},
        // Same shape, and the reason it needs saying twice: hashmap_has IS declared
        // `bool`, so the DIRECT call already printed true/false by accident of the C
        // declaration - but the checker had no type for it, so `var v = HashMap.has(m,k)`
        // declared a non-bool and printed `1`. Registering the type makes the accident
        // into the rule, in both spellings and through a variable.
        {"HashMap_has", "bool"},
        // File's three predicates. Same pair of tables, same split: the dotted
        // `File.exists(".")` printed `true` because File_exists is declared `bool` in
        // wyn_runtime.h, while `File::exists(".")` and `var v = File.exists(".")` both
        // printed `1` because the checker had no type for either. (The `.exists()`
        // METHOD on a string is a different lowering - `_exists` - and is registered in
        // the receiver table above.)
        {"File_exists", "bool"}, {"File_is_dir", "bool"}, {"File_is_file", "bool"},
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
// Named, not anonymous: both spellings' tables are held through one pointer below,
// and two anonymous structs are distinct types in C (-Wpointer-type-mismatch).
typedef struct { const char* ns; const char* method; const char* sym; } WynNsRename;
static const WynNsRename wyn_ns_renames[] = {
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

// THE TWO SPELLINGS LOWER DIFFERENTLY TODAY, AND THAT IS A BUG - BUT NOT THIS BUG.
//
// `Ns.method()` and `Ns::method()` are the same call, and codegen_expr.c lowers them
// through two separate chains that disagree. Measured on dev @ 82f8d2bc by reading
// the generated C for each spelling of the same program:
//
//   HashMap.set_int  -> hashmap_insert_int   (declared: builds and runs)
//   HashMap::set_int -> hashmap_set_int      (NOT declared: build fails)
//   String.char      -> String_char_from_int (builds)
//   String::char     -> String_char          (NOT declared: build fails)
//   Regex.match      -> regex_match          (builds)
//   Regex::match     -> Regex_match          (NOT declared: build fails)
//   File.list_dir    -> File_list_dir        (char*)
//   File::list_dir   -> file_list_dir        (WynArray)   <- DIFFERENT FUNCTIONS
//
// Converging them is a real fix and is NOT attempted here. The last row is why: the
// two File prefixes are not aliases. `char* File_list_dir(const char*)` and
// `WynArray file_list_dir(const char*)` return different C types, and each spelling's
// checker type already agrees with its own symbol, so pointing both at one of them
// breaks the other. ("Both names are declared" is not evidence they are the same
// function - that assumption regressed File::list_dir once already while this change
// was being written.) It needs the runtime types reconciled first; filed separately.
//
// What this file therefore owns is the lowering FOR EACH SPELLING, in one place, so
// the checker can ask what the call it is looking at will actually emit. The old
// arrangement had the `::` mapping only inside codegen, where the checker could not
// see it - which is precisely why the check-time rule shipped for `.` alone.
//
// The `::` map below mirrors codegen_expr.c's `::` chain exactly, including what it
// does NOT special-case (no Regex, Http, Task or String entries - those fall to the
// plain `<Ns>_<method>`), because a faithful copy is the only kind that can tell the
// truth about what will be emitted.
static const struct { const char* ns; const char* prefix; } wyn_ns_prefixes_colon[] = {
    {"HashMap",       "hashmap_"},
    {"HashSet",       "hashset_"},
    {"Random",        "random_"},
    {"File",          "file_"},
    {"StringBuilder", "StringBuilder_"},
    {"Color",         "Color_"},
    {"Time",          "Time_"},
    {"System",        "System_"},
    {NULL, NULL}
};
static const WynNsRename wyn_ns_renames_colon[] = {
    {"HashMap", "get", "hashmap_get_string"},
    {"HashMap", "set", "hashmap_set"},
    {"HashMap", "has", "hashmap_has"},
    {NULL, NULL, NULL}
};

static int wyn_ns_spelling_is_colon(const char* separator) {
    return separator && separator[0] == ':';
}

int wyn_namespace_c_symbol_spelled(const char* ns, const char* method,
                                   const char* separator, char* out, size_t out_sz) {
    if (!ns || !method || !out || out_sz == 0) return 0;
    if (!is_builtin_module(ns)) return 0;
    int colon = wyn_ns_spelling_is_colon(separator);
    const WynNsRename* renames = colon ? wyn_ns_renames_colon : wyn_ns_renames;
    for (int i = 0; renames[i].ns; i++) {
        if (strcmp(renames[i].ns, ns) == 0 && strcmp(renames[i].method, method) == 0) {
            snprintf(out, out_sz, "%s", renames[i].sym);
            return 1;
        }
    }
    const char* pfx = NULL;
    if (colon) {
        for (int i = 0; wyn_ns_prefixes_colon[i].ns; i++)
            if (strcmp(wyn_ns_prefixes_colon[i].ns, ns) == 0) { pfx = wyn_ns_prefixes_colon[i].prefix; break; }
    } else {
        pfx = wyn_ns_prefix_for(ns);
    }
    if (pfx) snprintf(out, out_sz, "%s%s", pfx, method);
    else     snprintf(out, out_sz, "%s_%s", ns, method);
    return 1;
}

// The dotted spelling, which is what this name has always meant - codegen_expr.c's
// dot chain and main.c's post-compile diagnostic both call it and are unchanged.
int wyn_namespace_c_symbol(const char* ns, const char* method, char* out, size_t out_sz) {
    return wyn_namespace_c_symbol_spelled(ns, method, ".", out, out_sz);
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
// The C prefix this namespace lowers to in `separator`'s spelling, so a caller can
// ask the index about the namespace as a whole.
static void wyn_ns_prefix_spelled(const char* ns, const char* separator,
                                  char* out, size_t out_sz) {
    char probe[320];
    // Derive it from the lowering itself rather than re-deriving the prefix map:
    // lower a method name that cannot be a rename, then drop it off the end.
    if (wyn_namespace_c_symbol_spelled(ns, "\x01", separator, probe, sizeof(probe))) {
        size_t n = strlen(probe);
        if (n >= 1) probe[n - 1] = '\0';           // strip the sentinel method
        snprintf(out, out_sz, "%s", probe);
        return;
    }
    snprintf(out, out_sz, "%s_", ns);
}

static int wyn_ns_declared_count_spelled(const char* ns, const char* separator) {
    char pfx[160];
    wyn_ns_prefix_spelled(ns, separator, pfx, sizeof(pfx));
    size_t pl = strlen(pfx);
    int n = 0;
    for (int i = 0; i < wyn_rt_sym_count; i++)
        if (strncmp(wyn_rt_syms[i], pfx, pl) == 0 && wyn_rt_syms[i][pl]) n++;
    return n;
}

int wyn_namespace_method_unknown_spelled(const char* ns, const char* method,
                                         const char* separator) {
    char sym[320];
    if (!wyn_namespace_c_symbol_spelled(ns, method, separator, sym, sizeof(sym))) return 0;
    if (wyn_runtime_declares(sym) != 0) return 0;   // declared, or index unavailable
    // Zero symbols under the namespace's prefix means the index cannot speak for the
    // namespace at all (a user module named `math` shadowing the builtin list, a
    // header this build does not ship), and the checker must stay permissive rather
    // than reject every call to it.
    if (wyn_ns_declared_count_spelled(ns, separator) == 0) return 0;
    return 1;
}

int wyn_namespace_method_unknown(const char* ns, const char* method) {
    return wyn_namespace_method_unknown_spelled(ns, method, ".");
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
// Which tier answered, because the right HELP text depends on it: "Wyn has no such
// function" is false when the only problem is the spelling the user chose.
#define WYN_NS_SUGGEST_OTHER_SPELLING  1
#define WYN_NS_SUGGEST_OTHER_NAMESPACE 2
#define WYN_NS_SUGGEST_NEAR_MISS       3
int wyn_suggest_namespace_method_spelled(const char* ns, const char* method,
                                        const char* separator,
                                        char* out, size_t out_sz) {
    if (!ns || !method || !out || out_sz == 0) return 0;
    if (!separator || !*separator) separator = ".";
    wyn_rt_index_load();
    if (wyn_rt_index_state != 1) return 0;

    // TIER 1: the same call in the OTHER spelling. Because the two spellings lower
    // differently (see wyn_namespace_c_symbol_spelled), a method can be real in one
    // and unbuildable in the other - `HashMap::set_int` lowers to an undeclared
    // hashmap_set_int while `HashMap.set_int` lowers to hashmap_insert_int and works.
    // Answering "unknown method" there and stopping would be true but useless; the
    // useful answer is the spelling that does work. Tried first because it is not a
    // guess at all: same namespace, same method, verified declared.
    {
        const char* other_sep = (separator[0] == ':') ? "." : "::";
        char sym[320];
        if (wyn_namespace_c_symbol_spelled(ns, method, other_sep, sym, sizeof(sym)) &&
            wyn_runtime_declares(sym) == 1) {
            snprintf(out, out_sz, "%s%s%s()", ns, other_sep, method);
            return WYN_NS_SUGGEST_OTHER_SPELLING;
        }
    }

    // TIER 2: the right name in the WRONG namespace - the likelier typo, and still
    // not a guess: `Time.millis()` is a real mistake with a real answer
    // (DateTime.millis). Kept in the separator the user typed.
    for (int i = 0; ; i++) {
        const char* other = builtin_module_name_at(i);
        if (!other) break;
        if (strcmp(other, ns) == 0) continue;
        char sym[320];
        if (!wyn_namespace_c_symbol_spelled(other, method, separator, sym, sizeof(sym))) continue;
        if (wyn_runtime_declares(sym) == 1) {
            snprintf(out, out_sz, "%s%s%s()", other, separator, method);
            return WYN_NS_SUGGEST_OTHER_NAMESPACE;
        }
    }

    // TIER 3: a near miss inside the namespace the user named
    // (`File.read_al` -> `File.read_all`).
    char pfx[160];
    wyn_ns_prefix_spelled(ns, separator, pfx, sizeof(pfx));
    size_t pl = strlen(pfx);
    const char* best = NULL;
    int best_dist = WYN_NAME_FAR;
    for (int i = 0; i < wyn_rt_sym_count; i++) {
        if (strncmp(wyn_rt_syms[i], pfx, pl) != 0 || !wyn_rt_syms[i][pl]) continue;
        const char* cand = wyn_rt_syms[i] + pl;
        int d = wyn_name_distance(method, cand);
        if (d > 0 && d < best_dist) { best_dist = d; best = cand; }
    }
    if (best) {
        snprintf(out, out_sz, "%s%s%s()", ns, separator, best);
        return WYN_NS_SUGGEST_NEAR_MISS;
    }
    return 0;
}

int wyn_suggest_namespace_method(const char* ns, const char* method, char* out, size_t out_sz) {
    return wyn_suggest_namespace_method_spelled(ns, method, ".", out, out_sz);
}

// ---------------------------------------------------------------------------
// V-38 (#391): set element-type dispatch. ONE authority for the four places that
// lower a set element - the `{:...}` literal, `x in s`, `s.add(x)` and the
// `HashSet.add(s, x)` namespace form. Those four disagreeing is exactly the defect
// shape this issue is about: the element type existed nowhere, so every one of them
// emitted the string-keyed call.

bool wyn_set_elem_method(const char* method_name) {
    if (!method_name) return false;
    static const char* const names[] = {
        "add", "insert", "contains", "remove", "add_int", "contains_int", NULL
    };
    for (int i = 0; names[i]; i++)
        if (strcmp(method_name, names[i]) == 0) return true;
    return false;
}

const char* wyn_set_elem_fn(const char* base, const Type* elem) {
    if (!base) return base;
    // A NULL element type is an OPEN set (`{:}` / `HashSet.new()` with nothing
    // added yet). Nothing can be inserted through a call that has no element, so
    // the string form is the harmless default and matches what the C prototypes
    // in hashset.h have always been.
    if (!elem) return base;
    const char* suffix = NULL;
    switch (elem->kind) {
        case TYPE_INT:    suffix = "_int";   break;
        case TYPE_FLOAT:  suffix = "_float"; break;
        case TYPE_BOOL:   suffix = "_bool";  break;
        default: return base;                      // string (and anything the
                                                   // checker refuses upstream)
    }
    // `add_int` / `contains_int` are already the int family under their advertised
    // spelling; appending a second suffix would name nothing.
    size_t bl = strlen(base);
    if (bl > 4 && strcmp(base + bl - 4, "_int") == 0) return base;

    // Static table rather than a formatted buffer: the caller uses the result as a
    // plain `const char*` with no lifetime contract, and a static snprintf buffer
    // would be clobbered by the next call in the same emit (`s.union(t)` emits two
    // set calls in one expression).
    struct { const char* base; const char* i; const char* f; const char* b; } map[] = {
        {"hashset_add",      "hashset_add_int",      "hashset_add_float",      "hashset_add_bool"},
        {"hashset_contains", "hashset_contains_int", "hashset_contains_float", "hashset_contains_bool"},
        {"hashset_remove",   "hashset_remove_int",   "hashset_remove_float",   "hashset_remove_bool"},
    };
    for (size_t i = 0; i < sizeof(map)/sizeof(map[0]); i++) {
        if (strcmp(base, map[i].base) != 0) continue;
        if (strcmp(suffix, "_int") == 0)   return map[i].i;
        if (strcmp(suffix, "_float") == 0) return map[i].f;
        return map[i].b;
    }
    return base;
}
