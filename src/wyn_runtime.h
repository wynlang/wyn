#ifndef WYN_RUNTIME_H
#define WYN_RUNTIME_H
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#include <setjmp.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "wyn_interface.h"
#include "io.h"
#include "arc_runtime.h"
#include "spawn.h"
#include "gui.h"
#include "future.h"
#include "optional.h"
#include "result.h"
#include "hashmap.h"
#include "hashset.h"

// Global argc/argv for System::args()
int __wyn_argc = 0;
char** __wyn_argv = NULL;

typedef struct { void* fn; void* env; } WynClosure;
WynClosure wyn_closure_new(void* fn, void* env) { return (WynClosure){fn, env}; }
int wyn_closure_call_int(WynClosure c, int arg) { return ((int(*)(void*,int))c.fn)(c.env, arg); }

// json.h included below in Json module section
#include "async_runtime.h"

int wyn_get_argc(void);
const char* wyn_get_argv(int index);
char* wyn_read_file(const char* path);
int wyn_write_file(const char* path, const char* content);
bool wyn_file_exists(const char* path);
int wyn_store_argv(int index);
int wyn_get_filename_valid(void);
int wyn_store_file_content(const char* path);
int wyn_get_content_valid(void);
int wyn_c_init_lexer(const char* source);
void wyn_c_init_parser();
int64_t wyn_c_parse_program();
int wyn_c_get_error_count();
char* wyn_c_get_error_message(int index);
char* wyn_c_ast_to_string(int64_t ast_ptr);
void wyn_c_free_ast(int64_t ast_ptr);
void wyn_c_init_checker();
void wyn_c_check_program(int ast_ptr);
bool wyn_c_checker_had_error();
bool wyn_c_generate_code(int ast_ptr, const char* c_filename);
char* wyn_c_create_c_filename(const char* filename);
bool wyn_c_compile_to_binary(const char* c_filename, const char* wyn_filename);
bool wyn_c_remove_file(const char* filename);

const char* wyn_string_concat_safe(const char* left, const char* right);

// Test module
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
int Test_summary();

// Http module
typedef struct HttpResponse HttpResponse;
// Http_get defined below
HttpResponse* Http_post(const char* url, const char* body, const char* content_type);
int Http_status(HttpResponse* resp);
const char* Http_body(HttpResponse* resp);
const char* Http_header(HttpResponse* resp, const char* name);
void Http_free(HttpResponse* resp);

// TcpServer module
typedef struct TcpServer TcpServer;
TcpServer* TcpServer_new(int port);
int TcpServer_listen(TcpServer* server);
int TcpServer_accept(TcpServer* server);
void TcpServer_close(TcpServer* server);

// Socket module
int Socket_set_timeout(int sock, int seconds);
int Socket_set_nonblocking(int sock);
int Socket_poll_read(int sock, int timeout_ms);
char* Socket_read_line(int sock);

// Url module
char* Url_encode(const char* str);
char* Url_decode(const char* str);

// String module
int wyn_string_len(const char* str);
int wyn_string_contains(const char* str, const char* substr);
int wyn_string_starts_with(const char* str, const char* prefix);
int wyn_string_ends_with(const char* str, const char* suffix);
char* wyn_string_to_upper(const char* str);
char* wyn_string_to_lower(const char* str);
char* wyn_string_trim(const char* str);
char* wyn_str_replace(const char* str, const char* old, const char* new);
char** wyn_string_split(const char* str, const char* delim, int* count);
char* wyn_string_join(char** strings, int count, const char* delim);
char* wyn_str_substring(const char* str, int start, int end);
int wyn_string_index_of(const char* str, const char* substr);
int wyn_string_last_index_of(const char* str, const char* substr);
char* wyn_string_repeat(const char* str, int n);
char* wyn_string_reverse(const char* str);
char* wyn_string_pad_left(const char* str, int width, const char* pad_char);
char* wyn_string_pad_right(const char* str, int width, const char* pad_char);

// Json module - simple key-value JSON using parallel arrays
// Implementation in json.c, declarations in json.h
#include "json.h"
WynJson* json_new();
void json_set_string(WynJson* json, const char* key, const char* value);
void json_set_int(WynJson* json, const char* key, int value);
char* json_stringify(WynJson* json);

// Regex module - POSIX regex
#include <regex.h>
int regex_match(const char* str, const char* pattern) {
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB) != 0) return 0;
    int result = regexec(&re, str, 0, NULL, 0) == 0 ? 1 : 0;
    regfree(&re);
    return result;
}
char* regex_replace(const char* str, const char* pattern, const char* replacement) {
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED) != 0) return strdup(str);
    
    char* result = malloc(strlen(str) * 2 + strlen(replacement) + 1);
    int ri = 0;
    const char* p = str;
    regmatch_t match;
    
    while (regexec(&re, p, 1, &match, 0) == 0) {
        // Copy text before match
        memcpy(result + ri, p, match.rm_so);
        ri += match.rm_so;
        // Copy replacement
        int rlen = strlen(replacement);
        memcpy(result + ri, replacement, rlen);
        ri += rlen;
        p += match.rm_eo;
    }
    // Copy remaining
    int remaining = strlen(p);
    memcpy(result + ri, p, remaining);
    ri += remaining;
    result[ri] = '\0';
    
    regfree(&re);
    return result;
}

// Time module wrappers
long Time_now();
long long Time_now_millis();
void Time_sleep(int seconds);

// Crypto module wrappers
unsigned int Crypto_hash32(const char* data);
unsigned long long Crypto_hash64(const char* data);

// HashMap module
// HashMap_new defined below
void HashMap_insert(int map, const char* key, int value);
int HashMap_get(int map, const char* key);
int HashMap_contains(int map, const char* key);
int HashMap_len(int map);
int HashMap_remove(int map, const char* key);
void HashMap_clear(int map);
void HashMap_free(int map);
int wyn_hashmap_new();
void wyn_hashmap_insert_int(int map, const char* key, int value);
int wyn_hashmap_get_int(int map, const char* key);
int wyn_hashmap_has(int map, const char* key);
int wyn_hashmap_len(int map);
void wyn_hashmap_free(int map);

// Arena module
typedef struct WynArena WynArena;
WynArena* wyn_arena_new();
int* wyn_arena_alloc_int(WynArena* arena, int value);
void wyn_arena_clear(WynArena* arena);
void wyn_arena_free(WynArena* arena);

// Array module
int wyn_array_find(int* arr, int len, int (*pred)(int), int* found);
int wyn_array_find_index(int* arr, int len, int (*pred)(int));
int* wyn_array_unique(int* arr, int len, int* out_len);
char* wyn_array_join(int* arr, int len, const char* separator);
int wyn_array_first(int* arr, int len, int* found);
int wyn_array_last(int* arr, int len, int* found);
int wyn_array_is_empty(int* arr, int len);
int wyn_array_any(int* arr, int len, int (*pred)(int));
int wyn_array_all(int* arr, int len, int (*pred)(int));
void wyn_array_reverse(int* arr, int len);
void wyn_array_sort(int* arr, int len);
int wyn_array_contains(int* arr, int len, int value);
int* wyn_array_slice(int* arr, int start, int end, int* out_len);
int wyn_array_index_of(int* arr, int len, int value);
int wyn_array_last_index_of(int* arr, int len, int value);
int* wyn_array_concat(int* arr1, int len1, int* arr2, int len2, int* out_len);
void wyn_array_fill(int* arr, int len, int value);
int wyn_array_sum(int* arr, int len);
int wyn_array_min(int* arr, int len);
int wyn_array_max(int* arr, int len);
double wyn_array_average(int* arr, int len);

// Time module
long wyn_time_now();
long long wyn_time_now_millis();
long long wyn_time_now_micros();
void wyn_time_sleep(int seconds);
void wyn_time_sleep_millis(int millis);
void wyn_time_sleep_micros(int micros);
char* wyn_time_format(long timestamp);
long wyn_time_parse(const char* str);
int wyn_time_year(long timestamp);
int wyn_time_month(long timestamp);
int wyn_time_day(long timestamp);
int wyn_time_hour(long timestamp);
int wyn_time_minute(long timestamp);
int wyn_time_second(long timestamp);

// Crypto module
uint32_t wyn_crypto_hash32(const char* data, size_t len);
uint64_t wyn_crypto_hash64(const char* data, size_t len);
void wyn_crypto_md5(const char* data, size_t len, char* output);
void wyn_crypto_sha256(const char* data, size_t len, char* output);
char* wyn_crypto_base64_encode(const char* data, size_t len);
char* wyn_crypto_base64_decode(const char* data, size_t* out_len);
void wyn_crypto_random_bytes(char* buffer, size_t len);
char* wyn_crypto_random_hex(size_t len);
char* wyn_crypto_xor_cipher(const char* data, size_t len, const char* key, size_t key_len);

// Math module
double wyn_math_abs(double x);
double wyn_math_min(double a, double b);
double wyn_math_max(double a, double b);
double wyn_math_pow(double base, double exp);
double wyn_math_sqrt(double x);
double wyn_math_floor(double x);
double wyn_math_ceil(double x);
double wyn_math_round(double x);

extern char* global_filename;
extern char* global_file_content;

jmp_buf* current_exception_buf = NULL;
const char** current_exception_msg = NULL;

typedef struct { void** keys; void** values; int count; } WynMap;

typedef struct {
    WynTypeId type;
    union {
        int int_val;
        double float_val;
        const char* string_val;
        struct WynArray* array_val;
        void* struct_val;
    } data;
} WynValue;

typedef struct WynArray { WynValue* data; int count; int capacity; } WynArray;
WynArray wyn_array_map(WynArray arr, long long (*fn)(long long));
WynArray wyn_array_filter(WynArray arr, long long (*fn)(long long));
long long wyn_array_reduce(WynArray arr, long long (*fn)(long long, long long), long long initial);
WynArray array_new() { WynArray arr = {0}; return arr; }
void array_push_int(WynArray* arr, int value) {
    if (arr->count >= arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 4 : arr->capacity * 2;
        arr->data = realloc(arr->data, sizeof(WynValue) * arr->capacity);
    }
    arr->data[arr->count].type = WYN_TYPE_INT;
    arr->data[arr->count].data.int_val = value;
    arr->count++;
}
void array_push_str(WynArray* arr, const char* value) {
    if (arr->count >= arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 4 : arr->capacity * 2;
        arr->data = realloc(arr->data, sizeof(WynValue) * arr->capacity);
    }
    arr->data[arr->count].type = WYN_TYPE_STRING;
    arr->data[arr->count].data.string_val = value;
    arr->count++;
}
void array_push_array(WynArray* arr, WynArray* nested) {
    if (arr->count >= arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 4 : arr->capacity * 2;
        arr->data = realloc(arr->data, sizeof(WynValue) * arr->capacity);
    }
    arr->data[arr->count].type = WYN_TYPE_ARRAY;
    arr->data[arr->count].data.array_val = nested;
    arr->count++;
}
int array_get_int(WynArray arr, int index) {
    if (index < 0 || index >= arr.count) {
        fprintf(stderr, "Warning: Array index out of bounds: %d (array size: %d)\n", index, arr.count);
        return 0;
    }
    if (arr.data[index].type == WYN_TYPE_INT) return arr.data[index].data.int_val;
    return 0;
}
const char* array_get_str(WynArray arr, int index) {
    if (index < 0 || index >= arr.count) {
        fprintf(stderr, "Warning: Array index out of bounds: %d (array size: %d)\n", index, arr.count);
        return "";
    }
    if (arr.data[index].type == WYN_TYPE_STRING) return arr.data[index].data.string_val;
    return "";
}
#define array_get_struct(arr, idx, T) (*(T*)arr.data[idx].data.struct_val)
WynValue array_get(WynArray arr, int index) {
    WynValue val = {0};
    if (index >= 0 && index < arr.count) val = arr.data[index];
    return val;
}
#define ARRAY_GET_STR(arr, idx) (array_get(arr, idx).data.string_val)
#define ARRAY_GET_INT(arr, idx) (array_get(arr, idx).data.int_val)
WynArray* array_get_array(WynArray arr, int index) {
    if (index < 0 || index >= arr.count) return NULL;
    if (arr.data[index].type == WYN_TYPE_ARRAY) return arr.data[index].data.array_val;
    return NULL;
}
int array_get_nested_int(WynArray arr, int index1, int index2) {
    WynArray* nested = array_get_array(arr, index1);
    if (nested == NULL) return 0;
    return array_get_int(*nested, index2);
}
int array_get_nested3_int(WynArray arr, int index1, int index2, int index3) {
    WynArray* nested1 = array_get_array(arr, index1);
    if (nested1 == NULL) return 0;
    WynArray* nested2 = array_get_array(*nested1, index2);
    if (nested2 == NULL) return 0;
    return array_get_int(*nested2, index3);
}

int array_len(WynArray arr) { return arr.count; }
bool array_is_empty(WynArray arr) { return arr.count == 0; }
bool array_contains(WynArray arr, int value) {
    for (int i = 0; i < arr.count; i++) {
        if (arr.data[i].type == WYN_TYPE_INT && arr.data[i].data.int_val == value) return true;
    }
    return false;
}
bool array_contains_str(WynArray arr, const char* value) {
    for (int i = 0; i < arr.count; i++) {
        if (arr.data[i].type == WYN_TYPE_STRING && strcmp(arr.data[i].data.string_val, value) == 0) return true;
    }
    return false;
}
int array_index_of_str(WynArray arr, const char* value) {
    for (int i = 0; i < arr.count; i++) {
        if (arr.data[i].type == WYN_TYPE_STRING && strcmp(arr.data[i].data.string_val, value) == 0) return i;
    }
    return -1;
}
void array_remove_str(WynArray* arr, const char* value) {
    for (int i = 0; i < arr->count; i++) {
        if (arr->data[i].type == WYN_TYPE_STRING && strcmp(arr->data[i].data.string_val, value) == 0) {
            for (int j = i; j < arr->count - 1; j++) arr->data[j] = arr->data[j + 1];
            arr->count--;
            return;
        }
    }
}
void array_push(WynArray* arr, int value) {
    if (arr->count >= arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 4 : arr->capacity * 2;
        arr->data = realloc(arr->data, sizeof(WynValue) * arr->capacity);
    }
    arr->data[arr->count].type = WYN_TYPE_INT;
    arr->data[arr->count].data.int_val = value;
    arr->count++;
}
#define array_push_struct(arr, value, StructType) do { \
    StructType __temp_val = (value); \
    if ((arr)->count >= (arr)->capacity) { \
        (arr)->capacity = (arr)->capacity == 0 ? 4 : (arr)->capacity * 2; \
        (arr)->data = realloc((arr)->data, sizeof(WynValue) * (arr)->capacity); \
    } \
    (arr)->data[(arr)->count].type = WYN_TYPE_STRUCT; \
    (arr)->data[(arr)->count].data.struct_val = malloc(sizeof(StructType)); \
    memcpy((arr)->data[(arr)->count].data.struct_val, &__temp_val, sizeof(StructType)); \
    (arr)->count++; \
} while(0)
int array_pop(WynArray* arr) {
    if (arr->count == 0) return 0;
    arr->count--;
    return arr->data[arr->count].data.int_val;
}
int array_index_of(WynArray arr, int value) {
    for (int i = 0; i < arr.count; i++) {
        if (arr.data[i].type == WYN_TYPE_INT && arr.data[i].data.int_val == value) return i;
    }
    return -1;
}
void array_reverse(WynArray* arr) {
    for (int i = 0; i < arr->count / 2; i++) {
        WynValue temp = arr->data[i];
        arr->data[i] = arr->data[arr->count - 1 - i];
        arr->data[arr->count - 1 - i] = temp;
    }
}
void array_sort(WynArray* arr) {
    for (int i = 0; i < arr->count - 1; i++) {
        for (int j = 0; j < arr->count - i - 1; j++) {
            if (arr->data[j].data.int_val > arr->data[j + 1].data.int_val) {
                WynValue temp = arr->data[j];
                arr->data[j] = arr->data[j + 1];
                arr->data[j + 1] = temp;
            }
        }
    }
}

