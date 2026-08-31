// wyn_runtime_slim.h - types + forward declarations for optimized builds
#ifndef WYN_RUNTIME_SLIM_H
#define WYN_RUNTIME_SLIM_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#ifdef __TINYC__
#define __auto_type long long
#endif

// Reference counting (wyn_rc.c) - copied verbatim from wyn_runtime.h:6-12.
// codegen emits wyn_rc_release() around every temporary string (see the
// string-concat and println lowering), so the slim header needs it too or any
// program that builds a string fails to compile under --release.
void* wyn_rc_alloc(size_t size);
void wyn_rc_retain(const void* ptr);
void wyn_rc_release(const void* ptr);
void wyn_rc_set_length(const void* ptr, unsigned int len);
unsigned int wyn_rc_get_length(const void* ptr);

// Abort-on-OOM allocators, verbatim from wyn_runtime.h:42-44. `static inline` in
// both headers, so there is nothing in the archive to link - they must be
// duplicated, not declared. Needed by the array_push_struct macro below.
static inline void* wyn_malloc(size_t n) { void* p = malloc(n); if (!p && n) { fprintf(stderr, "wyn: out of memory (%zu bytes)\n", n); abort(); } return p; }
static inline void* wyn_calloc(size_t c, size_t n) { void* p = calloc(c, n); if (!p && c && n) { fprintf(stderr, "wyn: out of memory\n"); abort(); } return p; }
static inline void* wyn_realloc(void* p, size_t n) { void* q = realloc(p, n); if (!q && n) { fprintf(stderr, "wyn: out of memory\n"); abort(); } return q; }

// Value type tags (WYN_TYPE_FLOAT, WYN_TYPE_STRING, ...). Included rather than
// copied: these tags are a WIRE FORMAT between codegen and the runtime, since
// codegen writes WYN_TYPE_FLOAT into WynValue.type and the runtime reads it
// back. A copied enum that drifted by one would silently mis-read every float
// instead of failing loudly, so both headers take the values from the single
// canonical definition here. wyn_runtime.h:170 includes this same file.
#include "arc_runtime.h"
// The HashMap / HashSet / Json entry points are plain functions living in
// hashmap.c / hashset.c / json.c, i.e. real symbols in libwyn_rt.a. Include
// their canonical headers rather than restating the prototypes, for the same
// drift reason as arc_runtime.h above: a hand-copied signature that disagreed
// (int vs long long, missing const) would compile and then corrupt arguments.
// Without these, --release rejected every program that touched a HashMap,
// HashSet or Json with `call to undeclared function 'hashmap_new'`.
#include "hashmap.h"
#include "hashset.h"
#include "json.h"

// Forward declarations for opaque types
typedef struct WynHashMap WynHashMap;
typedef struct WynHashSet WynHashSet;
typedef struct WynJson WynJson;
typedef struct WynOptional WynOptional;
typedef struct WynArena WynArena;
typedef struct HttpResponse HttpResponse;
typedef struct TcpServer TcpServer;

typedef struct WynArenaBlock {
    char* data;
    size_t used;
    size_t capacity;
    struct WynArenaBlock* next;
} WynArenaBlock;
typedef struct { void* fn; void* env; } WynClosure;
typedef struct HttpResponse HttpResponse;
typedef struct TcpServer TcpServer;
typedef struct WynArena WynArena;
typedef struct { void** keys; void** values; int count; } WynMap;
typedef struct {
    int type; // WynType enum
    union {
        long long int_val;
        double float_val;
        const char* string_val;
        struct WynArray* array_val;
        void* struct_val;
    } data;
} WynValue;
// Layout mirror of wyn_runtime.h WynArray (incl. the `writing` mutation flag) - keep in sync.
typedef struct WynArray { WynValue* restrict data; int count; int capacity; int writing; } WynArray;
typedef struct { int start; int end; int current; } WynRange;
typedef struct { const char* message; const char* type; } WynError;
typedef struct { WynArray arr; } Queue;
typedef struct { WynArray arr; } Stack;
typedef struct { int tag; union { int ok_value; const char* err_value; } data; } ResultInt;
typedef struct { int tag; union { const char* ok_value; const char* err_value; } data; } ResultString;
typedef struct { int tag; int value; } OptionInt;
typedef struct { int tag; const char* value; } OptionString;
typedef struct {
    long long value;
    pthread_mutex_t lock;
} WynSharedValue;
typedef struct {
    char* data;
    int len;
    int cap;
} WynStringBuilder;
typedef struct { char type; /* o=object, a=array, s=string, n=number, b=bool, x=null */ char* key; char* str_val; double num_val; int parent; int next_sibling; int first_child; } JsonNode;
typedef struct { char** fields; int field_count; } CsvRow;
typedef struct { CsvRow* rows; int row_count; char** headers; int header_count; } CsvDoc;

// Forward declarations
void wyn_arena_reset();
char* wyn_str_alloc(size_t len);
char* wyn_strdup(const char* s);

// Future/spawn declarations
typedef struct Future Future;
typedef void (*TaskFunc)(void*);
typedef void* (*TaskFuncWithReturn)(void*);
void wyn_spawn_fast(TaskFunc func, void* arg);
void wyn_spawn_fast_traced(TaskFunc func, void* arg, const char* file, int line);
Future* wyn_spawn_async(TaskFuncWithReturn func, void* arg);
Future* wyn_spawn_async_traced(TaskFuncWithReturn func, void* arg, const char* file, int line);
Future* future_new(void);
void future_set(Future* f, void* result);
void* future_get(Future* f);
void future_free(Future* f);
int future_is_ready(Future* f);
// `spawn f()` on a non-yielding fn lowers to wyn_spawn_inline; awaiting a set of
// futures lowers to wyn_await_all_int / wyn_await_any_int, which consume each
// future via future_get_consume. All four are real symbols (spawn_fast.c,
// future.c, runtime_exports.c) but were undeclared here, so every --release
// build of a spawn/await program failed. Signatures mirror wyn_runtime.h:17,
// :4825, :4909 and future.h.
struct Future* wyn_spawn_inline(TaskFuncWithReturn func, void* arg);
void* future_get_consume(Future* f);

// WebSocket module
int Ws_connect(const char* url);
int Ws_send(int sock, const char* msg);
char* Ws_recv(int sock);
void Ws_close(int sock);