int array_first(WynArray arr) {
    if (arr.count == 0) return 0;
    return array_get_int(arr, 0);
}
int array_last(WynArray arr) {
    if (arr.count == 0) return 0;
    return array_get_int(arr, arr.count - 1);
}
int array_count(WynArray arr, int value) {
    int count = 0;
    for (int i = 0; i < arr.count; i++) {
        if (array_get_int(arr, i) == value) count++;
    }
    return count;
}
void array_clear(WynArray* arr) {
    arr->count = 0;
}
int array_min(WynArray arr) {
    if (arr.count == 0) return 0;
    int min = array_get_int(arr, 0);
    for (int i = 1; i < arr.count; i++) {
        int val = array_get_int(arr, i);
        if (val < min) min = val;
    }
    return min;
}
int array_max(WynArray arr) {
    if (arr.count == 0) return 0;
    int max = array_get_int(arr, 0);
    for (int i = 1; i < arr.count; i++) {
        int val = array_get_int(arr, i);
        if (val > max) max = val;
    }
    return max;
}
int array_sum(WynArray arr) {
    int sum = 0;
    for (int i = 0; i < arr.count; i++) {
        sum += array_get_int(arr, i);
    }
    return sum;
}
int array_average(WynArray arr) {
    if (arr.count == 0) return 0;
    return array_sum(arr) / arr.count;
}
void array_remove_value(WynArray* arr, int value) {
    int write_idx = 0;
    for (int i = 0; i < arr->count; i++) {
        if (array_get_int(*arr, i) != value) {
            arr->data[write_idx++] = arr->data[i];
        }
    }
    arr->count = write_idx;
}
void array_remove_at(WynArray* arr, int index) {
    if (index < 0 || index >= arr->count) return;
    for (int i = index; i < arr->count - 1; i++) {
        arr->data[i] = arr->data[i+1];
    }
    arr->count--;
}
void array_insert(WynArray* arr, int index, int value) {
    if (index < 0 || index > arr->count) return;
    if (arr->count >= arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 4 : arr->capacity * 2;
        arr->data = realloc(arr->data, sizeof(WynValue) * arr->capacity);
    }
    for (int i = arr->count; i > index; i--) {
        arr->data[i] = arr->data[i-1];
    }
    arr->data[index].type = WYN_TYPE_INT;
    arr->data[index].data.int_val = value;
    arr->count++;
}
WynArray array_take(WynArray arr, int n) {
    WynArray result = array_new();
    int count = (n < arr.count) ? n : arr.count;
    for (int i = 0; i < count; i++) {
        if (result.count >= result.capacity) {
            result.capacity = result.capacity == 0 ? 4 : result.capacity * 2;
            result.data = realloc(result.data, sizeof(WynValue) * result.capacity);
        }
        result.data[result.count++] = arr.data[i];
    }
    return result;
}
WynArray array_skip(WynArray arr, int n) {
    WynArray result = array_new();
    for (int i = n; i < arr.count; i++) {
        if (result.count >= result.capacity) {
            result.capacity = result.capacity == 0 ? 4 : result.capacity * 2;
            result.data = realloc(result.data, sizeof(WynValue) * result.capacity);
        }
        result.data[result.count++] = arr.data[i];
    }
    return result;
}
WynArray wyn_array_slice_range(WynArray arr, int start, int end) {
    WynArray result = array_new();
    if (start < 0) start = 0;
    if (end > arr.count) end = arr.count;
    for (int i = start; i < end; i++) {
        if (result.count >= result.capacity) {
            result.capacity = result.capacity == 0 ? 4 : result.capacity * 2;
            result.data = realloc(result.data, sizeof(WynValue) * result.capacity);
        }
        result.data[result.count++] = arr.data[i];
    }
    return result;
}
char* array_join(WynArray arr, const char* sep) {
    if (arr.count == 0) return "";
    int total_len = 0;
    int sep_len = strlen(sep);
    for (int i = 0; i < arr.count; i++) {
        if (arr.data[i].type == WYN_TYPE_STRING) {
            total_len += strlen(arr.data[i].data.string_val);
        } else if (arr.data[i].type == WYN_TYPE_INT) {
            total_len += 12;
        }
        if (i < arr.count - 1) total_len += sep_len;
    }
    char* result = malloc(total_len + 1);
    result[0] = '\0';
    for (int i = 0; i < arr.count; i++) {
        if (arr.data[i].type == WYN_TYPE_STRING) {
            strcat(result, arr.data[i].data.string_val);
        } else if (arr.data[i].type == WYN_TYPE_INT) {
            char buf[16]; sprintf(buf, "%d", arr.data[i].data.int_val);
            strcat(result, buf);
        }
        if (i < arr.count - 1) strcat(result, sep);
    }
    return result;
}
WynArray array_concat(WynArray arr1, WynArray arr2) {
    WynArray result = array_new();
    for (int i = 0; i < arr1.count; i++) {
        if (result.count >= result.capacity) {
            result.capacity = result.capacity == 0 ? 4 : result.capacity * 2;
            result.data = realloc(result.data, sizeof(WynValue) * result.capacity);
        }
        result.data[result.count++] = arr1.data[i];
    }
    for (int i = 0; i < arr2.count; i++) {
        if (result.count >= result.capacity) {
            result.capacity = result.capacity == 0 ? 4 : result.capacity * 2;
            result.data = realloc(result.data, sizeof(WynValue) * result.capacity);
        }
        result.data[result.count++] = arr2.data[i];
    }
    return result;
}

typedef struct { int start; int end; int current; } WynRange;
WynRange range(int start, int end) {
    WynRange r = {start, end, start};
    return r;
}
bool range_has_next(WynRange* r) { return r->current < r->end; }
int range_next(WynRange* r) { return r->current++; }

int string_length(const char* str) { return strlen(str); }
char* string_substring(const char* str, int start, int end) {
    int len = end - start;
    char* result = malloc(len + 1);
    strncpy(result, str + start, len);
    result[len] = '\0';
    return result;
}
int string_contains(const char* str, const char* substr) {
    return strstr(str, substr) != NULL;
}
char* string_concat(const char* a, const char* b) {
    int len_a = strlen(a), len_b = strlen(b);
    char* result = malloc(len_a + len_b + 1);
    strcpy(result, a);
    strcat(result, b);
    return result;
}
char* string_upper(const char* str) {
    int len = strlen(str);
    char* result = malloc(len + 1);
    for (int i = 0; i < len; i++) {
        result[i] = toupper(str[i]);
    }
    result[len] = '\0';
    return result;
}
char* string_lower(const char* str) {
    int len = strlen(str);
    char* result = malloc(len + 1);
    for (int i = 0; i < len; i++) {
        result[i] = tolower(str[i]);
    }
    result[len] = '\0';
    return result;
}
int string_is_alpha(const char* str) {
    if (!str || !*str) return 0;
    for (int i = 0; str[i]; i++) {
        if (!isalpha(str[i])) return 0;
    }
    return 1;
}
int string_is_digit(const char* str) {
    if (!str || !*str) return 0;
    for (int i = 0; str[i]; i++) {
        if (!isdigit(str[i])) return 0;
    }
    return 1;
}
int string_is_alnum(const char* str) {
    if (!str || !*str) return 0;
    for (int i = 0; str[i]; i++) {
        if (!isalnum(str[i])) return 0;
    }
    return 1;
}
int string_is_whitespace(const char* str) {
    if (!str || !*str) return 0;
    for (int i = 0; str[i]; i++) {
        if (!isspace(str[i])) return 0;
    }
    return 1;
}
const char* string_char_at(const char* str, int index) {
    int len = strlen(str);
    if (index < 0 || index >= len) return "";
    char* result = malloc(2);
    result[0] = str[index];
    result[1] = '\0';
    return result;
}
int string_equals(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}
int string_count(const char* str, const char* substr) {
    int count = 0;
    int substr_len = strlen(substr);
    if (substr_len == 0) return 0;
    const char* pos = str;
    while ((pos = strstr(pos, substr)) != NULL) {
        count++;
        pos += substr_len;
    }
    return count;
}
int string_is_numeric(const char* str) {
    if (!str || !*str) return 0;
    int has_dot = 0;
    int i = 0;
    if (str[0] == '-' || str[0] == '+') i = 1;
    if (!str[i]) return 0;
    for (; str[i]; i++) {
        if (str[i] == '.') {
            if (has_dot) return 0;
            has_dot = 1;
        } else if (!isdigit(str[i])) {
            return 0;
        }
    }
    return 1;
}
char* string_capitalize(const char* str) {
    int len = strlen(str);
    char* result = malloc(len + 1);
    if (len > 0) result[0] = toupper(str[0]);
    for (int i = 1; i < len; i++) result[i] = tolower(str[i]);
    result[len] = '\0';
    return result;
}
char* string_reverse(const char* str) {
    int len = strlen(str);
    char* result = malloc(len + 1);
    for (int i = 0; i < len; i++) result[i] = str[len - 1 - i];
    result[len] = '\0';
    return result;
}
int string_len(const char* str) { return strlen(str); }
int string_is_empty(const char* str) { return str[0] == '\0'; }
int string_starts_with(const char* str, const char* prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}
int string_ends_with(const char* str, const char* suffix) {
    int str_len = strlen(str);
    int suffix_len = strlen(suffix);
    if (suffix_len > str_len) return 0;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}
int string_index_of(const char* str, const char* substr) {
    const char* pos = strstr(str, substr);
    return pos ? (int)(pos - str) : -1;
}
char* string_replace(const char* str, const char* old, const char* new) {
    int count = 0;
    const char* p = str;
    int old_len = strlen(old);
    while ((p = strstr(p, old))) { count++; p += old_len; }
    int new_len = strlen(new);
    int result_len = strlen(str) + count * (new_len - old_len);
    char* result = malloc(result_len + 1);
    char* r = result;
    p = str;
    while (*p) {
        const char* match = strstr(p, old);
        if (match) {
            int len = match - p;
            memcpy(r, p, len); r += len;
            memcpy(r, new, new_len); r += new_len;
            p = match + old_len;
        } else {
            strcpy(r, p); break;
        }
    }
    result[result_len] = '\0';
    return result;
}
char* string_replace_all(const char* str, const char* old, const char* new) {
    return string_replace(str, old, new);
}
int string_last_index_of(const char* str, const char* substr) {
    const char* last = NULL;
    const char* p = str;
    while ((p = strstr(p, substr))) { last = p; p++; }
    return last ? (int)(last - str) : -1;
}
char* string_slice(const char* str, int start, int end) {
    int len = strlen(str);
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (start >= end) return strdup("");
    int slice_len = end - start;
    char* result = malloc(slice_len + 1);
    memcpy(result, str + start, slice_len);
    result[slice_len] = '\0';
    return result;
}
char* string_repeat(const char* str, int count) {
    int len = strlen(str);
    char* result = malloc(len * count + 1);
    for (int i = 0; i < count; i++) memcpy(result + i * len, str, len);
    result[len * count] = '\0';
    return result;
}
char* string_title(const char* str) {
    int len = strlen(str);
    char* result = malloc(len + 1);
    int capitalize_next = 1;
    for (int i = 0; i < len; i++) {
        if (str[i] == ' ' || str[i] == '\t' || str[i] == '\n') {
            result[i] = str[i];
            capitalize_next = 1;
        } else if (capitalize_next) {
            result[i] = toupper(str[i]);
            capitalize_next = 0;
        } else {
            result[i] = tolower(str[i]);
        }
    }
    result[len] = '\0';
    return result;
}
char* string_trim_left(const char* str) {
    while (*str == ' ' || *str == '\t' || *str == '\n') str++;
    return strdup(str);
}
char* string_trim_right(const char* str) {
    int len = strlen(str);
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\t' || str[len-1] == '\n')) len--;
    char* result = malloc(len + 1);
    memcpy(result, str, len);
    result[len] = '\0';
    return result;
}
char* string_trim(const char* str) {
    while (*str == ' ' || *str == '\t' || *str == '\n') str++;
    int len = strlen(str);
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\t' || str[len-1] == '\n')) len--;
    char* result = malloc(len + 1);
    memcpy(result, str, len);
    result[len] = '\0';
    return result;
}
WynArray string_split(const char* str, const char* delim) {
    WynArray arr = array_new();
    int dlen = strlen(delim);
    const char* p = str;
    while (1) {
        const char* found = strstr(p, delim);
        if (!found) {
            array_push_str(&arr, strdup(p));
            break;
        }
        int len = found - p;
        char* seg = malloc(len + 1);
        memcpy(seg, p, len);
        seg[len] = 0;
        array_push_str(&arr, seg);
        p = found + dlen;
    }
    return arr;
}
const char* wyn_string_charat(const char* str, int index) {
    int len = strlen(str);
    if (index < 0 || index >= len) return "";
    char* result = malloc(2);
    result[0] = str[index];
    result[1] = '\0';
    return result;
}
WynArray string_chars(const char* str) {
    WynArray arr = array_new();
    for (int i = 0; str[i] != '\0'; i++) {
        char* ch = malloc(2);
        ch[0] = str[i];
        ch[1] = '\0';
        array_push_str(&arr, ch);
    }
    return arr;
}
WynArray string_to_bytes(const char* str) {
    WynArray arr = array_new();
    for (int i = 0; str[i] != '\0'; i++) {
        array_push_int(&arr, (int)(unsigned char)str[i]);
    }
    return arr;
}
char* string_pad_left(const char* str, int width, const char* pad) {
    int len = strlen(str);
    if (len >= width) return strdup(str);
    int pad_len = width - len;
    char* result = malloc(width + 1);
    for (int i = 0; i < pad_len; i++) result[i] = pad[0];
    strcpy(result + pad_len, str);
    return result;
}
char* string_pad_right(const char* str, int width, const char* pad) {
    int len = strlen(str);
    if (len >= width) return strdup(str);
    int pad_len = width - len;
    char* result = malloc(width + 1);
    strcpy(result, str);
    for (int i = len; i < width; i++) result[i] = pad[0];
    result[width] = '\0';
    return result;
}

WynArray string_lines(const char* str) {
    WynArray arr = array_new();
    char* copy = strdup(str);
    char* token = strtok(copy, "\n");
    while (token != NULL) {
        array_push_str(&arr, strdup(token));
        token = strtok(NULL, "\n");
    }
    free(copy);
    return arr;
}
WynArray string_words(const char* str) {
    WynArray arr = array_new();
    char* copy = strdup(str);
    char* token = strtok(copy, " \t\n\r");
    while (token != NULL) {
        array_push_str(&arr, strdup(token));
        token = strtok(NULL, " \t\n\r");
    }
    free(copy);
    return arr;
}

void set_clear(WynHashSet* set) {
    wyn_hashset_clear(set);
}
WynHashSet* set_union(WynHashSet* set1, WynHashSet* set2) {
    return wyn_hashset_union(set1, set2);
}
WynHashSet* set_intersection(WynHashSet* set1, WynHashSet* set2) {
    return wyn_hashset_intersection(set1, set2);
}
WynHashSet* set_difference(WynHashSet* set1, WynHashSet* set2) {
    return wyn_hashset_difference(set1, set2);
}
bool set_is_subset(WynHashSet* set1, WynHashSet* set2) {
    return wyn_hashset_is_subset(set1, set2);
}
bool set_is_superset(WynHashSet* set1, WynHashSet* set2) {
    return wyn_hashset_is_subset(set2, set1);
}
bool set_is_disjoint(WynHashSet* set1, WynHashSet* set2) {
    return wyn_hashset_is_disjoint(set1, set2);
}

double int_to_float(int n) { return (double)n; }
int int_abs(int n) { return n < 0 ? -n : n; }
int int_pow(int base, int exp) {
    int result = 1;
    for (int i = 0; i < exp; i++) result *= base;
    return result;
}
int int_min(int a, int b) { return a < b ? a : b; }
int int_max(int a, int b) { return a > b ? a : b; }
int int_clamp(int n, int min, int max) {
    if (n < min) return min;
    if (n > max) return max;
    return n;
}
int int_is_even(int n) { return n % 2 == 0; }
int int_is_odd(int n) { return n % 2 != 0; }
int int_is_positive(int n) { return n > 0; }
int int_is_negative(int n) { return n < 0; }
int int_is_zero(int n) { return n == 0; }
int int_sign(int n) { return (n > 0) - (n < 0); }
char* int_to_binary(int n) {
    if (n == 0) return "0";
    char* result = malloc(33);
    int i = 0;
    unsigned int num = (unsigned int)n;
    while (num > 0) {
        result[i++] = (num % 2) + '0';
        num /= 2;
    }
    result[i] = '\0';
    for (int j = 0; j < i/2; j++) {
        char temp = result[j];
        result[j] = result[i-1-j];
        result[i-1-j] = temp;
    }
    return result;
}
char* int_to_hex(int n) {
    char* result = malloc(12);
    sprintf(result, "%x", n);
    return result;
}

long long float_to_int(double f) { return (long long)f; }
double float_round(double f) { return round(f); }
double float_round_to(double f, int decimals) { double m = pow(10, decimals); return round(f * m) / m; }
double float_floor(double f) { return floor(f); }
double float_ceil(double f) { return ceil(f); }
double float_abs(double f) { return fabs(f); }
double float_pow(double base, double exp) { return pow(base, exp); }
double float_sqrt(double f) { return sqrt(f); }
double float_min(double a, double b) { return a < b ? a : b; }
double float_max(double a, double b) { return a > b ? a : b; }
double float_clamp(double f, double min, double max) {
    if (f < min) return min;
    if (f > max) return max;
    return f;
}
int float_is_nan(double f) { return isnan(f); }
int float_is_infinite(double f) { return isinf(f); }
int float_is_finite(double f) { return isfinite(f); }
int float_is_positive(double f) { return f > 0.0; }
int float_is_negative(double f) { return f < 0.0; }
double float_sign(double f) { return (f > 0.0) - (f < 0.0); }
double float_sin(double f) { return sin(f); }
double float_cos(double f) { return cos(f); }
double float_tan(double f) { return tan(f); }
double float_asin(double f) { return asin(f); }
double float_acos(double f) { return acos(f); }
double float_atan(double f) { return atan(f); }
double float_log(double f) { return log(f); }
double float_log10(double f) { return log10(f); }
double float_log2(double f) { return log2(f); }
double float_exp(double f) { return exp(f); }

int map_get(WynMap map, const char* key) {
    for (int i = 0; i < map.count; i++) {
        if (strcmp((char*)map.keys[i], key) == 0) {
            return (int)(intptr_t)map.values[i];
        }
    }
    return 0; // Not found
}

void map_set(WynMap* map, const char* key, int value) {
    // Check if key exists
    for (int i = 0; i < map->count; i++) {
        if (strcmp((char*)map->keys[i], key) == 0) {
            map->values[i] = (void*)(intptr_t)value;
            return;
        }
    }
    // Add new key-value pair
    map->keys = realloc(map->keys, sizeof(void*) * (map->count + 1));
    map->values = realloc(map->values, sizeof(void*) * (map->count + 1));
    map->keys[map->count] = (void*)strdup(key);
    map->values[map->count] = (void*)(intptr_t)value;
    map->count++;
}

void map_clear(WynMap* map) {
    if (map->keys) {
        for (int i = 0; i < map->count; i++) {
            free(map->keys[i]); // Free strdup'd keys
        }
        free(map->keys);
        free(map->values);
    }
    map->keys = NULL;
    map->values = NULL;
    map->count = 0;
}

char** map_keys(WynMap map) {
    char** keys = malloc(sizeof(char*) * (map.count + 1));
    for (int i = 0; i < map.count; i++) {
        keys[i] = (char*)map.keys[i];
    }
    keys[map.count] = NULL; // Null terminate
    return keys;
}

int map_get_or_default(WynHashMap* map, const char* key, int default_value) {
    if (!hashmap_has(map, key)) return default_value;
    return hashmap_get_int(map, key);
}
void map_merge(WynHashMap* dest, WynHashMap* src) {
    // Merge src into dest by iterating all buckets
    for (int i = 0; i < 128; i++) {
        void* entry = ((void**)src)[i];
        while (entry) {
            char* key = *(char**)entry;
            int value = *((int*)((char*)entry + sizeof(char*)));
            hashmap_insert_int(dest, key, value);
            entry = *((void**)((char*)entry + sizeof(char*) + sizeof(int)));
        }
    }
}
int map_len(WynHashMap* map) {
    int count = 0;
    for (int i = 0; i < 128; i++) {
        void* entry = ((void**)map)[i];
        while (entry) {
            count++;
            entry = *((void**)((char*)entry + sizeof(char*) + sizeof(int)));
        }
    }
    return count;
}
bool map_is_empty(WynHashMap* map) {
    for (int i = 0; i < 128; i++) {
        if (((void**)map)[i] != NULL) return false;
    }
    return true;
}
bool map_has(WynHashMap* map, const char* key) {
    return hashmap_has(map, key);
}
void map_remove(WynHashMap* map, const char* key) {
    hashmap_remove(map, key);
}

struct HttpResponse { char* body; int status; size_t size; };
char* http_headers[32] = {NULL};
int http_header_count = 0;
int http_last_status = 0;
char http_last_error[256] = {0};
char last_error[256] = {0};

char* http_request(const char* method, const char* url, const char* body) {
    char hostname[256], path[1024];
    int port = 80, is_https = 0;
    http_last_error[0] = 0;
    
    // Parse URL
    if(strncmp(url, "https://", 8) == 0) { url += 8; port = 443; is_https = 1; }
    else if(strncmp(url, "http://", 7) == 0) url += 7;
    
    const char* slash = strchr(url, '/');
    if(slash) {
        int len = slash - url;
        if(len >= 256) len = 255;
        strncpy(hostname, url, len);
        hostname[len] = 0;
        strncpy(path, slash, 1023);
        path[1023] = 0;
    } else {
        strncpy(hostname, url, 255);
        hostname[255] = 0;
        strcpy(path, "/");
    }
    
    // Check for port
    char* colon = strchr(hostname, ':');
    if(colon) { *colon = 0; port = atoi(colon + 1); }
    
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) { snprintf(http_last_error, 256, "Socket creation failed"); return NULL; }
    
    // Set timeout
    struct timeval tv = {30, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    // Resolve hostname
    struct hostent* server = gethostbyname(hostname);
    if(!server) { close(sock); snprintf(http_last_error, 256, "Host not found: %s", hostname); return NULL; }
    
    // Connect
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    addr.sin_port = htons(port);
    
    if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock); snprintf(http_last_error, 256, "Connection failed"); return NULL;
    }
    
    // HTTPS warning (TLS not implemented yet)
    if(is_https) {
        close(sock);
        snprintf(http_last_error, 256, "HTTPS not supported yet - use http:// instead");
        return NULL;
    }
    
    // Build request
    char* request = malloc(8192);
    int len = snprintf(request, 8192, "%s %s HTTP/1.1\r\nHost: %s\r\n", method, path, hostname);
    len += snprintf(request + len, 8192 - len, "User-Agent: Wyn/1.4\r\n");
    len += snprintf(request + len, 8192 - len, "Accept: */*\r\n");
    
    // Add custom headers
    for(int i = 0; i < http_header_count; i++) {
        len += snprintf(request + len, 8192 - len, "%s\r\n", http_headers[i]);
    }
    
    // Add body
    if(body) {
        len += snprintf(request + len, 8192 - len, "Content-Length: %d\r\n", (int)strlen(body));
        len += snprintf(request + len, 8192 - len, "Content-Type: application/x-www-form-urlencoded\r\n");
    }
    
    len += snprintf(request + len, 8192 - len, "Connection: close\r\n\r\n");
    if(body) len += snprintf(request + len, 8192 - len, "%s", body);
    
    // Send
    if(send(sock, request, len, 0) < 0) {
        free(request); close(sock);
        snprintf(http_last_error, 256, "Send failed");
        return NULL;
    }
    free(request);
    
    // Read response with dynamic allocation
    size_t capacity = 65536, total = 0;
    char* response = malloc(capacity);
    int n;
    while((n = recv(sock, response + total, capacity - total - 1, 0)) > 0) {
        total += n;
        if(total >= capacity - 1024) {
            capacity *= 2;
            char* new_resp = realloc(response, capacity);
            if(!new_resp) { free(response); close(sock); return NULL; }
            response = new_resp;
        }
    }
    response[total] = 0;
    close(sock);
    
    if(total == 0) {
        free(response);
        snprintf(http_last_error, 256, "Empty response");
        return NULL;
    }
    
    // Parse status
    http_last_status = 0;
    if(strncmp(response, "HTTP/", 5) == 0) {
        char* space = strchr(response, ' ');
        if(space) http_last_status = atoi(space + 1);
    }
    
    // Find body
    char* body_start = strstr(response, "\r\n\r\n");
    if(body_start) {
        body_start += 4;
        
        // Handle chunked transfer encoding
        if(strstr(response, "Transfer-Encoding: chunked") || strstr(response, "transfer-encoding: chunked")) {
            char* result = malloc(capacity);
            char* dst = result;
            char* src = body_start;
            while(*src) {
                int chunk_size;
                if(sscanf(src, "%x", &chunk_size) != 1) break;
                if(chunk_size == 0) break;
                src = strchr(src, '\n');
                if(!src) break;
                src++;
                memcpy(dst, src, chunk_size);
                dst += chunk_size;
                src += chunk_size + 2;
            }
            *dst = 0;
            free(response);
            return result;
        }
        
        char* result = malloc(strlen(body_start) + 1);
        strcpy(result, body_start);
        free(response);
        return result;
    }
    
    return response;
}

char* https_get(const char* url) {
    // POSIX HTTPS via openssl s_client (no curl dependency)
    char hostname[256], path[1024];
    const char* p = url + 8; // skip "https://"
    const char* slash = strchr(p, '/');
    if (slash) {
        int hlen = slash - p; if (hlen > 255) hlen = 255;
        memcpy(hostname, p, hlen); hostname[hlen] = 0;
        strncpy(path, slash, 1023); path[1023] = 0;
    } else {
        strncpy(hostname, p, 255); hostname[255] = 0;
        strcpy(path, "/");
    }
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
        "printf 'GET %s HTTP/1.1\\r\\nHost: %s\\r\\nConnection: close\\r\\n\\r\\n' | "
        "openssl s_client -quiet -connect %s:443 2>/dev/null",
        path, hostname, hostname);
    FILE* fp = popen(cmd, "r");
    if (!fp) return "";
    char* response = malloc(131072);
    size_t len = fread(response, 1, 131071, fp);
    response[len] = 0;
    pclose(fp);
    // Skip HTTP headers — find \r\n\r\n
    char* body = strstr(response, "\r\n\r\n");
    if (body) {
        body += 4;
        char* result = strdup(body);
        free(response);
        return result;
    }
    return response;
}
char* https_post(const char* url, const char* data) {
    char hostname[256], path[1024];
    const char* p = url + 8;
    const char* slash = strchr(p, '/');
    if (slash) {
        int hlen = slash - p; if (hlen > 255) hlen = 255;
        memcpy(hostname, p, hlen); hostname[hlen] = 0;
        strncpy(path, slash, 1023); path[1023] = 0;
    } else {
        strncpy(hostname, p, 255); hostname[255] = 0;
        strcpy(path, "/");
    }
    int dlen = data ? (int)strlen(data) : 0;
    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
        "printf 'POST %s HTTP/1.1\\r\\nHost: %s\\r\\nContent-Length: %d\\r\\nContent-Type: application/x-www-form-urlencoded\\r\\nConnection: close\\r\\n\\r\\n%s' | "
        "openssl s_client -quiet -connect %s:443 2>/dev/null",
        path, hostname, dlen, data ? data : "", hostname);
    FILE* fp = popen(cmd, "r");
    if (!fp) return "";
    char* response = malloc(131072);
    size_t len = fread(response, 1, 131071, fp);
    response[len] = 0;
    pclose(fp);
    char* body = strstr(response, "\r\n\r\n");
    if (body) {
        body += 4;
        char* result = strdup(body);
        free(response);
        return result;
    }
    return response;
}

char* http_get(const char* url) {
    char* result;
    if (strncmp(url, "https://", 8) == 0) result = https_get(url);
    else result = http_request("GET", url, NULL);
    return result ? result : "";
}
char* http_post(const char* url, const char* data) {
    char* result;
    if (strncmp(url, "https://", 8) == 0) result = https_post(url, data);
    else result = http_request("POST", url, data);
    return result ? result : "";
}
char* http_put(const char* url, const char* data) {
    if (strncmp(url, "https://", 8) == 0) {
        // POSIX HTTPS PUT via openssl
        char hostname[256], path[1024];
        const char* p = url + 8;
        const char* slash = strchr(p, '/');
        if (slash) { int h = slash-p; if(h>255)h=255; memcpy(hostname,p,h); hostname[h]=0; strncpy(path,slash,1023); path[1023]=0; }
        else { strncpy(hostname,p,255); hostname[255]=0; strcpy(path,"/"); }
        int dlen = data ? (int)strlen(data) : 0;
        char cmd[8192];
        snprintf(cmd, sizeof(cmd),
            "printf 'PUT %s HTTP/1.1\\r\\nHost: %s\\r\\nContent-Length: %d\\r\\nConnection: close\\r\\n\\r\\n%s' | openssl s_client -quiet -connect %s:443 2>/dev/null",
            path, hostname, dlen, data?data:"", hostname);
        FILE* fp = popen(cmd, "r"); if (!fp) return "";
        char* resp = malloc(131072); size_t len = fread(resp,1,131071,fp); resp[len]=0; pclose(fp);
        char* body = strstr(resp, "\r\n\r\n");
        if (body) { body+=4; char* r=strdup(body); free(resp); return r; }
        return resp;
    }
    char* result = http_request("PUT", url, data);
    return result ? result : "";
}
char* http_delete(const char* url) {
    if (strncmp(url, "https://", 8) == 0) {
        char hostname[256], path[1024];
        const char* p = url + 8;
        const char* slash = strchr(p, '/');
        if (slash) { int h = slash-p; if(h>255)h=255; memcpy(hostname,p,h); hostname[h]=0; strncpy(path,slash,1023); path[1023]=0; }
        else { strncpy(hostname,p,255); hostname[255]=0; strcpy(path,"/"); }
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
            "printf 'DELETE %s HTTP/1.1\\r\\nHost: %s\\r\\nConnection: close\\r\\n\\r\\n' | openssl s_client -quiet -connect %s:443 2>/dev/null",
            path, hostname, hostname);
        FILE* fp = popen(cmd, "r"); if (!fp) return "";
        char* resp = malloc(131072); size_t len = fread(resp,1,131071,fp); resp[len]=0; pclose(fp);
        char* body = strstr(resp, "\r\n\r\n");
        if (body) { body+=4; char* r=strdup(body); free(resp); return r; }
        return resp;
    }
    char* result = http_request("DELETE", url, NULL);
    return result ? result : "";
}

void http_set_header(const char* key, const char* val) {
    if(http_header_count < 32) {
        char* header = malloc(512);
        snprintf(header, 512, "%s: %s", key, val);
        http_headers[http_header_count++] = header;
    }
}

void http_clear_headers() {
    for(int i = 0; i < http_header_count; i++) free(http_headers[i]);
    http_header_count = 0;
}

int http_status() { return http_last_status; }
char* http_error() { return http_last_error[0] ? http_last_error : NULL; }
char* last_error_get() { return last_error[0] ? last_error : NULL; }

char* url_encode(const char* str) {
    char* result = malloc(strlen(str) * 3 + 1);
    char* p = result;
    while(*str) {
        if((*str >= 'A' && *str <= 'Z') || (*str >= 'a' && *str <= 'z') || (*str >= '0' && *str <= '9') || *str == '-' || *str == '_' || *str == '.' || *str == '~') {
            *p++ = *str;
        } else if(*str == ' ') {
            *p++ = '+';
        } else {
            sprintf(p, "%%%02X", (unsigned char)*str);
            p += 3;
        }
        str++;
    }
    *p = 0;
    return result;
}

char* url_decode(const char* str) {
    char* result = malloc(strlen(str) + 1);
    char* p = result;
    while(*str) {
        if(*str == '%' && str[1] && str[2]) {
            int val;
            sscanf(str + 1, "%2x", &val);
            *p++ = val;
            str += 3;
        } else if(*str == '+') {
            *p++ = ' ';
            str++;
        } else {
            *p++ = *str++;
        }
    }
    *p = 0;
    return result;
}