// Socket module
int Socket_connect(const char* host, int port);
int Socket_send(int sock, const char* data, int len);
char* Socket_recv(int sock, int max_len);
void Socket_close(int sock);

// Test runtime
extern int wyn_test_fail_count;
void wyn_assert(int condition);
void wyn_assert_eq_int(long long a, long long b);
void wyn_assert_eq_str(const char* a, const char* b);

// === Module function declarations (for slim header builds) ===

// Http
int Http_listen(int port);
int Http_accept(int server_fd);
char* Http_method(int req);
char* Http_path(int req);
char* Http_body(int req);
void Http_respond(int fd, int status, const char* body);
void Http_respond_json(int fd, int status, const char* json);
void Http_respond_with_header(int fd, int status, const char* content_type, const char* body);
void Http_close_client(int fd);
void Http_close_server(int fd);
int Http_status(int req);

// Json
long long Json_new(void);
void Json_set(long long j, const char* key, const char* val);
void Json_set_string(long long j, const char* key, const char* val);
void Json_set_int(long long j, const char* key, long long val);
// `int v`, matching wyn_runtime.h:3555 - the handle keeps this file's long long
// spelling (a WynJson* is pointer-sized) but the value width must not drift.
void Json_set_bool(long long j, const char* key, int val);
char* Json_get_string(long long j, const char* key);
long long Json_get_int(long long j, const char* key);
char* Json_stringify(long long j);
long long Json_parse(const char* s);
int Json_has(long long j, const char* key);
char* Json_to_pretty_string(long long j);

// File
char* File_read(const char* path);
WynArray File_read_lines(const char* path);
int File_write(const char* path, const char* content);
int File_exists(const char* path);
int File_delete(const char* path);
char* File_cwd(void);

// Db
long long Db_open(const char* path);
void Db_close(long long db);
long long Db_exec(long long db, const char* sql);
long long Db_exec_p(long long db, const char* sql, ...);
char* Db_query(long long db, const char* sql);
char* Db_query_p(long long db, const char* sql, ...);
char* Db_query_one(long long db, const char* sql);

// System
char* System_exec(const char* cmd);
void System_gc(void);

// Crypto / Encoding
char* Crypto_sha256(const char* data);
char* Crypto_hmac_sha256(const char* key, const char* data);
char* Crypto_hmac_sha256_hex(const char* hex_key, const char* data);
char* Encoding_base64_encode(const char* data);
char* Encoding_base64_decode(const char* data);

// Uuid
char* Uuid_generate(void);

// DateTime
long long DateTime_now(void);