char* base64_encode(const char* str) {
    static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int len = strlen(str);
    char* out = malloc(((len + 2) / 3) * 4 + 1);
    int i = 0, j = 0;
    while(i < len) {
        uint32_t a = i < len ? (unsigned char)str[i++] : 0;
        uint32_t b = i < len ? (unsigned char)str[i++] : 0;
        uint32_t c = i < len ? (unsigned char)str[i++] : 0;
        uint32_t triple = (a << 16) + (b << 8) + c;
        out[j++] = b64[(triple >> 18) & 0x3F];
        out[j++] = b64[(triple >> 12) & 0x3F];
        out[j++] = b64[(triple >> 6) & 0x3F];
        out[j++] = b64[triple & 0x3F];
    }
    for(int k = 0; k < (3 - len % 3) % 3; k++) out[j - 1 - k] = '=';
    out[j] = 0;
    return out;
}

int hash_string(const char* str) {
    unsigned int hash = 5381;
    int c;
    while((c = *str++)) hash = ((hash << 5) + hash) + c;
    return (int)hash;
}

void print_args_impl(int count, ...) {
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        if (i > 0) printf(" ");
        void* arg = va_arg(args, void*);
        // Simple heuristic: if it looks like a small integer, print as int
        if ((intptr_t)arg >= -1000000 && (intptr_t)arg <= 1000000) {
            printf("%d", (int)(intptr_t)arg);
        } else {
            printf("%s", (char*)arg);
        }
    }
    printf("\n");
    va_end(args);
}

void print_int(int x) { printf("%d", x); }
void print_float(double x) { printf("%f\n", x); }
void print_str(const char* s) { printf("%s", s); fflush(stdout); }
void print_bool(bool b) { printf("%s\n", b ? "true" : "false"); }
void print_int_no_nl(int x) { printf("%d", x); }
void print_float_no_nl(double x) { printf("%f", x); }
void print_str_no_nl(const char* s) { printf("%s", s); }
void print_bool_no_nl(bool b) { printf("%s", b ? "true" : "false"); }
void print_array(WynArray arr) { printf("["); for(int i = 0; i < arr.count; i++) { if(i > 0) printf(", "); printf("%d", array_get_int(arr, i)); } printf("]\n"); }
void print_array_no_nl(WynArray arr) { printf("["); for(int i = 0; i < arr.count; i++) { if(i > 0) printf(", "); printf("%d", array_get_int(arr, i)); } printf("]"); }
void print_value(WynValue v) {
    switch(v.type) {
        case WYN_TYPE_INT: printf("%d\n", v.data.int_val); break;
        case WYN_TYPE_FLOAT: printf("%g\n", v.data.float_val); break;
        case WYN_TYPE_STRING: printf("%s\n", v.data.string_val); break;
        default: printf("<value>\n"); break;
    }
}
#define print_no_nl(x) _Generic((x), \
    int: print_int_no_nl, \
    double: print_float_no_nl, \
    char*: print_str_no_nl, \
    const char*: print_str_no_nl, \
    bool: print_bool_no_nl, \
    WynArray: print_array_no_nl, \
    default: print_int_no_nl)(x)

void print_hex(int x) { printf("0x%x\n", x); }
void print_bin(int x) { for(int i = 31; i >= 0; i--) printf("%d", (x >> i) & 1); printf("\n"); }
#define println(x) do { print(x); printf("\n"); } while(0)
void print_debug(const char* label, int val) { printf("%s: %d\n", label, val); }
#define print(x) _Generic((x), \
    int: print_int, \
    float: print_float, \
    double: print_float, \
    char*: print_str, \
    const char*: print_str, \
    bool: print_bool, \
    WynArray: print_array, \
    WynValue: print_value, \
    default: print_int)(x)

int input() { int x = 0; if (scanf("%d", &x) != 1) { while(getchar() != '\n') { /* clear input buffer */ } return 0; } return x; }
float input_float() { float x = 0.0f; if (scanf("%f", &x) != 1) { while(getchar() != '\n') { /* clear input buffer */ } return 0.0f; } return x; }
char* input_line() { static char buffer[1024]; if (fgets(buffer, sizeof(buffer), stdin)) { size_t len = strlen(buffer); if (len > 0 && buffer[len-1] == '\n') buffer[len-1] = '\0'; return buffer; } return ""; }
void printf_wyn(const char* format, ...) { va_list args; va_start(args, format); vprintf(format, args); va_end(args); }
char* string_format(const char* format, ...) {
    va_list args; va_start(args, format);
    // First pass: calculate length
    char temp[4096]; int pos = 0;
    for (int i = 0; format[i] && pos < 4000; i++) {
        if (format[i] == '{' && format[i+1] == '}') {
            const char* s = va_arg(args, const char*);
            pos += snprintf(temp + pos, 4000 - pos, "%s", s ? s : "(null)");
            i++;
        } else { temp[pos++] = format[i]; }
    }
    temp[pos] = '\0'; va_end(args);
    return strdup(temp);
}
double sin_approx(double x) { return x - (x*x*x)/6 + (x*x*x*x*x)/120; }
double cos_approx(double x) { return 1 - (x*x)/2 + (x*x*x*x)/24; }
double pi_const() { return 3.14159265359; }
double e_const() { return 2.71828182846; }
int str_len(const char* s) { return strlen(s); }
int str_eq(const char* a, const char* b) { return strcmp(a, b) == 0; }
char* str_concat(const char* a, const char* b) { char* r = malloc(strlen(a) + strlen(b) + 1); strcpy(r, a); strcat(r, b); return r; }
char* str_upper(const char* s) { char* r = malloc(strlen(s) + 1); for(int i = 0; s[i]; i++) r[i] = toupper(s[i]); r[strlen(s)] = 0; return r; }
char* str_lower(const char* s) { char* r = malloc(strlen(s) + 1); for(int i = 0; s[i]; i++) r[i] = tolower(s[i]); r[strlen(s)] = 0; return r; }
int str_contains(const char* s, const char* sub) { return strstr(s, sub) != NULL; }
int str_starts_with(const char* s, const char* prefix) { return strncmp(s, prefix, strlen(prefix)) == 0; }
int str_ends_with(const char* s, const char* suffix) { int sl = strlen(s); int pl = strlen(suffix); return sl >= pl && strcmp(s + sl - pl, suffix) == 0; }
char* str_trim(const char* s) { while(*s == ' ') s++; int len = strlen(s); while(len > 0 && s[len-1] == ' ') len--; char* r = malloc(len + 1); strncpy(r, s, len); r[len] = 0; return r; }
const char* Fs_read_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buffer = malloc(size + 1);
    if (!buffer) { fclose(f); return ""; }
    size_t read = fread(buffer, 1, size, f);
    buffer[read] = '\0';
    fclose(f);
    return buffer;
}
char* str_repeat(const char* s, int count) { int len = strlen(s); char* r = malloc(len * count + 1); r[0] = 0; for(int i = 0; i < count; i++) strcat(r, s); return r; }
char* str_reverse(const char* s) { int len = strlen(s); char* r = malloc(len + 1); for(int i = 0; i < len; i++) r[i] = s[len-1-i]; r[len] = 0; return r; }
char* int_to_string(long long x) { char* r = malloc(32); sprintf(r, "%lld", x); return r; }
char* float_to_string(double x) { char* r = malloc(32); sprintf(r, "%g", x); return r; }
char* bool_to_string(bool x) { char* r = malloc(8); strcpy(r, x ? "true" : "false"); return r; }
int bool_to_int(bool x) { return x ? 1 : 0; }
bool bool_not(bool x) { return !x; }
bool bool_and(bool x, bool y) { return x && y; }
bool bool_or(bool x, bool y) { return x || y; }
bool bool_xor(bool x, bool y) { return x != y; }
long long wyn_safe_div(long long a, long long b) {
    if (b == 0) {
        fprintf(stderr, "Warning: Division by zero\n");
        return 0;
    }
    return a / b;
}
long long wyn_safe_mod(long long a, long long b) {
    if (b == 0) {
        fprintf(stderr, "Warning: Modulo by zero\n");
        return 0;
    }
    return a % b;
}
char* char_to_string(char x) { char* r = malloc(2); r[0] = x; r[1] = 0; return r; }
int char_to_int(char x) { return (int)x; }
bool char_is_alpha(char x) { return (x >= 'A' && x <= 'Z') || (x >= 'a' && x <= 'z'); }
bool char_is_numeric(char x) { return x >= '0' && x <= '9'; }
bool char_is_alphanumeric(char x) { return char_is_alpha(x) || char_is_numeric(x); }
bool char_is_whitespace(char x) { return x == ' ' || x == '\t' || x == '\n' || x == '\r'; }
bool char_is_uppercase(char x) { return x >= 'A' && x <= 'Z'; }
bool char_is_lowercase(char x) { return x >= 'a' && x <= 'z'; }
char char_to_upper(char x) { return (x >= 'a' && x <= 'z') ? x - 32 : x; }
char char_to_lower(char x) { return (x >= 'A' && x <= 'Z') ? x + 32 : x; }
char* str_to_string(const char* x) { return (char*)x; }
#define to_string(x) _Generic((x), \
    int: int_to_string, \
    long: int_to_string, \
    long long: int_to_string, \
    double: float_to_string, \
    char*: str_to_string, \
    const char*: str_to_string, \
    bool: bool_to_string, \
    default: int_to_string)(x)

typedef struct { const char* message; const char* type; } WynError;
WynError Error(const char* msg) { WynError e = {msg, "Error"}; return e; }
WynError TypeError(const char* msg) { WynError e = {msg, "TypeError"}; return e; }
WynError ValueError(const char* msg) { WynError e = {msg, "ValueError"}; return e; }
WynError DivisionByZeroError(const char* msg) { WynError e = {msg, "DivisionByZeroError"}; return e; }
char* str_substring(const char* s, int start, int end) { int len = strlen(s); if(start < 0) start = 0; if(end > len) end = len; if(start >= end) return malloc(1); int sublen = end - start; char* r = malloc(sublen + 1); strncpy(r, s + start, sublen); r[sublen] = 0; return r; }
int str_index_of(const char* s, const char* sub) { char* p = strstr(s, sub); return p ? (int)(p - s) : -1; }
char* str_slice(const char* s, int start, int end) { return str_substring(s, start, end); }
char* str_pad_start(const char* s, int len, const char* pad) { int slen = strlen(s); if(slen >= len) { char* r = malloc(slen + 1); strcpy(r, s); return r; } int padlen = len - slen; char* r = malloc(len + 1); for(int i = 0; i < padlen; i++) r[i] = pad[0]; strcpy(r + padlen, s); return r; }
char* str_pad_end(const char* s, int len, const char* pad) { int slen = strlen(s); if(slen >= len) { char* r = malloc(slen + 1); strcpy(r, s); return r; } char* r = malloc(len + 1); strcpy(r, s); for(int i = slen; i < len; i++) r[i] = pad[0]; r[len] = 0; return r; }
char* str_remove_prefix(const char* s, const char* prefix) { int plen = strlen(prefix); if(strncmp(s, prefix, plen) == 0) { char* r = malloc(strlen(s) - plen + 1); strcpy(r, s + plen); return r; } char* r = malloc(strlen(s) + 1); strcpy(r, s); return r; }
char* str_remove_suffix(const char* s, const char* suffix) { int slen = strlen(s); int suflen = strlen(suffix); if(slen >= suflen && strcmp(s + slen - suflen, suffix) == 0) { char* r = malloc(slen - suflen + 1); strncpy(r, s, slen - suflen); r[slen - suflen] = 0; return r; } char* r = malloc(slen + 1); strcpy(r, s); return r; }
char* str_capitalize(const char* s) { char* r = malloc(strlen(s) + 1); strcpy(r, s); if(r[0]) r[0] = toupper(r[0]); for(int i = 1; r[i]; i++) r[i] = tolower(r[i]); return r; }
char* str_center(const char* s, int width) { int len = strlen(s); if(len >= width) { char* r = malloc(len + 1); strcpy(r, s); return r; } int pad = (width - len) / 2; char* r = malloc(width + 1); for(int i = 0; i < pad; i++) r[i] = ' '; strcpy(r + pad, s); for(int i = pad + len; i < width; i++) r[i] = ' '; r[width] = 0; return r; }
char** str_lines(const char* s) { char** lines = malloc(sizeof(char*)); lines[0] = malloc(strlen(s) + 1); strcpy(lines[0], s); return lines; }
char** str_words(const char* s) { char** words = malloc(sizeof(char*)); words[0] = malloc(strlen(s) + 1); strcpy(words[0], s); return words; }
void str_free(char* s) { if(s) free(s); }
long long str_parse_int(const char* s) {
    if(!s || !*s) return 0;
    char* end;
    errno = 0;
    long long val = strtoll(s, &end, 10);
    if(errno != 0 || end == s) return 0;
    return val;
}
int str_parse_int_failed(int result) {
    return result == 0;
}
double str_parse_float(const char* s) { return atof(s); }
int abs_val(int x) { return x < 0 ? -x : x; }
int min(int a, int b) { return a < b ? a : b; }
int max(int a, int b) { return a > b ? a : b; }
int pow_int(int base, int exp) { int r = 1; for(int i = 0; i < exp; i++) r *= base; return r; }
int clamp(int x, int min_val, int max_val) { return x < min_val ? min_val : (x > max_val ? max_val : x); }
int sign(int x) { return x < 0 ? -1 : (x > 0 ? 1 : 0); }
int gcd(int a, int b) { while(b) { int t = b; b = a % b; a = t; } return a; }
int lcm(int a, int b) { return a * b / gcd(a, b); }
int is_even(int x) { return x % 2 == 0; }
int is_odd(int x) { return x % 2 != 0; }
char* file_read(const char* path) {
    last_error[0] = 0;
    FILE* f = fopen(path, "r");
    if(!f) { snprintf(last_error, 256, "Cannot open file: %s", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc(sz + 1);
    if(!buf) { snprintf(last_error, 256, "Out of memory"); fclose(f); return NULL; }
    fread(buf, 1, sz, f);
    buf[sz] = 0;
    fclose(f);
    return buf;
}
WynArray file_list_dir(const char* path) {
    WynArray arr = array_new();
    DIR* dir = opendir(path);
    if (!dir) return arr;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char* name = malloc(strlen(entry->d_name) + 1);
        strcpy(name, entry->d_name);
        array_push_str(&arr, name);
    }
    closedir(dir);
    return arr;
}
int file_is_file(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode);
}
int file_is_dir(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}
char* file_get_cwd() {
    char* buf = malloc(1024);
    if (getcwd(buf, 1024) == NULL) { free(buf); return ""; }
    return buf;
}
int file_create_dir(const char* path) {
    return mkdir(path, 0755) == 0;
}
int file_file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int)st.st_size;
}
char* file_path_join(const char* a, const char* b) {
    int len_a = strlen(a);
    int len_b = strlen(b);
    int needs_sep = (len_a > 0 && a[len_a-1] != '/') ? 1 : 0;
    char* result = malloc(len_a + len_b + needs_sep + 1);
    strcpy(result, a);
    if (needs_sep) strcat(result, "/");
    strcat(result, b);
    return result;
}
char* file_basename(const char* path) {
    const char* last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        char* result = malloc(strlen(path) + 1);
        strcpy(result, path);
        return result;
    }
    char* result = malloc(strlen(last_slash));
    strcpy(result, last_slash + 1);
    return result;
}
char* file_dirname(const char* path) {
    const char* last_slash = strrchr(path, '/');
    if (last_slash == NULL) return ".";
    int len = last_slash - path;
    if (len == 0) return "/";
    char* result = malloc(len + 1);
    strncpy(result, path, len);
    result[len] = '\0';
    return result;
}
char* file_extension(const char* path) {
    const char* last_dot = strrchr(path, '.');
    const char* last_slash = strrchr(path, '/');
    if (last_dot == NULL || (last_slash != NULL && last_dot < last_slash)) return "";
    char* result = malloc(strlen(last_dot));
    strcpy(result, last_dot + 1);
    return result;
}
char* Path_basename(const char* p) { return file_basename(p); }
char* Path_dirname(const char* p) { return file_dirname(p); }
char* Path_extension(const char* p) { return file_extension(p); }
char* Path_join(const char* a, const char* b) { return file_path_join(a, b); }