// Env
char* Env_get(const char* key);
int Env_set(const char* key, const char* val);
WynClosure wyn_closure_new(void* fn, void* env);
int wyn_closure_call_int(WynClosure c, int arg);
const char* wyn_string_concat_safe(const char* left, const char* right);
int regex_match(const char* str, const char* pattern);
char* regex_replace(const char* str, const char* pattern, const char* replacement);
int Regex_match(const char* s, const char* p);
char* Regex_replace(const char* s, const char* p, const char* r);
int Regex_find(const char* s, const char* p);
char* regex_find_all(const char* str, const char* pattern);
char* regex_split(const char* str, const char* pattern);
WynArray array_new();
void array_push_int(WynArray* arr, long long value);
// Float/bool element push. Mirrors array_push_int; definitions live in
// wyn_runtime.h:647 / :659. Those are non-inline, so they only reach the linker
// through a TU that includes wyn_runtime.h - src/runtime_exports.c exists for
// exactly that, and IS now in Makefile's RT_SRCS (it was previously only in
// TCC_RT_SRCS, which is why every --release build used to fail at link).
void array_push_float(WynArray* arr, double value);
void array_push_bool(WynArray* arr, int value);
void array_each(WynArray arr, long long (*fn)(long long));
void array_free(WynArray* arr);
// Pushing a struct is a MACRO, not a function: it needs the static struct type
// to size the heap box. Kept byte-identical to wyn_runtime.h:931-943 - a
// divergent copy would lay out the boxed struct differently from the reader
// (array_get_struct), which is a silent memory bug rather than a link error.
#define array_push_struct(arr, value, StructType) do { \
    StructType __temp_val = (value); \
    if ((arr)->count >= (arr)->capacity) { \
        (arr)->capacity = (arr)->capacity == 0 ? 4 : (arr)->capacity * 2; \
        (arr)->data = wyn_realloc((arr)->data, sizeof(WynValue) * (arr)->capacity); \
    } \
    (arr)->data[(arr)->count].type = WYN_TYPE_STRUCT; \
    (arr)->data[(arr)->count].data.struct_val = wyn_malloc(sizeof(StructType)); \
    memcpy((arr)->data[(arr)->count].data.struct_val, &__temp_val, sizeof(StructType)); \
    (arr)->count++; \
} while(0)
// Out-of-bounds panic used by the bounds-checked getters below. Copied verbatim
// from wyn_runtime.h:755-782 (wyn_lenient_mode / wyn_src_file / wyn_oob_panic)
// so the two headers report identical diagnostics.
//
// Lenient-mode gate: panics that guard silent wrong answers (OOB index,
// bad numeric parses, checked-arithmetic overflow) are FATAL by default.
// WYN_LENIENT=1 restores the old print-and-continue-with-0 behavior for
// programs that depended on it. WYN_STRICT is accepted as a no-op alias of
// the default so existing "strict" invocations keep working.
static inline int wyn_lenient_mode(void) {
    static int cached = -1;
    if (cached < 0) cached = getenv("WYN_LENIENT") != NULL;
    return cached;
}
// A panic file is normally remapped to the user's `.wyn` source by the `#line`
// directives codegen emits per statement. Hoisted code without a preceding
// `#line` (e.g. lambda bodies) reports the raw generated `__FILE__`, which ends
// in `.wyn.c` and leaks the compiler's generated-C seam. Trim a trailing
// `.wyn.c` -> `.wyn` so a panic never points the user at a file they can't see.
static inline const char* wyn_src_file(const char* file) {
    if (!file) return file;
    size_t n = strlen(file);
    static char buf[1024];   // panic is terminal; a shared buffer is fine
    if (n >= 6 && n < sizeof(buf) && strcmp(file + n - 6, ".wyn.c") == 0) {
        memcpy(buf, file, n - 2);   // drop the trailing ".c"
        buf[n - 2] = '\0';
        return buf;
    }
    return file;
}
static inline void wyn_oob_panic(int index, int count, const char* file, int line) {
    if (file && line > 0)
        fprintf(stderr, "panic at %s:%d: array index out of bounds: index %d, length %d\n", wyn_src_file(file), line, index, count);
    else
        fprintf(stderr, "panic: array index out of bounds: index %d, length %d\n", index, count);
    if (!wyn_lenient_mode()) exit(1);
}
// Float/bool element getters. The full header (wyn_runtime.h:671-687) defines
// these as `static inline` *_impl functions behind a __FILE__/__LINE__ macro so
// an out-of-bounds panic can name the call site. Reproduced here verbatim
// rather than declared as plain functions, because codegen emits the MACRO
// spelling `array_get_float(arr, idx)` with two arguments - a two-parameter
// prototype would compile but lose the source location, and the *_impl symbols
// are static-inline in the full header so they are not in the archive to link
// against. Keep byte-identical to wyn_runtime.h.
#define array_get_float(arr, idx) array_get_float_impl(arr, idx, __FILE__, __LINE__)
static inline double array_get_float_impl(WynArray arr, int index, const char* file, int line) {
    if (index < 0) index += arr.count;
    if (index < 0 || index >= arr.count) { wyn_oob_panic(index, arr.count, file, line); return 0.0; }
    if (arr.data[index].type == WYN_TYPE_FLOAT) return arr.data[index].data.float_val;
    if (arr.data[index].type == WYN_TYPE_INT) return (double)arr.data[index].data.int_val;
    return 0.0;
}
// Bool element getter. Returns `bool` so to_string()'s _Generic dispatch picks
// bool_to_string -> "true"/"false" rather than int_to_string -> "1"/"0" (G5).
#define array_get_bool(arr, idx) array_get_bool_impl(arr, idx, __FILE__, __LINE__)
static inline bool array_get_bool_impl(WynArray arr, int index, const char* file, int line) {
    if (index < 0) index += arr.count;
    if (index < 0 || index >= arr.count) { wyn_oob_panic(index, arr.count, file, line); return false; }
    return arr.data[index].data.int_val ? true : false;
}
void array_push_str(WynArray* arr, const char* value);
void array_push_array(WynArray* arr, WynArray* nested);
// int/str element getters, same reasoning (and the same verbatim-copy rule) as
// the float/bool pair above: wyn_runtime.h:783-804 defines them as `static
// inline *_impl` behind a __FILE__/__LINE__ macro, so they are NOT symbols in
// libwyn_rt.a. Declaring them as plain prototypes here - which is what this
// header did - compiled fine and then failed at link with "Undefined symbols:
// _array_get_str" for any --release program that read a [string] element.
#define array_get_int(arr, idx) array_get_int_impl(arr, idx, __FILE__, __LINE__)
static inline long long array_get_int_impl(WynArray arr, int index, const char* file, int line) {
    if (index < 0) index += arr.count;   // Python-style negative index: a[-1] == last
    if (index < 0 || index >= arr.count) {
        wyn_oob_panic(index, arr.count, file, line);
        return 0;
    }
    if (arr.data[index].type == WYN_TYPE_INT || arr.data[index].type == WYN_TYPE_BOOL)
        return arr.data[index].data.int_val;   // bool is stored in int_val
    if (arr.data[index].type == WYN_TYPE_FLOAT) return (long long)arr.data[index].data.float_val;
    return 0;
}
#define array_get_str(arr, idx) array_get_str_impl(arr, idx, __FILE__, __LINE__)
static inline const char* array_get_str_impl(WynArray arr, int index, const char* file, int line) {
    if (index < 0) index += arr.count;   // Python-style negative index
    if (index < 0 || index >= arr.count) {
        wyn_oob_panic(index, arr.count, file, line);
        return "";
    }
    if (arr.data[index].type == WYN_TYPE_STRING) return arr.data[index].data.string_val;
    return "";
}
#define array_get_struct(arr, idx, T) (*(T*)arr.data[idx].data.struct_val)
WynValue array_get(WynArray arr, int index);
WynArray* array_get_array(WynArray arr, int index);
int array_get_nested_int(WynArray arr, int index1, int index2);
int array_get_nested3_int(WynArray arr, int index1, int index2, int index3);
// The non-int nested getters must be declared here too, and with EXACTLY the
// return types wyn_runtime.h defines: codegen_expr.c picks one of these by the
// inner element's type, so a missing declaration is not merely a warning - the
// implicit int return truncates a float and reinterprets a char* as a number,
// which is how `m[0][1]` on a [[string]] printed an address. Signatures mirror
// wyn_runtime.h:820-863; keep them in sync.
double array_get_nested_float(WynArray arr, int index1, int index2);
double array_get_nested3_float(WynArray arr, int index1, int index2, int index3);
bool array_get_nested_bool(WynArray arr, int index1, int index2);
const char* array_get_nested_str(WynArray arr, int index1, int index2);
const char* array_get_nested3_str(WynArray arr, int index1, int index2, int index3);
int array_len(WynArray arr);
bool array_is_empty(WynArray arr);
bool array_contains(WynArray arr, int value);
bool array_contains_str(WynArray arr, const char* value);
int array_index_of_str(WynArray arr, const char* value);
void array_remove_str(WynArray* arr, const char* value);
void array_push(WynArray* arr, long long value);