int file_write(const char* path, const char* data) {
    last_error[0] = 0;
    FILE* f = fopen(path, "w");
    if(!f) { snprintf(last_error, 256, "Cannot write file: %s", path); return 0; }
    fputs(data, f);
    fclose(f);
    return 1;
}
int file_exists(const char* path) { FILE* f = fopen(path, "r"); if(f) { fclose(f); return 1; } return 0; }
int file_delete(const char* path) { return remove(path) == 0; }
int file_copy(const char* src, const char* dst) {
    FILE* fsrc = fopen(src, "rb");
    if(!fsrc) return 0;
    FILE* fdst = fopen(dst, "wb");
    if(!fdst) { fclose(fsrc); return 0; }
    char buf[8192];
    size_t n;
    while((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) {
        if(fwrite(buf, 1, n, fdst) != n) { fclose(fsrc); fclose(fdst); return 0; }
    }
    fclose(fsrc); fclose(fdst);
    return 1;
}
int file_move(const char* src, const char* dst) { return rename(src, dst) == 0; }

// Forward declarations for file functions
int file_size(const char* path);
int file_is_dir(const char* path);
int file_is_file(const char* path);
int file_mkdir(const char* path);
char* file_get_cwd();

// File namespace aliases: File.read(path) -> File_read(path)
char* File_read(const char* p) { return file_read(p); }
int File_write(const char* p, const char* d) { return file_write(p, d); }
int File_exists(const char* p) { return file_exists(p); }
int File_delete(const char* p) { return file_delete(p); }
int File_copy(const char* s, const char* d) { return file_copy(s, d); }
int File_move(const char* s, const char* d) { return file_move(s, d); }
long long File_size(const char* p) { return file_size(p); }
int File_is_dir(const char* p) { return file_is_dir(p); }
int File_is_file(const char* p) { return file_is_file(p); }
int File_mkdir(const char* p) { return file_mkdir(p); }
char* File_list_dir(const char* p) {
    DIR* dir = opendir(p);
    if (!dir) return "";
    char* result = malloc(65536);
    result[0] = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        strcat(result, entry->d_name);
        strcat(result, "\n");
    }
    closedir(dir);
    return result;
}
int File_append(const char* p, const char* d) {
    FILE* f = fopen(p, "a");
    if (!f) return 0;
    fputs(d, f);
    fclose(f);
    return 1;
}
char* File_cwd() { return file_get_cwd(); }

// File handle API: File.open/read_line/write_line/close
#define MAX_FILE_HANDLES 32
static FILE* file_handles[MAX_FILE_HANDLES] = {0};

long long File_open(const char* path, const char* mode) {
    FILE* f = fopen(path, mode);
    if (!f) return -1;
    for (int i = 1; i < MAX_FILE_HANDLES; i++) {
        if (!file_handles[i]) { file_handles[i] = f; return i; }
    }
    fclose(f);
    return -1;
}
char* File_read_line(long long handle) {
    if (handle <= 0 || handle >= MAX_FILE_HANDLES || !file_handles[handle]) return "";
    char* buf = malloc(4096);
    if (fgets(buf, 4096, file_handles[handle])) return buf;
    free(buf);
    return "";
}
int File_write_line(long long handle, const char* data) {
    if (handle <= 0 || handle >= MAX_FILE_HANDLES || !file_handles[handle]) return 0;
    return fputs(data, file_handles[handle]) >= 0;
}
int File_eof(long long handle) {
    if (handle <= 0 || handle >= MAX_FILE_HANDLES || !file_handles[handle]) return 1;
    return feof(file_handles[handle]) ? 1 : 0;
}
void File_close(long long handle) {
    if (handle <= 0 || handle >= MAX_FILE_HANDLES || !file_handles[handle]) return;
    fclose(file_handles[handle]);
    file_handles[handle] = NULL;
}

// === HTTP Server ===
static void http_send_response(int client_fd, int status, const char* content_type, const char* body) {
    const char* status_text = "OK";
    if (status == 201) status_text = "Created";
    else if (status == 400) status_text = "Bad Request";
    else if (status == 404) status_text = "Not Found";
    else if (status == 500) status_text = "Internal Server Error";
    int body_len = body ? (int)strlen(body) : 0;
    char header[1024];
    snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
        status, status_text, content_type ? content_type : "text/plain", body_len);
    send(client_fd, header, strlen(header), 0);
    if (body && body_len > 0) send(client_fd, body, body_len, 0);
}

void Http_respond(long long client_fd, long long status, const char* content_type, const char* body) {
    http_send_response((int)client_fd, (int)status, content_type, body);
    close((int)client_fd);
}

int Http_serve(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return -1;
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(server_fd); return -1; }
    if (listen(server_fd, 128) < 0) { close(server_fd); return -1; }
    return server_fd;
}

char* Http_accept(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) return "";
    char buf[8192] = {0};
    int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) { close(client_fd); return ""; }
    buf[n] = 0;
    
    // Save a copy for body parsing
    char buf_copy[8192];
    memcpy(buf_copy, buf, n + 1);
    
    // Parse method and path from original
    char method[16] = "", path[1024] = "";
    char* sp1 = strchr(buf, ' ');
    if (sp1) {
        *sp1 = 0;
        strncpy(method, buf, 15);
        char* sp2 = strchr(sp1 + 1, ' ');
        if (sp2) { *sp2 = 0; strncpy(path, sp1 + 1, 1023); }
    }
    
    // Find body from copy
    char* body_start = strstr(buf_copy, "\r\n\r\n");
    const char* body = body_start ? body_start + 4 : "";
    
    char* result = malloc(16384);
    snprintf(result, 16384, "%s|%s|%s|%d", method, path, body, client_fd);
    return result;
}

void Http_close_server(int fd) { if (fd >= 0) close(fd); }

// HashMap/HashSet: codegen maps HashMap.new() -> hashmap_new(), HashSet.new() -> hashset_new()
// HashMap.keys() -> newline-separated string of all keys
// HashMap.len() -> number of entries
extern char* hashmap_keys_string(WynHashMap* map);
char* hashmap_keys(WynHashMap* map) { return hashmap_keys_string(map); }

// String.split(delim) -> newline-separated string (use split_at for indexed access)
char* string_split_to_str(const char* s, const char* delim) {
    if (!s || !delim) return "";
    int dlen = strlen(delim);
    char* result = malloc(strlen(s) + 256);
    result[0] = 0;
    const char* p = s;
    while (1) {
        const char* found = strstr(p, delim);
        if (!found) { strcat(result, p); break; }
        strncat(result, p, found - p);
        strcat(result, "\n");
        p = found + dlen;
    }
    return result;
}

// Array.sort_by(fn) — sort using comparison function
void wyn_array_sort_by(WynArray* arr, long long (*cmp)(long long, long long)) {
    // Simple insertion sort (stable, good for small arrays)
    for (int i = 1; i < arr->count; i++) {
        long long key = arr->data[i].data.int_val;
        int j = i - 1;
        while (j >= 0 && cmp(arr->data[j].data.int_val, key) > 0) {
            arr->data[j + 1] = arr->data[j];
            j--;
        }
        arr->data[j + 1].data.int_val = key;
    }
}
// HashSet namespace: HashSet.new() -> hashset_new()
WynHashSet* HashSet_new() { return hashset_new(); }

// Json namespace: Json.new() -> Json_new(), j.set_string() -> json_set_string()
// Note: Json_get_string/Json_get_int defined in json_runtime.c
WynJson* Json_new() { return json_new(); }
void Json_set(WynJson* j, const char* k, const char* v) { json_set_string(j, k, v); }
void Json_set_string(WynJson* j, const char* k, const char* v) { json_set_string(j, k, v); }
void Json_set_int(WynJson* j, const char* k, int v) { json_set_int(j, k, v); }
void Json_set_bool(WynJson* j, const char* k, int v) { json_set_int(j, k, v ? 1 : 0); }char* Json_stringify(WynJson* j) { return json_stringify(j); }

// Terminal module: POSIX terminal control
#include <sys/ioctl.h>
#include <termios.h>

static struct termios __wyn_orig_termios;
static int __wyn_raw_mode_active = 0;

int Terminal_cols() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) return w.ws_col;
    return 80;
}
int Terminal_rows() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) return w.ws_row;
    return 24;
}
void Terminal_raw_mode() {
    if (__wyn_raw_mode_active) return;
    tcgetattr(STDIN_FILENO, &__wyn_orig_termios);
    struct termios raw = __wyn_orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;   // Non-blocking
    raw.c_cc[VTIME] = 1;  // 100ms timeout
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    __wyn_raw_mode_active = 1;
}
void Terminal_restore() {
    if (!__wyn_raw_mode_active) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &__wyn_orig_termios);
    __wyn_raw_mode_active = 0;
}
int Terminal_read_key() {
    char c = 0;
    int n = read(STDIN_FILENO, &c, 1);
    if (n <= 0) return 0;
    // Handle escape sequences (arrow keys etc)
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return 27; // bare ESC
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return 27;
        if (seq[0] == '[') {
            if (seq[1] == 'A') return 1000; // Up
            if (seq[1] == 'B') return 1001; // Down
            if (seq[1] == 'C') return 1002; // Right
            if (seq[1] == 'D') return 1003; // Left
        }
        return 27;
    }
    return (int)c;
}
void Terminal_clear() {
    write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
}
void Terminal_move(int row, int col) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row, col);
    write(STDOUT_FILENO, buf, len);
}
void Terminal_write(const char* s) {
    write(STDOUT_FILENO, s, strlen(s));
}

// Http: Http.get() maps to http_get() (lowercase, returns string)
// Note: Http_get in net_advanced.c returns HttpResponse* (different API)

// Regex namespace aliases
int Regex_match(const char* s, const char* p) { return regex_match(s, p); }
char* Regex_replace(const char* s, const char* p, const char* r) { return regex_replace(s, p, r); }

int file_size(const char* path) {
    FILE* f = fopen(path, "rb");
    if(!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    return (int)sz;
}
int file_mkdir(const char* path) {
    last_error[0] = 0;
#ifdef _WIN32
    int result = _mkdir(path);
#else
    int result = mkdir(path, 0755);
#endif
    if(result != 0) snprintf(last_error, 256, "Cannot create directory: %s", path);
    return result == 0;
}
int file_rmdir(const char* path) {
    last_error[0] = 0;
    int result = rmdir(path);
    if(result != 0) snprintf(last_error, 256, "Cannot remove directory: %s", path);
    return result == 0;
}
long file_modified_time(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long)st.st_mtime;
}

int file_create_dir_all(const char* path) {
    if (!path || !*path) return 0;
    char tmp[1024];
    char *p = NULL;
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return 0;
    snprintf(tmp, sizeof(tmp), "%s", path);
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            #ifdef _WIN32
            mkdir(tmp);
            #else
            mkdir(tmp, 0755);
            #endif
            *p = '/';
        }
    }
    #ifdef _WIN32
    return mkdir(tmp) == 0 || errno == EEXIST;
    #else
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
    #endif
}

int file_remove_dir_all(const char* path) {
    if (!path || !*path) return 0;
    DIR *d = opendir(path);
    if (!d) return remove(path) == 0;
    struct dirent *p;
    int r = 0;
    while (!r && (p = readdir(d))) {
        if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) continue;
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s/%s", path, p->d_name);
        struct stat st;
        if (stat(buf, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                r = !file_remove_dir_all(buf);
            } else {
                r = remove(buf) != 0;
            }
        }
    }
    closedir(d);
    return !r && rmdir(path) == 0;
}

char* System_exec(const char* cmd) {
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "";
    char* result = malloc(65536);
    result[0] = 0;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        strcat(result, buffer);
    }
    pclose(pipe);
    return result;
}
int System_exec_code(const char* cmd) {
    int result = system(cmd);
    #ifdef _WIN32
    return result;
    #else
    return WEXITSTATUS(result);
    #endif
}
void System_exit(int code) { exit(code); }
char* System_env(const char* key) {
    char* val = getenv(key);
    if (!val) return "";
    char* result = malloc(strlen(val) + 1);
    strcpy(result, val);
    return result;
}

int System_set_env(const char* key, const char* value) {
    #ifdef _WIN32
    return _putenv_s(key, value) == 0;
    #else
    return setenv(key, value, 1) == 0;
    #endif
}

WynArray System_args() {
    WynArray arr;
    arr.data = malloc(__wyn_argc * sizeof(WynValue));
    arr.count = __wyn_argc;
    arr.capacity = __wyn_argc;
    for (int i = 0; i < __wyn_argc; i++) {
        arr.data[i].type = WYN_TYPE_STRING;
        arr.data[i].data.string_val = __wyn_argv[i];
    }
    return arr;
}

char* Env_get(const char* name) {
    if (!name) return "";
    char* value = getenv(name);
    return value ? value : "";
}
int Env_set(const char* name, const char* value) {
    if (!name || !value) return 0;
#ifdef _WIN32
    return _putenv_s(name, value) == 0;
#else
    return setenv(name, value, 1) == 0;
#endif
}
WynArray Env_all() {
    extern char **environ;
    WynArray arr;
    int count = 0;
    for (char** env = environ; *env; env++) count++;
    arr.data = malloc(count * sizeof(WynValue));
    arr.count = count;
    arr.capacity = count;
    for (int i = 0; i < count; i++) {
        arr.data[i].type = WYN_TYPE_STRING;
        arr.data[i].data.string_val = environ[i];
    }
    return arr;
}

float Math_sin(float x) { return sinf(x); }
float Math_cos(float x) { return cosf(x); }
float Math_tan(float x) { return tanf(x); }
float Math_sqrt(float x) { return sqrtf(x); }
float Math_pow(float base, float exp) { return powf(base, exp); }
float Math_floor(float x) { return floorf(x); }
float Math_ceil(float x) { return ceilf(x); }
float Math_round(float x) { return roundf(x); }
long long Math_abs(long long x) { return x < 0 ? -x : x; }
float Math_max(float a, float b) { return a > b ? a : b; }
float Math_min(float a, float b) { return a < b ? a : b; }
float Math_random() {
    static int initialized = 0;
    if (!initialized) { srand(time(NULL)); initialized = 1; }
    return (float)rand() / (float)RAND_MAX;
}
const float Math_PI = 3.14159265358979323846f;
const float Math_E = 2.71828182845904523536f;

long long DateTime_now() { return (long long)time(NULL); }
long long DateTime_millis() { struct timeval tv; gettimeofday(&tv, NULL); return (long long)(tv.tv_sec * 1000LL + tv.tv_usec / 1000); }
long long DateTime_micros() { struct timeval tv; gettimeofday(&tv, NULL); return (long long)(tv.tv_sec * 1000000LL + tv.tv_usec); }
char* DateTime_format(int timestamp, const char* fmt) {
    time_t t = (time_t)timestamp;
    struct tm* tm_info = localtime(&t);
    char* buffer = malloc(256);
    strftime(buffer, 256, fmt, tm_info);
    return buffer;
}
void DateTime_sleep(int seconds) { sleep(seconds); }

typedef struct { WynArray arr; } Queue;

Queue* Queue_new() {
    Queue* q = malloc(sizeof(Queue));
    q->arr.data = NULL;
    q->arr.count = 0;
    q->arr.capacity = 0;
    return q;
}

void Queue_push(Queue* q, int value) {
    if (q->arr.count >= q->arr.capacity) {
        q->arr.capacity = q->arr.capacity == 0 ? 4 : q->arr.capacity * 2;
        q->arr.data = realloc(q->arr.data, sizeof(WynValue) * q->arr.capacity);
    }
    q->arr.data[q->arr.count].type = WYN_TYPE_INT;
    q->arr.data[q->arr.count].data.int_val = value;
    q->arr.count++;
}

int Queue_pop(Queue* q) {
    if (q->arr.count == 0) return 0;
    int value = q->arr.data[0].data.int_val;
    for (int i = 1; i < q->arr.count; i++) {
        q->arr.data[i-1] = q->arr.data[i];
    }
    q->arr.count--;
    return value;
}

int Queue_peek(Queue* q) {
    if (q->arr.count == 0) return 0;
    return q->arr.data[0].data.int_val;
}

int Queue_len(Queue* q) {
    return q->arr.count;
}

int Queue_is_empty(Queue* q) {
    return q->arr.count == 0;
}

typedef struct { WynArray arr; } Stack;

Stack* Stack_new() {
    Stack* s = malloc(sizeof(Stack));
    s->arr.data = NULL;
    s->arr.count = 0;
    s->arr.capacity = 0;
    return s;
}

void Stack_push(Stack* s, int value) {
    if (s->arr.count >= s->arr.capacity) {
        s->arr.capacity = s->arr.capacity == 0 ? 4 : s->arr.capacity * 2;
        s->arr.data = realloc(s->arr.data, sizeof(WynValue) * s->arr.capacity);
    }
    s->arr.data[s->arr.count].type = WYN_TYPE_INT;
    s->arr.data[s->arr.count].data.int_val = value;
    s->arr.count++;
}

int Stack_pop(Stack* s) {
    if (s->arr.count == 0) return 0;
    s->arr.count--;
    return s->arr.data[s->arr.count].data.int_val;
}

int Stack_peek(Stack* s) {
    if (s->arr.count == 0) return 0;
    return s->arr.data[s->arr.count - 1].data.int_val;
}

int Stack_len(Stack* s) {
    return s->arr.count;
}

int Stack_is_empty(Stack* s) {
    return s->arr.count == 0;
}

int Net_listen(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        return -1;
    }
    if (listen(sockfd, 5) < 0) {
        close(sockfd);
        return -1;
    }
    return sockfd;
}

int Net_connect(const char* host, int port) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    if (getaddrinfo(host, port_str, &hints, &res) != 0) return -1;
    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) { freeaddrinfo(res); return -1; }
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    return sockfd;
}

int Net_send(int sockfd, const char* data) {
    int len = strlen(data);
    int sent = send(sockfd, data, len, 0);
    return sent;
}

char* Net_recv(int sockfd) {
    char* buffer = malloc(4096);
    int received = recv(sockfd, buffer, 4095, 0);
    if (received < 0) {
        free(buffer);
        return "";
    }
    buffer[received] = '\0';
    return buffer;
}

int Net_close(int sockfd) {
    return close(sockfd) == 0 ? 1 : 0;
}