// Lightweight int array for spawn futures
typedef struct { long long* data; int count; int capacity; } WynIntArray;
WynIntArray int_array_new();
void int_array_push(WynIntArray* a, long long v);
long long int_array_get(WynIntArray a, int i);
int int_array_len(WynIntArray a);
// await_all / await_any over a set of futures. Declared after WynIntArray
// because they take one by value. wyn_runtime.h:4807-4917.
//
// ALL EIGHT await_all variants are needed, not just the ones a given program
// calls: codegen_expr.c:1173 emits a `_Generic((futs), WynIntArray:
// wyn_await_all_int<sfx>, WynArray: wyn_await_all<sfx>)` selection, and C
// requires every association in a _Generic to name a declared function even
// though only one is chosen. Declaring just the chosen arm still failed.
WynArray wyn_await_all(WynArray futures);
WynArray wyn_await_all_str(WynArray futures);
WynArray wyn_await_all_float(WynArray futures);
WynArray wyn_await_all_struct(WynArray futures);
WynArray wyn_await_all_int(WynIntArray futures);
WynArray wyn_await_all_int_str(WynIntArray futures);
WynArray wyn_await_all_int_float(WynIntArray futures);
WynArray wyn_await_all_int_struct(WynIntArray futures);
long long wyn_await_any(WynArray futures);
long long wyn_await_any_int(WynIntArray futures);
int array_pop(WynArray* arr);
int array_index_of(WynArray arr, int value);
void array_reverse(WynArray* arr);
void array_sort(WynArray* arr);
void array_sort_str(WynArray* arr);
void array_sort_float(WynArray* arr);
WynArray array_sorted(WynArray arr);
WynArray array_flatten(WynArray arr);
int array_first(WynArray arr);
int array_last(WynArray arr);
int array_count(WynArray arr, int value);
void array_clear(WynArray* arr);
int array_min(WynArray arr);
int array_max(WynArray arr);
int array_sum(WynArray arr);
double array_sum_float(WynArray arr);
double array_min_float(WynArray arr);
double array_max_float(WynArray arr);
double array_first_float(WynArray arr);
double array_last_float(WynArray arr);
double array_pop_float(WynArray* arr);
long long array_contains_float(WynArray arr, double val);
long long array_index_of_float(WynArray arr, double val);
double array_average(WynArray arr);
void array_remove_value(WynArray* arr, int value);
void array_remove_at(WynArray* arr, int index);
void array_insert(WynArray* arr, int index, int value);
WynArray array_take(WynArray arr, int n);
WynArray array_skip(WynArray arr, int n);
WynArray wyn_array_slice_range(WynArray arr, int start, int end);
WynArray wyn_array_slice_from(WynArray arr, int start);
char* array_join(WynArray arr, const char* sep);
WynArray array_concat(WynArray arr1, WynArray arr2);
WynRange range(int start, int end);
bool range_has_next(WynRange* r);
int range_next(WynRange* r);
int string_length(const char* str);
char* string_substring(const char* str, int start, int end);
// Declarations, not copies - see the to_string note near the bottom of this
// file. The hand-written `static inline` versions these replace all omitted the
// wyn_rc_set_length() the real ones do (wyn_runtime.h:1383-1401), so a
// --release string carried a bogus RC length and the release() codegen emits
// after it corrupted the heap.
bool string_contains(const char* str, const char* substr);
char* string_concat(const char* a, const char* b);
char* string_upper(const char* str);
char* string_lower(const char* str);
int string_is_alpha(const char* str);
int string_is_digit(const char* str);
int string_is_alnum(const char* str);
int string_is_whitespace(const char* str);
const char* string_char_at(const char* str, int index);
int string_equals(const char* a, const char* b);
int string_count(const char* str, const char* substr);
int string_is_numeric(const char* str);
char* string_capitalize(const char* str);
char* string_reverse(const char* str);
int string_len(const char* str);
int string_is_empty(const char* str);
int string_starts_with(const char* str, const char* prefix);
int string_ends_with(const char* str, const char* suffix);
int string_index_of(const char* str, const char* substr);
// Ditto: the local copy this replaces had a real bug the archive version does
// not - when a match was found mid-string it copied the prefix but never the
// replacement, and never advanced past the needle, so `"Hello World".replace(
// "World", "Wyn")` overran its buffer and segfaulted under --release while
// working on the default path. wyn_runtime.h:1516.
char* string_replace(const char* str, const char* old, const char* new_str);
char* string_replace_all(const char* str, const char* old, const char* new_str);
int string_last_index_of(const char* str, const char* substr);
char* string_slice(const char* str, int start, int end);
char* string_repeat(const char* str, int count);
char* string_title(const char* str);
char* string_trim_left(const char* str);
char* string_trim_right(const char* str);
char* string_trim(const char* str);
WynArray string_split(const char* str, const char* delim);
const char* wyn_string_charat(const char* str, int index);
WynArray string_chars(const char* str);
WynArray string_to_bytes(const char* str);
char* string_pad_left(const char* str, int width, const char* pad);
char* string_pad_right(const char* str, int width, const char* pad);
WynArray string_lines(const char* str);
WynArray string_words(const char* str);
void set_clear(WynHashSet* set);
WynHashSet* set_union(WynHashSet* set1, WynHashSet* set2);
WynHashSet* set_intersection(WynHashSet* set1, WynHashSet* set2);
WynHashSet* set_difference(WynHashSet* set1, WynHashSet* set2);
bool set_is_subset(WynHashSet* set1, WynHashSet* set2);
bool set_is_superset(WynHashSet* set1, WynHashSet* set2);
bool set_is_disjoint(WynHashSet* set1, WynHashSet* set2);
double int_to_float(int n);
int int_abs(int n);
int int_pow(int base, int exp);
int int_min(int a, int b);
int int_max(int a, int b);
int int_clamp(int n, int min, int max);
int int_is_even(int n);
int int_is_odd(int n);
int int_is_positive(int n);
int int_is_negative(int n);
int int_is_zero(int n);
int int_sign(int n);
char* int_to_binary(int n);
char* int_to_hex(int n);
long long float_to_int(double f);
double float_round(double f);
double float_round_to(double f, int decimals);
double float_floor(double f);
double float_ceil(double f);
double float_abs(double f);
double float_pow(double base, double exp);
double float_sqrt(double f);
double float_min(double a, double b);
double float_max(double a, double b);
double float_clamp(double f, double min, double max);
int float_is_nan(double f);
int float_is_infinite(double f);
int float_is_finite(double f);
int float_is_positive(double f);
int float_is_negative(double f);
double float_sign(double f);
double float_sin(double f);
double float_cos(double f);
double float_tan(double f);
double float_asin(double f);
double float_acos(double f);
double float_atan(double f);
double float_log(double f);
double float_log10(double f);
double float_log2(double f);
double float_exp(double f);
int map_get(WynMap map, const char* key);
void map_set(WynMap* map, const char* key, int value);
void map_clear(WynMap* map);
int map_get_or_default(WynHashMap* map, const char* key, int default_value);
void map_merge(WynHashMap* dest, WynHashMap* src);
int map_len(WynHashMap* map);
bool map_is_empty(WynHashMap* map);
bool map_has(WynHashMap* map, const char* key);
void map_remove(WynHashMap* map, const char* key);
char* http_request(const char* method, const char* url, const char* body);
char* https_get(const char* url);
char* https_post(const char* url, const char* data);
char* http_get(const char* url);
char* http_post(const char* url, const char* data);
char* http_put(const char* url, const char* data);
char* http_delete(const char* url);
void http_set_header(const char* key, const char* val);
void http_clear_headers();
int http_status();
char* http_error();
char* last_error_get();
char* url_encode(const char* str);
char* url_decode(const char* str);
char* base64_encode(const char* str);
int hash_string(const char* str);
void print_args_impl(int count, ...);
void print_int(int x);
void print_float(double x);
void print_str(const char* s);
void print_bool(bool b);
void print_int_no_nl(long long x);
void print_float_no_nl(double x);
void print_str_no_nl(const char* s);
void print_bool_no_nl(bool b);
void print_array(WynArray arr);
void print_array_no_nl(WynArray arr);
// print() buffers the whole line and emits ONE fwrite, so concurrent spawns
// cannot interleave mid-line. Defined in wyn_runtime.h, exported through
// libwyn_rt.a; declared here because --release includes this header instead.
// WynStrBuf's layout is duplicated from wyn_runtime.h, which is the definition
// site. C11 permits an identical typedef redeclaration, so including both headers
// in one translation unit is safe. The _Static_assert below fails the build if
// the two ever diverge - a silent layout mismatch would corrupt the caller's
// stack frame, because the functions are compiled against the fat header and
// called from code compiled against this one.
#ifndef WYN_STRBUF_DEFINED
#define WYN_STRBUF_DEFINED
typedef struct { char* buf; size_t len; size_t cap; } WynStrBuf;
#endif
typedef struct { WynStrBuf sb; } WynOut;
_Static_assert(sizeof(WynStrBuf) == sizeof(char*) + 2 * sizeof(size_t),
               "WynStrBuf layout diverged from wyn_runtime.h");