char* Time_format(int timestamp) {
    time_t t = (time_t)timestamp;
    struct tm* tm_info = localtime(&t);
    char* buffer = malloc(64);
    strftime(buffer, 64, "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}
int arr_sum(WynArray arr, int len) { int s = 0; for(int i = 0; i < len; i++) s += array_get_int(arr, i); return s; }
int arr_max(WynArray arr, int len) { int m = array_get_int(arr, 0); for(int i = 1; i < len; i++) { int val = array_get_int(arr, i); if(val > m) m = val; } return m; }
int arr_min(WynArray arr, int len) { int m = array_get_int(arr, 0); for(int i = 1; i < len; i++) { int val = array_get_int(arr, i); if(val < m) m = val; } return m; }
int arr_contains(WynArray arr, int len, int val) { for(int i = 0; i < len; i++) if(array_get_int(arr, i) == val) return 1; return 0; }
int arr_find(WynArray arr, int len, int val) { for(int i = 0; i < len; i++) if(array_get_int(arr, i) == val) return i; return -1; }
void arr_reverse(int* arr, int len) { for(int i = 0; i < len/2; i++) { int t = arr[i]; arr[i] = arr[len-1-i]; arr[len-1-i] = t; } }
void arr_sort(int* arr, int len) { for(int i = 0; i < len-1; i++) for(int j = 0; j < len-i-1; j++) if(arr[j] > arr[j+1]) { int t = arr[j]; arr[j] = arr[j+1]; arr[j+1] = t; } }
int arr_count(int* arr, int len, int val) { int c = 0; for(int i = 0; i < len; i++) if(arr[i] == val) c++; return c; }
void arr_fill(int* arr, int len, int val) { for(int i = 0; i < len; i++) arr[i] = val; }
int arr_all(int* arr, int len, int val) { for(int i = 0; i < len; i++) if(arr[i] != val) return 0; return 1; }
char* arr_join(int* arr, int len, const char* sep) { int total = 0; for(int i = 0; i < len; i++) { total += snprintf(NULL, 0, "%d", arr[i]); if(i < len-1) total += strlen(sep); } char* r = malloc(total + 1); r[0] = 0; for(int i = 0; i < len; i++) { char buf[32]; snprintf(buf, 32, "%d", arr[i]); strcat(r, buf); if(i < len-1) strcat(r, sep); } return r; }
WynArray arr_map_double(WynArray arr) { WynArray result = array_new(); for(int i = 0; i < arr.count; i++) { int val = array_get_int(arr, i); array_push_int(&result, val * 2); } return result; }
WynArray arr_map_square(WynArray arr) { WynArray result = array_new(); for(int i = 0; i < arr.count; i++) { int val = array_get_int(arr, i); array_push_int(&result, val * val); } return result; }
WynArray arr_filter_positive(WynArray arr) { WynArray result = array_new(); for(int i = 0; i < arr.count; i++) { int val = array_get_int(arr, i); if(val > 0) array_push_int(&result, val); } return result; }
WynArray arr_filter_even(WynArray arr) { WynArray result = array_new(); for(int i = 0; i < arr.count; i++) { int val = array_get_int(arr, i); if(val % 2 == 0) array_push_int(&result, val); } return result; }
WynArray arr_filter_greater_than_3(WynArray arr) { WynArray result = array_new(); for(int i = 0; i < arr.count; i++) { int val = array_get_int(arr, i); if(val > 3) array_push_int(&result, val); } return result; }
int arr_reduce_sum(WynArray arr) { int result = 0; for(int i = 0; i < arr.count; i++) { result += array_get_int(arr, i); } return result; }
int arr_reduce_product(WynArray arr) { int result = 1; for(int i = 0; i < arr.count; i++) { result *= array_get_int(arr, i); } return result; }
WynArray wyn_array_map(WynArray arr, long long (*fn)(long long)) {
    WynArray result = array_new();
    for (int i = 0; i < arr.count; i++) {
        long long val = array_get_int(arr, i);
        array_push_int(&result, fn(val));
    }
    return result;
}
WynArray wyn_array_filter(WynArray arr, long long (*fn)(long long)) {
    WynArray result = array_new();
    for (int i = 0; i < arr.count; i++) {
        long long val = array_get_int(arr, i);
        if (fn(val)) array_push_int(&result, val);
    }
    return result;
}
long long wyn_array_reduce(WynArray arr, long long (*fn)(long long, long long), long long initial) {
    long long result = initial;
    for (int i = 0; i < arr.count; i++) {
        long long val = array_get_int(arr, i);
        result = fn(result, val);
    }
    return result;
}
int random_int(int min, int max) { return min + rand() % (max - min + 1); }
int random_range(int min, int max) { return min + rand() % (max - min + 1); }
double random_float() { return (double)rand() / RAND_MAX; }
void seed_random(int seed) { srand(seed); }
long long time_now() { return (long long)time(NULL); }
char* time_format(int timestamp, const char* fmt) {
    time_t t = (time_t)timestamp;
    struct tm* tm_info = localtime(&t);
    char* buffer = malloc(80);
    strftime(buffer, 80, fmt, tm_info);
    return buffer;
}
void assert_eq(int a, int b) { if (a != b) { printf("Assertion failed: %d != %d\n", a, b); exit(1); } }
void assert_true(int cond) { if (!cond) { printf("Assertion failed\n"); exit(1); } }
void assert_false(int cond) { if (cond) { printf("Assertion failed\n"); exit(1); } }
void panic(const char* msg) { printf("Panic: %s\n", msg); exit(1); }
void todo(const char* msg) { printf("TODO: %s\n", msg); exit(1); }
void exit_program(int code) { exit(code); }
void sleep_ms(int ms) { struct timespec ts; ts.tv_sec = ms / 1000; ts.tv_nsec = (ms % 1000) * 1000000; nanosleep(&ts, NULL); }
char* getenv_var(const char* name) { return getenv(name); }
int setenv_var(const char* name, const char* val) { return setenv(name, val, 1) == 0; }
int sqrt_int(int x) { return (int)sqrt(x); }
int ceil_int(double x) { return (int)ceil(x); }
int floor_int(double x) { return (int)floor(x); }
int round_int(double x) { return (int)round(x); }
double abs_float(double x) { return fabs(x); }
char* str_replace(const char* s, const char* old, const char* new) { int count = 0; const char* p = s; int oldlen = strlen(old); int newlen = strlen(new); while((p = strstr(p, old))) { count++; p += oldlen; } int total = strlen(s) + count * (newlen - oldlen) + 1; char* r = malloc(total); char* dst = r; p = s; while(*p) { if(strncmp(p, old, oldlen) == 0) { memcpy(dst, new, newlen); dst += newlen; p += oldlen; } else { *dst++ = *p++; } } *dst = 0; return r; }
char** str_split(const char* s, const char* delim, int* count) {
    char** r = malloc(100 * sizeof(char*));
    *count = 0;
    int dlen = strlen(delim);
    const char* p = s;
    while (*count < 100) {
        const char* found = strstr(p, delim);
        if (!found) {
            r[*count] = strdup(p);
            (*count)++;
            break;
        }
        int len = found - p;
        r[*count] = malloc(len + 1);
        memcpy(r[*count], p, len);
        r[*count][len] = 0;
        (*count)++;
        p = found + dlen;
    }
    return r;
}
char* split_get(const char* s, const char* delim, int index) { int count = 0; char** parts = str_split(s, delim, &count); if (index < 0 || index >= count) return ""; return parts[index]; }
int split_count(const char* s, const char* delim) { int count = 0; str_split(s, delim, &count); return count; }
char* char_at(const char* s, int index) { if (index < 0 || index >= strlen(s)) return ""; static char buf[2]; buf[0] = s[index]; buf[1] = '\0'; return buf; }
int is_numeric(const char* s) { if (!s || !*s) return 0; int i = 0; if (s[0] == '-' || s[0] == '+') i++; if (!s[i]) return 0; while (s[i]) { if (s[i] < '0' || s[i] > '9') return 0; i++; } return 1; }
int str_count(const char* s, const char* substr) { if (!s || !substr || !*substr) return 0; int count = 0; const char* p = s; while ((p = strstr(p, substr)) != NULL) { count++; p += strlen(substr); } return count; }
int str_contains_substr(const char* s, const char* substr) { return strstr(s, substr) != NULL; }
char* str_join(char** arr, int len, const char* sep) { int total = 0; for(int i = 0; i < len; i++) total += strlen(arr[i]); total += (len - 1) * strlen(sep) + 1; char* r = malloc(total); r[0] = 0; for(int i = 0; i < len; i++) { if(i > 0) strcat(r, sep); strcat(r, arr[i]); } return r; }
char* int_to_str(int n) { char* r = malloc(12); sprintf(r, "%d", n); return r; }
long long str_to_int(const char* s) { return atoll(s); }
double str_to_float(const char* s) { return atof(s); }
void swap(int* a, int* b) { int t = *a; *a = *b; *b = t; }
double clamp_float(double x, double min_val, double max_val) { return x < min_val ? min_val : (x > max_val ? max_val : x); }
double lerp(double a, double b, double t) { return a + t * (b - a); }
double map_range(double x, double in_min, double in_max, double out_min, double out_max) { return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min; }
int bit_set(int x, int pos) { return x | (1 << pos); }
int bit_clear(int x, int pos) { return x & ~(1 << pos); }
int bit_toggle(int x, int pos) { return x ^ (1 << pos); }
int bit_check(int x, int pos) { return (x >> pos) & 1; }
int bit_count(int x) { int c = 0; while(x) { c += x & 1; x >>= 1; } return c; }

// ARC functions are provided by arc_runtime.c

// Result type implementations
typedef struct { int tag; union { int ok_value; const char* err_value; } data; } ResultInt;
typedef struct { int tag; union { const char* ok_value; const char* err_value; } data; } ResultString;

ResultInt ResultInt_Ok(int value) { ResultInt r; r.tag = 0; r.data.ok_value = value; return r; }
ResultInt ResultInt_Err(const char* msg) { ResultInt r; r.tag = 1; r.data.err_value = msg; return r; }
int ResultInt_is_ok(ResultInt r) { return r.tag == 0; }
int ResultInt_is_err(ResultInt r) { return r.tag == 1; }
int ResultInt_unwrap(ResultInt r) { if (r.tag == 1) { fprintf(stderr, "Error: unwrap() called on Err: %s\n", r.data.err_value); exit(1); } return r.data.ok_value; }
const char* ResultInt_unwrap_err(ResultInt r) { if (r.tag == 0) { fprintf(stderr, "Error: unwrap_err() called on Ok\n"); exit(1); } return r.data.err_value; }

ResultString ResultString_Ok(const char* value) { ResultString r; r.tag = 0; r.data.ok_value = value; return r; }
ResultString ResultString_Err(const char* msg) { ResultString r; r.tag = 1; r.data.err_value = msg; return r; }
int ResultString_is_ok(ResultString r) { return r.tag == 0; }
int ResultString_is_err(ResultString r) { return r.tag == 1; }
const char* ResultString_unwrap(ResultString r) { if (r.tag == 1) { fprintf(stderr, "Error: unwrap() called on Err: %s\n", r.data.err_value); exit(1); } return r.data.ok_value; }
const char* ResultString_unwrap_err(ResultString r) { if (r.tag == 0) { fprintf(stderr, "Error: unwrap_err() called on Ok\n"); exit(1); } return r.data.err_value; }

typedef struct { int tag; int value; } OptionInt;
OptionInt OptionInt_Some(int value) { OptionInt o; o.tag = 1; o.value = value; return o; }
OptionInt OptionInt_None() { OptionInt o; o.tag = 0; o.value = 0; return o; }
int OptionInt_is_some(OptionInt o) { return o.tag == 1; }
int OptionInt_is_none(OptionInt o) { return o.tag == 0; }
int OptionInt_unwrap(OptionInt o) { if (o.tag == 0) { fprintf(stderr, "Error: unwrap() called on None\n"); exit(1); } return o.value; }
int OptionInt_unwrap_or(OptionInt o, int def) { return o.tag == 1 ? o.value : def; }

typedef struct { int tag; const char* value; } OptionString;
OptionString OptionString_Some(const char* value) { OptionString o; o.tag = 1; o.value = value; return o; }
OptionString OptionString_None() { OptionString o; o.tag = 0; o.value = NULL; return o; }
int OptionString_is_some(OptionString o) { return o.tag == 1; }
int OptionString_is_none(OptionString o) { return o.tag == 0; }
const char* OptionString_unwrap(OptionString o) { if (o.tag == 0) { fprintf(stderr, "Error: unwrap() called on None\n"); exit(1); } return o.value; }
const char* OptionString_unwrap_or(OptionString o, const char* def) { return o.tag == 1 ? o.value : def; }

// Task — simplified concurrency API
// Task.all(f1, f2) — wait for multiple spawns
// Task.race(f1, f2) — first spawn wins
// Task.value(n) — shared mutable int between spawns
// Task.get(v) / Task.set(v, n) — read/write shared value

// Shared value: thread-safe int with mutex
typedef struct {
    long long value;
    pthread_mutex_t lock;
} WynSharedValue;

#define MAX_SHARED_VALUES 64
static WynSharedValue* shared_registry[MAX_SHARED_VALUES] = {0};

long long Task_value(long long initial) {
    WynSharedValue* sv = malloc(sizeof(WynSharedValue));
    sv->value = initial;
    pthread_mutex_init(&sv->lock, NULL);
    for (int i = 1; i < MAX_SHARED_VALUES; i++) {
        if (!shared_registry[i]) { shared_registry[i] = sv; return i; }
    }
    free(sv);
    return 0;
}

long long Task_get(long long handle) {
    if (handle <= 0 || handle >= MAX_SHARED_VALUES || !shared_registry[handle]) return 0;
    WynSharedValue* sv = shared_registry[handle];
    pthread_mutex_lock(&sv->lock);
    long long val = sv->value;
    pthread_mutex_unlock(&sv->lock);
    return val;
}

void Task_set(long long handle, long long value) {
    if (handle <= 0 || handle >= MAX_SHARED_VALUES || !shared_registry[handle]) return;
    WynSharedValue* sv = shared_registry[handle];
    pthread_mutex_lock(&sv->lock);
    sv->value = value;
    pthread_mutex_unlock(&sv->lock);
}

void Task_add(long long handle, long long amount) {
    if (handle <= 0 || handle >= MAX_SHARED_VALUES || !shared_registry[handle]) return;
    WynSharedValue* sv = shared_registry[handle];
    pthread_mutex_lock(&sv->lock);
    sv->value += amount;
    pthread_mutex_unlock(&sv->lock);
}

void Task_free_value(long long handle) {
    if (handle <= 0 || handle >= MAX_SHARED_VALUES || !shared_registry[handle]) return;
    pthread_mutex_destroy(&shared_registry[handle]->lock);
    free(shared_registry[handle]);
    shared_registry[handle] = NULL;
}

// Channel API (advanced) — kept for power users
#define MAX_TASKS 64
static WynTask* task_registry[MAX_TASKS] = {0};

long long Task_channel(long long capacity) {
    WynTask* task = wyn_task_new((int)capacity);
    for (int i = 1; i < MAX_TASKS; i++) {
        if (!task_registry[i]) { task_registry[i] = task; return i; }
    }
    wyn_task_free(task);
    return 0;
}
void Task_send(long long handle, long long value) {
    if (handle <= 0 || handle >= MAX_TASKS || !task_registry[handle]) return;
    long long* boxed = malloc(sizeof(long long));
    *boxed = value;
    wyn_task_send(task_registry[handle], boxed);
}
long long Task_recv(long long handle) {
    if (handle <= 0 || handle >= MAX_TASKS || !task_registry[handle]) return 0;
    void* ptr = wyn_task_recv(task_registry[handle]);
    if (!ptr) return 0;
    long long val = *(long long*)ptr;
    free(ptr);
    return val;
}
void Task_close(long long handle) {
    if (handle <= 0 || handle >= MAX_TASKS || !task_registry[handle]) return;
    wyn_task_close(task_registry[handle]);
}

// === SQLite Database Module ===
// Only compiled when WYN_USE_SQLITE is defined (set by codegen when Db. is used)
#ifdef WYN_USE_SQLITE
#include <sqlite3.h>

#define MAX_DB_HANDLES 16
static sqlite3* db_handles[MAX_DB_HANDLES] = {0};

long long Db_open(const char* path) {
    sqlite3* db;
    if (sqlite3_open(path, &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return -1;
    }
    for (int i = 1; i < MAX_DB_HANDLES; i++) {
        if (!db_handles[i]) { db_handles[i] = db; return i; }
    }
    sqlite3_close(db);
    return -1;
}

int Db_exec(long long handle, const char* sql) {
    if (handle <= 0 || handle >= MAX_DB_HANDLES || !db_handles[handle]) return -1;
    char* err = NULL;
    int rc = sqlite3_exec(db_handles[handle], sql, NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

char* Db_query(long long handle, const char* sql) {
    if (handle <= 0 || handle >= MAX_DB_HANDLES || !db_handles[handle]) return "";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_handles[handle], sql, -1, &stmt, NULL) != SQLITE_OK) return "";
    
    char* result = malloc(65536);
    result[0] = 0;
    int first_row = 1;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (!first_row) strcat(result, "\n");
        first_row = 0;
        int cols = sqlite3_column_count(stmt);
        for (int i = 0; i < cols; i++) {
            if (i > 0) strcat(result, "|");
            const char* val = (const char*)sqlite3_column_text(stmt, i);
            if (val) strcat(result, val);
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

char* Db_query_one(long long handle, const char* sql) {
    if (handle <= 0 || handle >= MAX_DB_HANDLES || !db_handles[handle]) return "";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_handles[handle], sql, -1, &stmt, NULL) != SQLITE_OK) return "";
    char* result = "";
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* val = (const char*)sqlite3_column_text(stmt, 0);
        result = val ? strdup(val) : "";
    }
    sqlite3_finalize(stmt);
    return result;
}

long long Db_last_insert_id(long long handle) {
    if (handle <= 0 || handle >= MAX_DB_HANDLES || !db_handles[handle]) return 0;
    return (long long)sqlite3_last_insert_rowid(db_handles[handle]);
}

char* Db_error(long long handle) {
    if (handle <= 0 || handle >= MAX_DB_HANDLES || !db_handles[handle]) return "invalid handle";
    return (char*)sqlite3_errmsg(db_handles[handle]);
}

void Db_close(long long handle) {
    if (handle <= 0 || handle >= MAX_DB_HANDLES || !db_handles[handle]) return;
    sqlite3_close(db_handles[handle]);
    db_handles[handle] = NULL;
}

#else
// SQLite not available — stub functions
long long Db_open(const char* path) { fprintf(stderr, "Error: SQLite not available. Install libsqlite3-dev.\n"); return -1; }
int Db_exec(long long h, const char* sql) { return -1; }
char* Db_query(long long h, const char* sql) { return ""; }
char* Db_query_one(long long h, const char* sql) { return ""; }
long long Db_last_insert_id(long long h) { return 0; }
char* Db_error(long long h) { return "sqlite not available"; }
void Db_close(long long h) {}
#endif // WYN_USE_SQLITE

// === StringBuilder — O(1) amortized append ===
typedef struct {
    char* data;
    int len;
    int cap;
} WynStringBuilder;

#define MAX_STRING_BUILDERS 32
static WynStringBuilder sb_pool[MAX_STRING_BUILDERS] = {0};

long long StringBuilder_new() {
    for (int i = 1; i < MAX_STRING_BUILDERS; i++) {
        if (!sb_pool[i].data) {
            sb_pool[i].cap = 256;
            sb_pool[i].data = malloc(256);
            sb_pool[i].data[0] = 0;
            sb_pool[i].len = 0;
            return i;
        }
    }
    return -1;
}

void StringBuilder_append(long long handle, const char* s) {
    if (handle <= 0 || handle >= MAX_STRING_BUILDERS || !sb_pool[handle].data) return;
    WynStringBuilder* sb = &sb_pool[handle];
    int slen = strlen(s);
    while (sb->len + slen + 1 > sb->cap) {
        sb->cap *= 2;
        sb->data = realloc(sb->data, sb->cap);
    }
    memcpy(sb->data + sb->len, s, slen);
    sb->len += slen;
    sb->data[sb->len] = 0;
}

char* StringBuilder_to_string(long long handle) {
    if (handle <= 0 || handle >= MAX_STRING_BUILDERS || !sb_pool[handle].data) return "";
    return sb_pool[handle].data;
}

long long StringBuilder_len(long long handle) {
    if (handle <= 0 || handle >= MAX_STRING_BUILDERS) return 0;
    return sb_pool[handle].len;
}

void StringBuilder_clear(long long handle) {
    if (handle <= 0 || handle >= MAX_STRING_BUILDERS || !sb_pool[handle].data) return;
    sb_pool[handle].len = 0;
    sb_pool[handle].data[0] = 0;
}

void StringBuilder_free(long long handle) {
    if (handle <= 0 || handle >= MAX_STRING_BUILDERS || !sb_pool[handle].data) return;
    free(sb_pool[handle].data);
    sb_pool[handle].data = NULL;
    sb_pool[handle].len = 0;
    sb_pool[handle].cap = 0;
}

// === JSON Parsing ===
// Simple recursive descent JSON parser returning handle-based access
#define MAX_JSON_NODES 4096
typedef struct { char type; /* o=object, a=array, s=string, n=number, b=bool, x=null */ char* key; char* str_val; double num_val; int parent; int next_sibling; int first_child; } JsonNode;
static JsonNode json_nodes[MAX_JSON_NODES];
static int json_node_count = 0;

static int json_alloc_node() { if (json_node_count >= MAX_JSON_NODES) return -1; int i = json_node_count++; json_nodes[i] = (JsonNode){0}; json_nodes[i].parent = -1; json_nodes[i].next_sibling = -1; json_nodes[i].first_child = -1; return i; }

static const char* json_skip_ws(const char* p) { while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++; return p; }

static const char* json_parse_value(const char* p, int parent);

static const char* json_parse_string_raw(const char* p, char** out) {
    if (*p != '"') return p;
    p++;
    const char* start = p;
    while (*p && *p != '"') { if (*p == '\\') p++; p++; }
    int len = p - start;
    *out = malloc(len + 1);
    memcpy(*out, start, len);
    (*out)[len] = 0;
    if (*p == '"') p++;
    return p;
}

static const char* json_parse_value(const char* p, int parent) {
    p = json_skip_ws(p);
    int node = json_alloc_node();
    if (node < 0) return p;
    json_nodes[node].parent = parent;
    
    if (*p == '"') {
        json_nodes[node].type = 's';
        p = json_parse_string_raw(p, &json_nodes[node].str_val);
    } else if (*p == '{') {
        json_nodes[node].type = 'o';
        p++;
        int last_child = -1;
        p = json_skip_ws(p);
        while (*p && *p != '}') {
            p = json_skip_ws(p);
            char* key = NULL;
            p = json_parse_string_raw(p, &key);
            p = json_skip_ws(p);
            if (*p == ':') p++;
            int before = json_node_count;
            p = json_parse_value(p, node);
            if (before < json_node_count) {
                json_nodes[before].key = key;
                if (last_child >= 0) json_nodes[last_child].next_sibling = before;
                else json_nodes[node].first_child = before;
                last_child = before;
            }
            p = json_skip_ws(p);
            if (*p == ',') p++;
        }
        if (*p == '}') p++;
    } else if (*p == '[') {
        json_nodes[node].type = 'a';
        p++;
        int last_child = -1;
        p = json_skip_ws(p);
        while (*p && *p != ']') {
            int before = json_node_count;
            p = json_parse_value(p, node);
            if (before < json_node_count) {
                if (last_child >= 0) json_nodes[last_child].next_sibling = before;
                else json_nodes[node].first_child = before;
                last_child = before;
            }
            p = json_skip_ws(p);
            if (*p == ',') p++;
        }
        if (*p == ']') p++;
    } else if (*p == 't') { json_nodes[node].type = 'b'; json_nodes[node].num_val = 1; p += 4; }
    else if (*p == 'f') { json_nodes[node].type = 'b'; json_nodes[node].num_val = 0; p += 5; }
    else if (*p == 'n') { json_nodes[node].type = 'x'; p += 4; }
    else { json_nodes[node].type = 'n'; json_nodes[node].num_val = strtod(p, (char**)&p); }
    return p;
}

long long Json_parse(const char* text) {
    json_node_count = 0;
    json_parse_value(text, -1);
    return 0; // root node is always 0
}

static int json_find_child(int parent, const char* key) {
    int c = json_nodes[parent].first_child;
    while (c >= 0) {
        if (json_nodes[c].key && strcmp(json_nodes[c].key, key) == 0) return c;
        c = json_nodes[c].next_sibling;
    }
    return -1;
}

char* Json_get(long long root, const char* key) {
    int c = json_find_child((int)root, key);
    if (c < 0) return "";
    if (json_nodes[c].type == 's') return json_nodes[c].str_val ? json_nodes[c].str_val : "";
    if (json_nodes[c].type == 'n') { char* buf = malloc(32); snprintf(buf, 32, "%g", json_nodes[c].num_val); return buf; }
    if (json_nodes[c].type == 'b') return json_nodes[c].num_val ? "true" : "false";
    return "";
}

long long Json_get_int(long long root, const char* key) {
    int c = json_find_child((int)root, key);
    if (c < 0) return 0;
    return (long long)json_nodes[c].num_val;
}

long long Json_has(long long root, const char* key) {
    return json_find_child((int)root, key) >= 0 ? 1 : 0;
}

long long Json_array_len(long long node) {
    int n = (int)node;
    if (json_nodes[n].type != 'a') return 0;
    int count = 0;
    int c = json_nodes[n].first_child;
    while (c >= 0) { count++; c = json_nodes[c].next_sibling; }
    return count;
}

long long Json_array_get(long long node, long long index) {
    int c = json_nodes[(int)node].first_child;
    for (int i = 0; i < (int)index && c >= 0; i++) c = json_nodes[c].next_sibling;
    return c >= 0 ? c : -1;
}

char* Json_node_str(long long node) {
    int n = (int)node;
    if (n < 0 || n >= json_node_count) return "";
    if (json_nodes[n].type == 's') return json_nodes[n].str_val ? json_nodes[n].str_val : "";
    if (json_nodes[n].type == 'n') { char* buf = malloc(32); snprintf(buf, 32, "%g", json_nodes[n].num_val); return buf; }
    return "";
}

char* Json_keys(long long root) {
    char* result = malloc(4096); result[0] = 0;
    int c = json_nodes[(int)root].first_child;
    while (c >= 0) {
        if (json_nodes[c].key) { strcat(result, json_nodes[c].key); strcat(result, "\n"); }
        c = json_nodes[c].next_sibling;
    }
    return result;
}

// === Base64 ===
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
char* Encoding_base64_encode(const char* data) {
    int len = strlen(data);
    int out_len = 4 * ((len + 2) / 3);
    char* out = malloc(out_len + 1);
    int j = 0;
    for (int i = 0; i < len; i += 3) {
        int a = data[i], b = (i+1 < len) ? data[i+1] : 0, c = (i+2 < len) ? data[i+2] : 0;
        int triple = (a << 16) | (b << 8) | c;
        out[j++] = b64_table[(triple >> 18) & 0x3F];
        out[j++] = b64_table[(triple >> 12) & 0x3F];
        out[j++] = (i+1 < len) ? b64_table[(triple >> 6) & 0x3F] : '=';
        out[j++] = (i+2 < len) ? b64_table[triple & 0x3F] : '=';
    }
    out[j] = 0;
    return out;
}

static int b64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62; if (c == '/') return 63;
    return -1;
}

char* Encoding_base64_decode(const char* data) {
    int len = strlen(data);
    char* out = malloc(len);
    int j = 0;
    for (int i = 0; i < len; i += 4) {
        int a = b64_decode_char(data[i]), b = b64_decode_char(data[i+1]);
        int c = (i+2 < len && data[i+2] != '=') ? b64_decode_char(data[i+2]) : 0;
        int d = (i+3 < len && data[i+3] != '=') ? b64_decode_char(data[i+3]) : 0;
        int triple = (a << 18) | (b << 12) | (c << 6) | d;
        out[j++] = (triple >> 16) & 0xFF;
        if (i+2 < len && data[i+2] != '=') out[j++] = (triple >> 8) & 0xFF;
        if (i+3 < len && data[i+3] != '=') out[j++] = triple & 0xFF;
    }
    out[j] = 0;
    return out;
}

char* Encoding_hex_encode(const char* data) {
    int len = strlen(data);
    char* out = malloc(len * 2 + 1);
    for (int i = 0; i < len; i++) sprintf(out + i*2, "%02x", (unsigned char)data[i]);
    out[len*2] = 0;
    return out;
}

// === Crypto (SHA-256) ===
char* Crypto_sha256(const char* data) {
    // Use openssl command (POSIX)
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "printf '%%s' '%s' | openssl dgst -sha256 -hex 2>/dev/null | awk '{print $NF}'", data);
    FILE* fp = popen(cmd, "r");
    if (!fp) return "";
    char* result = malloc(65); result[0] = 0;
    fgets(result, 65, fp);
    pclose(fp);
    // Trim newline
    int len = strlen(result);
    if (len > 0 && result[len-1] == '\n') result[len-1] = 0;
    return result;
}

char* Crypto_md5(const char* data) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "printf '%%s' '%s' | openssl dgst -md5 -hex 2>/dev/null | awk '{print $NF}'", data);
    FILE* fp = popen(cmd, "r");
    if (!fp) return "";
    char* result = malloc(33); result[0] = 0;
    fgets(result, 33, fp);
    pclose(fp);
    int len = strlen(result);
    if (len > 0 && result[len-1] == '\n') result[len-1] = 0;
    return result;
}

// === Os module ===
char* Os_platform() {
    #ifdef __APPLE__
    return "macos";
    #elif __linux__
    return "linux";
    #elif _WIN32
    return "windows";
    #else
    return "unknown";
    #endif
}

char* Os_arch() {
    #ifdef __aarch64__
    return "arm64";
    #elif __x86_64__
    return "x86_64";
    #elif __i386__
    return "x86";
    #else
    return "unknown";
    #endif
}