void wyn_out_begin(WynOut* o);
void wyn_out_str(WynOut* o, const char* s);
void wyn_out_int(WynOut* o, long long v);
void wyn_out_float(WynOut* o, double v);
void wyn_out_bool(WynOut* o, bool v);
void wyn_out_elem(WynOut* o, WynValue v);
void wyn_out_array(WynOut* o, WynArray arr);
void wyn_out_flush(WynOut* o);
void print_value(WynValue v);
void print_hex(int x);
void print_bin(int x);
void print_debug(const char* label, int val);
// long long / double, not int / float: a Wyn int is 64-bit and a Wyn float is a
// C double, so the narrower C types silently truncated (4294967297 -> 1) and lost
// precision. Must match the definitions in wyn_runtime.h.
long long input();
double input_float();
char* input_line();
void printf_wyn(const char* format, ...);
char* string_format(const char* format, ...);
char* wyn_str_format(const char* format, int argc, ...);
double sin_approx(double x);
double cos_approx(double x);
double pi_const();
double e_const();
int str_len(const char* s);
int str_eq(const char* a, const char* b);
char* str_concat(const char* a, const char* b);
char* str_upper(const char* s);
char* str_lower(const char* s);
int str_contains(const char* s, const char* sub);
int str_starts_with(const char* s, const char* prefix);
int str_ends_with(const char* s, const char* suffix);
char* str_trim(const char* s);
const char* Fs_read_file(const char* path);
char* str_repeat(const char* s, int count);
char* str_reverse(const char* s);
int bool_to_int(bool x);
bool bool_not(bool x);
bool bool_and(bool x, bool y);
bool bool_or(bool x, bool y);
bool bool_xor(bool x, bool y);
// `a / b` and `a % b` lower to the MACRO form, which threads __FILE__/__LINE__
// through so a divide-by-zero panic names the user's line. The two-argument
// prototypes that used to be here compiled and then failed at link with
// "Undefined symbols: _wyn_safe_div" - only the *_impl symbols exist
// (wyn_runtime.h:2597-2612). Same trap as array_get_float/_str above.
long long wyn_safe_div_impl(long long a, long long b, const char* file, int line);
long long wyn_safe_mod_impl(long long a, long long b, const char* file, int line);
#define wyn_safe_div(a, b) wyn_safe_div_impl(a, b, __FILE__, __LINE__)
#define wyn_safe_mod(a, b) wyn_safe_mod_impl(a, b, __FILE__, __LINE__)
int char_to_int(char x);
char char_from_int(int x);
bool char_is_alpha(char x);
bool char_is_numeric(char x);
bool char_is_alphanumeric(char x);
bool char_is_whitespace(char x);
char* String_from_chars(WynArray arr);
bool char_is_uppercase(char x);
bool char_is_lowercase(char x);
char char_to_upper(char x);
char char_to_lower(char x);
WynError Error(const char* msg);
WynError TypeError(const char* msg);
WynError ValueError(const char* msg);
WynError DivisionByZeroError(const char* msg);
char* str_substring(const char* s, int start, int end);
int str_index_of(const char* s, const char* sub);
char* str_slice(const char* s, int start, int end);
char* str_pad_start(const char* s, int len, const char* pad);
char* str_pad_end(const char* s, int len, const char* pad);
char* str_remove_prefix(const char* s, const char* prefix);
char* str_remove_suffix(const char* s, const char* suffix);
char* str_capitalize(const char* s);
char* str_center(const char* s, int width);
void str_free(char* s);
long long str_parse_int(const char* s);
long long str_ascii(const char* s);
const char* String_char_from_int(long long n);
int str_parse_int_failed(int result);
double str_parse_float(const char* s);
int abs_val(int x);
int pow_int(int base, int exp);
int clamp(int x, int min_val, int max_val);
int sign(int x);
int gcd(int a, int b);
int lcm(int a, int b);
char* file_read(const char* path);
WynArray file_list_dir(const char* path);
int file_is_file(const char* path);
int file_is_dir(const char* path);
char* file_get_cwd();
int file_create_dir(const char* path);
int file_file_size(const char* path);
char* file_path_join(const char* a, const char* b);
char* file_basename(const char* path);
char* file_dirname(const char* path);
char* file_extension(const char* path);
char* Path_basename(const char* p);
char* Path_dirname(const char* p);
char* Path_extension(const char* p);
char* Path_join(const char* a, const char* b);
int file_write(const char* path, const char* data);
int file_exists(const char* path);
int file_delete(const char* path);
int file_copy(const char* src, const char* dst);
int file_move(const char* src, const char* dst);
// File module - declared in module declarations block above
int File_copy(const char* s, const char* d);
int File_move(const char* s, const char* d);
long long File_size(const char* p);
int File_is_dir(const char* p);
int File_is_file(const char* p);
int File_mkdir(const char* p);
char* File_list_dir(const char* p);
int File_append(const char* p, const char* d);
long long File_open(const char* path, const char* mode);
char* File_read_line(long long handle);
int File_write_line(long long handle, const char* data);
int File_eof(long long handle);
void File_close(long long handle);
// Http, Json, etc. declared in module declarations block above
int Http_route_match(const char* pattern, const char* path, WynHashMap* params);
WynHashMap* Http_parse_request(const char* raw);
int Http_ctx_fd(WynHashMap* ctx);
void Http_respond_html(int fd, int status, const char* html);
int Http_serve(int port);
char* hashmap_keys_str(WynHashMap* map);
WynArray hashmap_keys(WynHashMap* map);
WynArray hashmap_values(WynHashMap* map);
WynArray hashmap_get_array(WynHashMap* map, const char* key);
WynArray* hashmap_group_slot(WynHashMap* map, const char* key);
// `m[k]` index reads: panic on a missing key (see hashmap.c).
int    hashmap_index_int_impl(WynHashMap* map, const char* key, const char* file, int line);
double hashmap_index_float_impl(WynHashMap* map, const char* key, const char* file, int line);
char*  hashmap_index_string_impl(WynHashMap* map, const char* key, const char* file, int line);
int    hashmap_index_bool_impl(WynHashMap* map, const char* key, const char* file, int line);
#define hashmap_index_int(map, key)    hashmap_index_int_impl(map, key, __FILE__, __LINE__)
#define hashmap_index_float(map, key)  hashmap_index_float_impl(map, key, __FILE__, __LINE__)
#define hashmap_index_string(map, key) hashmap_index_string_impl(map, key, __FILE__, __LINE__)
#define hashmap_index_bool(map, key)   hashmap_index_bool_impl(map, key, __FILE__, __LINE__)
char* string_split_to_str(const char* s, const char* delim);
WynHashSet* HashSet_new();
// Json declared in module declarations block above
void Terminal_raw_mode();
void Terminal_restore();
int Terminal_read_key();
void Terminal_clear();
void Terminal_write(const char* s);
int Terminal_cols();
int Terminal_rows();
void Terminal_move(int row, int col);
void Terminal_color(int fg);
void Terminal_bg(int bg);
void Terminal_reset();
void Terminal_bold();
void Terminal_dim();
void Terminal_underline();
void Terminal_hide_cursor();
void Terminal_show_cursor();
void Terminal_print_color(const char* s, int fg);
void Terminal_box(int row, int col, int w, int h);
void Terminal_progress(int row, int col, int width, int percent);
char* Regex_find_all(const char* s, const char* p);
char* Regex_split(const char* s, const char* p);
int file_size(const char* path);
int file_mkdir(const char* path);
int file_rmdir(const char* path);
int file_create_dir_all(const char* path);
int file_remove_dir_all(const char* path);
char* System_exec(const char* cmd);
int System_exec_code(const char* cmd);
void System_exit(int code);
char* System_env(const char* key);
int System_set_env(const char* key, const char* value);
WynArray System_args();
char* Env_get(const char* name);
int Env_set(const char* name, const char* value);
WynArray Env_all();
float Math_sin(float x);
float Math_cos(float x);
float Math_tan(float x);
float Math_sqrt(float x);
float Math_pow(float base, float exp);
float Math_floor(float x);
float Math_ceil(float x);
float Math_round_to(float x, long long places);
float Math_atan2(float y, float x);
float Math_pi();
float Math_e();
float String_to_float(const char* s);
float Math_round(float x);
long long Math_abs(long long x);
float Math_max(float a, float b);
float Math_min(float a, float b);
float Math_random();
long long DateTime_now();
long long DateTime_millis();
long long DateTime_micros();
char* DateTime_format(int timestamp, const char* fmt);
void DateTime_sleep(int seconds);
Queue* Queue_new();
void Queue_push(Queue* q, int value);
int Queue_pop(Queue* q);
int Queue_peek(Queue* q);
int Queue_len(Queue* q);
int Queue_is_empty(Queue* q);
Stack* Stack_new();
void Stack_push(Stack* s, int value);
int Stack_pop(Stack* s);
int Stack_peek(Stack* s);
int Stack_len(Stack* s);
int Stack_is_empty(Stack* s);
int Net_listen(int port);
int Net_connect(const char* host, int port);
int Net_send(int sockfd, const char* data);
char* Net_recv(int sockfd);
int Net_close(int sockfd);
char* Time_format(int timestamp);
// Time.now()/Time.sleep(), Shared.* and the array HOFs. Every one is a symbol in
// libwyn_rt.a and every one was missing here, so --release rejected the programs
// that use them. `long` (not long long) for Time_now is deliberate - it matches
// wyn_runtime.h:450 exactly; disagreeing would be a silent ABI mismatch.
long Time_now();
void Time_sleep(long long ms);
long long Shared_new(long long initial);
long long Shared_get(long long handle);
long long Shared_add(long long handle, long long delta);
WynArray wyn_array_map(WynArray arr, long long (*fn)(long long));
WynArray wyn_array_filter(WynArray arr, long long (*fn)(long long));
long long wyn_array_reduce(WynArray arr, long long (*fn)(long long, long long), long long initial);
long file_modified_time(const char* path);
int arr_sum(WynArray arr, int len);
int arr_max(WynArray arr, int len);
int arr_min(WynArray arr, int len);
int arr_contains(WynArray arr, int len, int val);
int arr_find(WynArray arr, int len, int val);
void arr_reverse(int* arr, int len);
void arr_sort(int* arr, int len);
int arr_count(int* arr, int len, int val);
void arr_fill(int* arr, int len, int val);
int arr_all(int* arr, int len, int val);
char* arr_join(int* arr, int len, const char* sep);
WynArray arr_map_double(WynArray arr);
WynArray arr_map_square(WynArray arr);
WynArray arr_filter_positive(WynArray arr);
WynArray arr_filter_even(WynArray arr);
WynArray arr_filter_greater_than_3(WynArray arr);
int arr_reduce_sum(WynArray arr);
int arr_reduce_product(WynArray arr);
int random_int(int min, int max);
int random_range(int min, int max);
double random_float();
void seed_random(int seed);
int random_bool();
char* random_string(int len);
char* random_hex(int len);
char* random_uuid();
int random_choice_int(long long* arr, int len);
char* random_choice_str(char** arr, int len);
void random_seed_auto();
long long time_now();
char* time_format(int timestamp, const char* fmt);
void assert_eq(int a, int b);
void assert_true(int cond);
void assert_false(int cond);
void panic(const char* msg);
void todo(const char* msg);
void exit_program(int code);
void sleep_ms(int ms);
char* getenv_var(const char* name);
int setenv_var(const char* name, const char* val);
int sqrt_int(int x);
int ceil_int(double x);
int floor_int(double x);
int round_int(double x);
double abs_float(double x);
char* str_replace(const char* s, const char* old, const char* new);
char* split_get(const char* s, const char* delim, int index);
int split_count(const char* s, const char* delim);
char* char_at(const char* s, int index);
int is_numeric(const char* s);
int str_count(const char* s, const char* substr);
int str_contains_substr(const char* s, const char* substr);
char* str_join(char** arr, int len, const char* sep);
char* int_to_str(int n);
long long str_to_int(const char* s);
double str_to_float(const char* s);
void swap(int* a, int* b);
double clamp_float(double x, double min_val, double max_val);
double lerp(double a, double b, double t);
double map_range(double x, double in_min, double in_max, double out_min, double out_max);
int bit_set(int x, int pos);
int bit_clear(int x, int pos);
int bit_toggle(int x, int pos);
int bit_check(int x, int pos);
int bit_count(int x);
ResultInt ResultInt_Ok(int value);
ResultInt ResultInt_Err(const char* msg);
int ResultInt_is_ok(ResultInt r);
int ResultInt_is_err(ResultInt r);
int ResultInt_unwrap(ResultInt r);
const char* ResultInt_unwrap_err(ResultInt r);
long long ResultInt_unwrap_or(ResultInt r, long long def);
ResultString ResultString_Ok(const char* value);
ResultString ResultString_Err(const char* msg);
int ResultString_is_ok(ResultString r);
int ResultString_is_err(ResultString r);
const char* ResultString_unwrap(ResultString r);
const char* ResultString_unwrap_err(ResultString r);
OptionInt OptionInt_Some(int value);
OptionInt OptionInt_None();
int OptionInt_is_some(OptionInt o);
int OptionInt_is_none(OptionInt o);
int OptionInt_unwrap(OptionInt o);
int OptionInt_unwrap_or(OptionInt o, int def);
OptionString OptionString_Some(const char* value);
OptionString OptionString_None();
int OptionString_is_some(OptionString o);
int OptionString_is_none(OptionString o);
const char* OptionString_unwrap(OptionString o);
const char* OptionString_unwrap_or(OptionString o, const char* def);
long long Task_value(long long initial);
long long Task_get(long long handle);
void Task_set(long long handle, long long value);
void Task_add(long long handle, long long amount);
void Task_free_value(long long handle);
long long Task_channel(long long capacity);
void Task_send(long long handle, long long value);
long long Task_recv(long long handle);
void Task_close(long long handle);
long long Task_try_recv(long long handle, long long* out_value);
long long Task_select_2(long long ch1, long long ch2);
long long Task_select_3(long long ch1, long long ch2, long long ch3);
// Db declared in module declarations block above
long long Db_last_insert_id(long long handle);
char* Db_error(long long handle);
long long StringBuilder_new();
void StringBuilder_append(long long handle, const char* s);
long long StringBuilder_len(long long handle);
void StringBuilder_clear(long long handle);
void StringBuilder_free(long long handle);
char* StringBuilder_to_string(long long handle);   // wyn_runtime.h:5195
// Json_parse declared in module declarations block above
char* Json_get(long long root, const char* key);
long long Json_get_int(long long root, const char* key);
long long Json_array_len(long long node);
long long Json_array_get(long long node, long long index);
char* Json_node_str(long long node);
WynArray Json_keys(long long root);
char* Encoding_base64_encode(const char* data);
char* Encoding_base64_decode(const char* data);
char* Encoding_hex_encode(const char* data);
char* Crypto_sha256(const char* data);
char* Crypto_md5(const char* data);
char* Os_platform();
char* Os_arch();
char* Os_hostname();
long long Os_pid();
char* Os_temp_dir();
char* Os_home_dir();
char* Uuid_generate();
long long array_pop_int(WynArray* arr);
WynArray array_reverse_copy(WynArray arr);
char* array_join_str(WynArray arr, const char* sep);
long long array_index_of_int(WynArray arr, long long val);
void array_insert_at(WynArray* arr, int index, long long val);
WynArray array_unique_int(WynArray arr);
char* hashmap_values_string(WynHashMap* map);
double Math_log(double x);
double Math_log10(double x);
double Math_exp(double x);
long long Math_clamp(long long x, long long lo, long long hi);
long long Math_sign(long long x);
long long DateTime_diff(long long a, long long b);
long long DateTime_add_seconds(long long t, long long n);
char* DateTime_to_iso(long long timestamp);
long long regex_find(const char* str, const char* pattern);
int File_rename(const char* old_path, const char* new_path);
void Test_assert_eq_float(double actual, double expected, double epsilon, const char* msg);
// The rest of the Test module (test_runtime.c, declared at wyn_runtime.h:250-262).
// `assert`/`assert_eq` in a Wyn test block lower to these, so without them no
// test file could be built with --release.
void Test_init(const char* suite_name);
void Test_assert(int condition, const char* message);
void Test_assert_eq_int(int actual, int expected, const char* message);
void Test_assert_eq_str(const char* actual, const char* expected, const char* message);
void Test_assert_ne_int(int actual, int expected, const char* message);
void Test_assert_gt(int actual, int threshold, const char* message);
void Test_assert_lt(int actual, int threshold, const char* message);
void Test_assert_gte(int actual, int threshold, const char* message);
void Test_assert_lte(int actual, int threshold, const char* message);
void Test_assert_contains(const char* haystack, const char* needle, const char* message);
void Test_assert_null(void* ptr, const char* message);
void Test_assert_not_null(void* ptr, const char* message);
void Test_describe(const char* description);
void Test_skip(const char* reason);
char* Net_resolve(const char* hostname);
char* Db_escape(const char* str);
void Log_set_level(long long level);
void Log_debug(const char* msg);
void Log_info(const char* msg);
void Log_warn(const char* msg);
void Log_error(const char* msg);
char* Process_exec_capture(const char* cmd);
long long Process_exec_status(const char* cmd);
long long Http_get_json(const char* url);
long long Http_post_json(const char* url, const char* data);
long long hashmap_get_or_int(WynHashMap* map, const char* key, long long default_val);
char* hashmap_get_or_str(WynHashMap* map, const char* key, const char* default_val);
double hashmap_get_or_float(WynHashMap* map, const char* key, double default_val);
long long hashmap_get_or_bool(WynHashMap* map, const char* key, long long default_val);
void wyn_map_compound_missing_key(const char* key);
double Json_get_float(long long root, const char* key);
long long Json_get_bool(long long root, const char* key);
long long Json_get_array(long long root, const char* key);
long long Json_get_object(long long root, const char* key);
char* File_glob(const char* pattern);
char* File_walk_dir(const char* path);
char* File_temp_file();
char* DateTime_format_duration(long long ms);
long long DateTime_day_of_week(long long timestamp);
long long DateTime_year(long long timestamp);
long long DateTime_month(long long timestamp);
long long DateTime_day(long long timestamp);
long long DateTime_hour(long long timestamp);
long long DateTime_minute(long long timestamp);
long long DateTime_second(long long timestamp);
char* Encoding_hex_decode(const char* hex);
char* Encoding_csv_parse(const char* csv);
char* Crypto_hmac_sha256(const char* key, const char* data);
char* Crypto_hmac_sha256_hex(const char* hex_key, const char* data);
char* Crypto_random_bytes(long long n);
void StringBuilder_append_int(long long handle, long long val);
void StringBuilder_append_line(long long handle, const char* s);
double Math_lerp(double a, double b, double t);
double Math_map_range(double x, double in_min, double in_max, double out_min, double out_max);
long long string_char_code(const char* s, long long index);
long long Db_table_exists(long long handle, const char* table_name);
void Test_assert_not_contains(const char* haystack, const char* needle, const char* msg);
void Http_set_timeout(long long seconds);
long long Http_timeout();
long long Csv_parse(const char* text);
long long Csv_row_count(long long handle);
const char* Csv_get(long long handle, long long row, long long col);
const char* Csv_get_field(long long handle, long long row, const char* header);
long long Csv_col_count(long long handle, long long row);
const char* Csv_header(long long handle, long long col);
long long Csv_header_count(long long handle);
void System_gc();
void Data_save(const char* path, WynHashMap* map);
WynHashMap* Data_load(const char* path);
char* Template_render_string(const char* tmpl, WynHashMap* ctx);
char* Template_render(const char* path, WynHashMap* ctx);
void System_load_env(const char* path);