char* Os_hostname() { static char buf[256]; gethostname(buf, sizeof(buf)); return buf; }
long long Os_pid() { return (long long)getpid(); }
char* Os_temp_dir() { return "/tmp"; }
char* Os_home_dir() { char* h = getenv("HOME"); return h ? h : "/tmp"; }

// === UUID v4 ===
char* Uuid_generate() {
    char* uuid = malloc(37);
    unsigned char bytes[16];
    FILE* f = fopen("/dev/urandom", "rb");
    if (f) { fread(bytes, 1, 16, f); fclose(f); }
    else { for (int i = 0; i < 16; i++) bytes[i] = rand() & 0xFF; }
    bytes[6] = (bytes[6] & 0x0F) | 0x40; // version 4
    bytes[8] = (bytes[8] & 0x3F) | 0x80; // variant
    snprintf(uuid, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],
        bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);
    return uuid;
}

// === Array extensions ===
long long array_pop_int(WynArray* arr) {
    if (arr->count <= 0) return 0;
    arr->count--;
    return arr->data[arr->count].data.int_val;
}


WynArray array_reverse_copy(WynArray arr) {
    WynArray result = array_new();
    for (int i = arr.count - 1; i >= 0; i--) array_push_int(&result, array_get_int(arr, i));
    return result;
}

char* array_join_str(WynArray arr, const char* sep) {
    char* result = malloc(65536); result[0] = 0;
    for (int i = 0; i < arr.count; i++) {
        if (i > 0) strcat(result, sep);
        if (arr.data[i].type == WYN_TYPE_STRING) strcat(result, arr.data[i].data.string_val);
        else { char buf[32]; snprintf(buf, 32, "%lld", (long long)arr.data[i].data.int_val); strcat(result, buf); }
    }
    return result;
}

long long array_index_of_int(WynArray arr, long long val) {
    for (int i = 0; i < arr.count; i++) if (array_get_int(arr, i) == val) return i;
    return -1;
}

void array_insert_at(WynArray* arr, int index, long long val) {
    if (index < 0) index = 0;
    if (index > arr->count) index = arr->count;
    array_push_int(arr, 0); // grow
    for (int i = arr->count - 1; i > index; i--) arr->data[i] = arr->data[i-1];
    arr->data[index].type = WYN_TYPE_INT;
    arr->data[index].data.int_val = val;
}

WynArray array_unique_int(WynArray arr) {
    WynArray result = array_new();
    for (int i = 0; i < arr.count; i++) {
        long long val = array_get_int(arr, i);
        int found = 0;
        for (int j = 0; j < result.count; j++) if (array_get_int(result, j) == val) { found = 1; break; }
        if (!found) array_push_int(&result, val);
    }
    return result;
}

// === HashMap extensions ===
extern void hashmap_remove(WynHashMap* map, const char* key);
extern void hashmap_clear(WynHashMap* map);

char* hashmap_values_string(WynHashMap* map) {
    if (!map) return "";
    char* result = malloc(65536); result[0] = 0;
    for (int i = 0; i < 128; i++) {
        void* entry = ((void**)map)[i]; // bucket
        // Walk chain — simplified, just use keys and get
    }
    // Use keys + get approach
    char* keys = hashmap_keys_string(map);
    const char* p = keys;
    while (*p) {
        const char* nl = strchr(p, '\n');
        if (!nl) break;
        int klen = nl - p;
        char key[256]; memcpy(key, p, klen); key[klen] = 0;
        char* val = hashmap_get_string(map, key);
        if (val && *val) { strcat(result, val); strcat(result, "\n"); }
        else {
            int ival = hashmap_get_int(map, key);
            char buf[32]; snprintf(buf, 32, "%d", ival);
            strcat(result, buf); strcat(result, "\n");
        }
        p = nl + 1;
    }
    return result;
}

// === Math extensions ===
double Math_log(double x) { return log(x); }
double Math_log10(double x) { return log10(x); }
double Math_exp(double x) { return exp(x); }
long long Math_clamp(long long x, long long lo, long long hi) { return x < lo ? lo : (x > hi ? hi : x); }
long long Math_sign(long long x) { return x > 0 ? 1 : (x < 0 ? -1 : 0); }

// === DateTime extensions ===
long long DateTime_diff(long long a, long long b) { return a - b; }
long long DateTime_add_seconds(long long t, long long n) { return t + n; }
char* DateTime_to_iso(long long timestamp) {
    time_t t = (time_t)timestamp;
    struct tm* tm = gmtime(&t);
    char* buf = malloc(32);
    strftime(buf, 32, "%Y-%m-%dT%H:%M:%SZ", tm);
    return buf;
}

// === Regex extensions ===
long long regex_find(const char* str, const char* pattern) {
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED) != 0) return -1;
    regmatch_t match;
    int result = regexec(&re, str, 1, &match, 0) == 0 ? match.rm_so : -1;
    regfree(&re);
    return result;
}

char* regex_find_all(const char* str, const char* pattern) {
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED) != 0) return "";
    char* result = malloc(65536); result[0] = 0;
    const char* p = str;
    regmatch_t match;
    while (regexec(&re, p, 1, &match, 0) == 0) {
        int len = match.rm_eo - match.rm_so;
        char* m = malloc(len + 1); memcpy(m, p + match.rm_so, len); m[len] = 0;
        strcat(result, m); strcat(result, "\n"); free(m);
        p += match.rm_eo;
    }
    regfree(&re);
    return result;
}

// === File extensions ===
int File_rename(const char* old_path, const char* new_path) { return rename(old_path, new_path) == 0; }

// === Test extensions ===
void Test_assert_eq_float(double actual, double expected, double epsilon, const char* msg) {
    extern void Test_assert(int condition, const char* message);
    double diff = actual - expected;
    if (diff < 0) diff = -diff;
    Test_assert(diff < epsilon, msg);
}

// === Net extensions ===
char* Net_resolve(const char* hostname) {
    struct addrinfo hints = {0}, *res;
    hints.ai_family = AF_INET;
    if (getaddrinfo(hostname, NULL, &hints, &res) != 0) return "";
    char* ip = malloc(INET_ADDRSTRLEN);
    struct sockaddr_in* addr = (struct sockaddr_in*)res->ai_addr;
    inet_ntop(AF_INET, &addr->sin_addr, ip, INET_ADDRSTRLEN);
    freeaddrinfo(res);
    return ip;
}

// === Db extensions ===
char* Db_escape(const char* str) {
    int len = strlen(str);
    char* out = malloc(len * 2 + 1);
    int j = 0;
    for (int i = 0; i < len; i++) {
        if (str[i] == '\'') { out[j++] = '\''; out[j++] = '\''; }
        else out[j++] = str[i];
    }
    out[j] = 0;
    return out;
}

// === Log Module ===
static int log_level = 0; // 0=debug, 1=info, 2=warn, 3=error
void Log_set_level(long long level) { log_level = (int)level; }
void Log_debug(const char* msg) { if (log_level <= 0) fprintf(stderr, "\x1b[90m[DEBUG %ld] %s\x1b[0m\n", (long)time(NULL), msg); }
void Log_info(const char* msg)  { if (log_level <= 1) fprintf(stderr, "\x1b[32m[INFO  %ld] %s\x1b[0m\n", (long)time(NULL), msg); }
void Log_warn(const char* msg)  { if (log_level <= 2) fprintf(stderr, "\x1b[33m[WARN  %ld] %s\x1b[0m\n", (long)time(NULL), msg); }
void Log_error(const char* msg) { if (log_level <= 3) fprintf(stderr, "\x1b[31m[ERROR %ld] %s\x1b[0m\n", (long)time(NULL), msg); }

// === Process Module ===
char* Process_exec_capture(const char* cmd) {
    // Run command and capture both stdout and stderr
    char full_cmd[4096];
    snprintf(full_cmd, sizeof(full_cmd), "%s 2>&1", cmd);
    FILE* fp = popen(full_cmd, "r");
    if (!fp) return "";
    char* result = malloc(131072);
    size_t len = fread(result, 1, 131071, fp);
    result[len] = 0;
    int status = pclose(fp);
    return result;
}

long long Process_exec_status(const char* cmd) {
    return WEXITSTATUS(system(cmd));
}

// === Http extensions ===
long long Http_get_json(const char* url) {
    char* body = http_get(url);
    if (!body || !*body) return -1;
    return Json_parse(body);
}

long long Http_post_json(const char* url, const char* data) {
    char* body = http_post(url, data);
    if (!body || !*body) return -1;
    return Json_parse(body);
}

// === Array extensions round 2 ===

long long wyn_arr_any(WynArray arr, long long (*pred)(long long)) {
    for (int i = 0; i < arr.count; i++) if (pred(array_get_int(arr, i))) return 1;
    return 0;
}

long long wyn_arr_all(WynArray arr, long long (*pred)(long long)) {
    for (int i = 0; i < arr.count; i++) if (!pred(array_get_int(arr, i))) return 0;
    return 1;
}

void wyn_arr_each(WynArray arr, void (*fn)(long long)) {
    for (int i = 0; i < arr.count; i++) fn(array_get_int(arr, i));
}

// === HashMap extensions round 2 ===
long long hashmap_get_or_int(WynHashMap* map, const char* key, long long default_val) {
    if (!hashmap_has(map, key)) return default_val;
    return hashmap_get_int(map, key);
}

char* hashmap_get_or_str(WynHashMap* map, const char* key, const char* default_val) {
    if (!hashmap_has(map, key)) return (char*)default_val;
    return hashmap_get_string(map, key);
}

// === Json extensions round 2 ===
double Json_get_float(long long root, const char* key) {
    int c = json_find_child((int)root, key);
    if (c < 0) return 0.0;
    return json_nodes[c].num_val;
}

long long Json_get_bool(long long root, const char* key) {
    int c = json_find_child((int)root, key);
    if (c < 0) return 0;
    return (long long)json_nodes[c].num_val;
}

long long Json_get_array(long long root, const char* key) {
    int c = json_find_child((int)root, key);
    if (c < 0 || json_nodes[c].type != 'a') return -1;
    return c;
}

long long Json_get_object(long long root, const char* key) {
    int c = json_find_child((int)root, key);
    if (c < 0 || json_nodes[c].type != 'o') return -1;
    return c;
}

// === File extensions round 2 ===
char* File_glob(const char* pattern) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "ls -1 %s 2>/dev/null", pattern);
    FILE* fp = popen(cmd, "r");
    if (!fp) return "";
    char* result = malloc(65536); result[0] = 0;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) strcat(result, line);
    pclose(fp);
    return result;
}

char* File_walk_dir(const char* path) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "find '%s' -type f 2>/dev/null", path);
    FILE* fp = popen(cmd, "r");
    if (!fp) return "";
    char* result = malloc(262144); result[0] = 0;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) strcat(result, line);
    pclose(fp);
    return result;
}

char* File_temp_file() {
    static int counter = 0;
    char* path = malloc(256);
    snprintf(path, 256, "/tmp/wyn_tmp_%d_%d", (int)getpid(), counter++);
    return path;
}

// === DateTime extensions round 2 ===
char* DateTime_format_duration(long long ms) {
    char* buf = malloc(64);
    if (ms < 1000) snprintf(buf, 64, "%lldms", ms);
    else if (ms < 60000) snprintf(buf, 64, "%.1fs", ms / 1000.0);
    else if (ms < 3600000) snprintf(buf, 64, "%lldm %llds", ms / 60000, (ms % 60000) / 1000);
    else snprintf(buf, 64, "%lldh %lldm", ms / 3600000, (ms % 3600000) / 60000);
    return buf;
}

long long DateTime_day_of_week(long long timestamp) {
    time_t t = (time_t)timestamp;
    struct tm* tm = localtime(&t);
    return tm ? tm->tm_wday : 0;
}

long long DateTime_year(long long timestamp) { time_t t = (time_t)timestamp; struct tm* tm = localtime(&t); return tm ? tm->tm_year + 1900 : 0; }
long long DateTime_month(long long timestamp) { time_t t = (time_t)timestamp; struct tm* tm = localtime(&t); return tm ? tm->tm_mon + 1 : 0; }
long long DateTime_day(long long timestamp) { time_t t = (time_t)timestamp; struct tm* tm = localtime(&t); return tm ? tm->tm_mday : 0; }
long long DateTime_hour(long long timestamp) { time_t t = (time_t)timestamp; struct tm* tm = localtime(&t); return tm ? tm->tm_hour : 0; }
long long DateTime_minute(long long timestamp) { time_t t = (time_t)timestamp; struct tm* tm = localtime(&t); return tm ? tm->tm_min : 0; }
long long DateTime_second(long long timestamp) { time_t t = (time_t)timestamp; struct tm* tm = localtime(&t); return tm ? tm->tm_sec : 0; }

// === Regex extensions round 2 ===
char* regex_split(const char* str, const char* pattern) {
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED) != 0) return strdup(str);
    char* result = malloc(strlen(str) + 256); result[0] = 0;
    const char* p = str;
    regmatch_t match;
    while (regexec(&re, p, 1, &match, 0) == 0) {
        strncat(result, p, match.rm_so);
        strcat(result, "\n");
        p += match.rm_eo;
    }
    strcat(result, p);
    regfree(&re);
    return result;
}

// === Encoding extensions round 2 ===
char* Encoding_hex_decode(const char* hex) {
    int len = strlen(hex) / 2;
    char* out = malloc(len + 1);
    for (int i = 0; i < len; i++) {
        unsigned int byte;
        sscanf(hex + i*2, "%2x", &byte);
        out[i] = (char)byte;
    }
    out[len] = 0;
    return out;
}

char* Encoding_csv_parse(const char* csv) {
    // Returns pipe-separated fields, newline-separated rows
    char* result = malloc(strlen(csv) + 256); result[0] = 0;
    const char* p = csv;
    while (*p) {
        if (*p == ',') strcat(result, "|");
        else if (*p == '\n') strcat(result, "\n");
        else { char c[2] = {*p, 0}; strcat(result, c); }
        p++;
    }
    return result;
}

// === Crypto extensions round 2 ===
char* Crypto_hmac_sha256(const char* key, const char* data) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "printf '%%s' '%s' | openssl dgst -sha256 -hmac '%s' -hex 2>/dev/null | awk '{print $NF}'", data, key);
    FILE* fp = popen(cmd, "r");
    if (!fp) return "";
    char* result = malloc(65); result[0] = 0;
    fgets(result, 65, fp); pclose(fp);
    int len = strlen(result);
    if (len > 0 && result[len-1] == '\n') result[len-1] = 0;
    return result;
}

char* Crypto_random_bytes(long long n) {
    char* bytes = malloc(n * 2 + 1);
    FILE* f = fopen("/dev/urandom", "rb");
    if (!f) { bytes[0] = 0; return bytes; }
    bytes[0] = 0;
    for (int i = 0; i < (int)n; i++) {
        unsigned char b;
        fread(&b, 1, 1, f);
        sprintf(bytes + i*2, "%02x", b);
    }
    fclose(f);
    return bytes;
}

// === StringBuilder extensions ===
void StringBuilder_append_int(long long handle, long long val) {
    char buf[32]; snprintf(buf, 32, "%lld", val);
    StringBuilder_append(handle, buf);
}

void StringBuilder_append_line(long long handle, const char* s) {
    StringBuilder_append(handle, s);
    StringBuilder_append(handle, "\n");
}

// === Math extensions round 2 ===
double Math_lerp(double a, double b, double t) { return a + (b - a) * t; }
double Math_map_range(double x, double in_min, double in_max, double out_min, double out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// === String extensions ===
long long string_char_code(const char* s, long long index) {
    if (!s || index < 0 || index >= (long long)strlen(s)) return 0;
    return (unsigned char)s[index];
}

// === Db extensions round 2 ===
#ifdef WYN_USE_SQLITE
long long Db_table_exists(long long handle, const char* table_name) {
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='%s'", table_name);
    char* result = Db_query_one(handle, sql);
    return result && strcmp(result, "0") != 0 ? 1 : 0;
}
#else
long long Db_table_exists(long long h, const char* t) { return 0; }
#endif

// === Test extensions ===
void Test_assert_not_contains(const char* haystack, const char* needle, const char* msg) {
    extern void Test_assert(int condition, const char* message);
    Test_assert(strstr(haystack, needle) == NULL, msg);
}

#endif // WYN_RUNTIME_H