// to_string helpers. These are DECLARATIONS of the real runtime functions
// (wyn_runtime.h:2589-2633, in the archive via runtime_exports.c), NOT local
// copies - and that distinction is the whole point.
//
// This header used to define int_to_string / float_to_string as `static inline`
// returning a `static char __buf[]`. A single shared buffer per function means
// every to_string in one expression returns the SAME pointer, so string
// interpolation with two or more of them printed the last value in every slot:
// `print("${a} + ${b} = ${a+b}")` with a=10 b=20 gave "30 + 30 = 30" under
// --release and "10 + 20 = 30" on the default path. It also broke the RC
// contract codegen relies on - it emits wyn_rc_release() on the result of
// int_to_string/float_to_string, which is only valid for a wyn_rc_alloc'd
// buffer. Linking the real, per-call-allocating definitions fixes both and
// makes the two build modes agree by construction rather than by copy.
char* int_to_string(long long x);
char* float_to_string(double x);
char* str_to_string(const char* x);
char* bool_to_string(bool x);
char* array_to_string(WynArray arr);

// _Generic macros for type-dispatched print/println/to_string
#define print_no_nl(x) _Generic((x), \
    int: print_int_no_nl, \
    float: print_float_no_nl, \
    double: print_float_no_nl, \
    char*: print_str_no_nl, \
    const char*: print_str_no_nl, \
    bool: print_bool_no_nl, \
    WynArray: print_array_no_nl, \
    default: print_int_no_nl)(x)

#define print(x) _Generic((x), \
    int: print_int_no_nl, \
    long: print_int_no_nl, \
    long long: print_int_no_nl, \
    float: print_float_no_nl, \
    double: print_float_no_nl, \
    char*: print_str_no_nl, \
    const char*: print_str_no_nl, \
    bool: print_bool_no_nl, \
    WynArray: print_array, \
    default: print_int_no_nl)(x)

#define wyn_out_append(o, x) _Generic((x), \
    int: wyn_out_int, \
    long: wyn_out_int, \
    long long: wyn_out_int, \
    float: wyn_out_float, \
    double: wyn_out_float, \
    char*: wyn_out_str, \
    const char*: wyn_out_str, \
    bool: wyn_out_bool, \
    WynArray: wyn_out_array, \
    default: wyn_out_int)(o, x)

// Was: do { print(x); printf("\n"); } while(0) - TWO libc calls, so under
// --release println had exactly the interleaving bug the fat header fixed for
// the default path (libc locks one printf per-FILE, not a sequence). Measured
// on the default path: 8 spawns x 200 lines gave 362-690 malformed lines of
// 1600 with the two-call shape, 0 with one call. Buffer, then emit once.
#define println(x) do { WynOut __wl; wyn_out_begin(&__wl); wyn_out_append(&__wl, x); wyn_out_str(&__wl, "\n"); wyn_out_flush(&__wl); } while(0)

#define to_string(x) _Generic((x), \
    int: int_to_string, \
    long: int_to_string, \
    long long: int_to_string, \
    float: float_to_string, \
    double: float_to_string, \
    char*: str_to_string, \
    const char*: str_to_string, \
    bool: bool_to_string, \
    WynArray: array_to_string, \
    default: int_to_string)(x)

static inline void print_val(const char* s) { if(s) fputs(s, stdout); }
#endif
