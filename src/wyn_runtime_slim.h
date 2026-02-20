// wyn_runtime_slim.h — types + forward declarations for optimized builds
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
typedef struct WynArray { WynValue* restrict data; int count; int capacity; } WynArray;
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
Future* wyn_spawn_async(TaskFuncWithReturn func, void* arg);
Future* future_new(void);
void future_set(Future* f, void* result);
void* future_get(Future* f);
void future_free(Future* f);
int future_is_ready(Future* f);

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
char* Json_get_string(long long j, const char* key);
long long Json_get_int(long long j, const char* key);
char* Json_stringify(long long j);
long long Json_parse(const char* s);
int Json_has(long long j, const char* key);
char* Json_to_pretty_string(long long j);

// File
char* File_read(const char* path);
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
void array_push_str(WynArray* arr, const char* value);
void array_push_array(WynArray* arr, WynArray* nested);
long long array_get_int(WynArray arr, int index);
const char* array_get_str(WynArray arr, int index);
WynValue array_get(WynArray arr, int index);
WynArray* array_get_array(WynArray arr, int index);
int array_get_nested_int(WynArray arr, int index1, int index2);
int array_get_nested3_int(WynArray arr, int index1, int index2, int index3);
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
int array_pop(WynArray* arr);
int array_index_of(WynArray arr, int value);
void array_reverse(WynArray* arr);
void array_sort(WynArray* arr);
int array_first(WynArray arr);
int array_last(WynArray arr);
int array_count(WynArray arr, int value);
void array_clear(WynArray* arr);
int array_min(WynArray arr);
int array_max(WynArray arr);
int array_sum(WynArray arr);
int array_average(WynArray arr);
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
static inline int string_contains(const char* str, const char* substr) { return strstr(str, substr) != NULL; }
char* string_concat(const char* a, const char* b);
static inline char* string_upper(const char* str) {
    size_t len = strlen(str);
    char* r = wyn_str_alloc(len);
    for (size_t i = 0; i < len; i++) r[i] = (str[i] >= 'a' && str[i] <= 'z') ? str[i] - 32 : str[i];
    r[len] = '\0';
    return r;
}
static inline char* string_lower(const char* str) {
    size_t len = strlen(str);
    char* r = wyn_str_alloc(len);
    for (size_t i = 0; i < len; i++) r[i] = (str[i] >= 'A' && str[i] <= 'Z') ? str[i] + 32 : str[i];
    r[len] = '\0';
    return r;
}
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
static inline char* string_replace(const char* str, const char* old, const char* new_str) {
    size_t ol = strlen(old), nl = strlen(new_str), sl = strlen(str);
    if (!ol) return wyn_strdup(str);
    // Count occurrences
    int cnt = 0; const char* p = str;
    while ((p = strstr(p, old))) { cnt++; p += ol; }
    if (!cnt) return wyn_strdup(str);
    char* r = wyn_str_alloc(sl + cnt * (nl - ol));
    char* d = r; p = str;
    while (*p) {
        const char* m = strstr(p, old);
        if (m == p) { memcpy(d, new_str, nl); d += nl; p += ol; }
        else if (m) { size_t n = m - p; memcpy(d, p, n); d += n; }
        else { strcpy(d, p); d += strlen(p); break; }
    }
    *d = '\0';
    return r;
}
static inline char* string_replace_all(const char* str, const char* old, const char* new_str) { return string_replace(str, old, new_str); }
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
void print_value(WynValue v);
void print_hex(int x);
void print_bin(int x);
void print_debug(const char* label, int val);
int input();
float input_float();
char* input_line();
void printf_wyn(const char* format, ...);
char* string_format(const char* format, ...);
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
long long wyn_safe_div(long long a, long long b);
long long wyn_safe_mod(long long a, long long b);
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
// File module — declared in module declarations block above
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
// Db declared in module declarations block above
long long Db_last_insert_id(long long handle);
char* Db_error(long long handle);
long long StringBuilder_new();
void StringBuilder_append(long long handle, const char* s);
long long StringBuilder_len(long long handle);
void StringBuilder_clear(long long handle);
void StringBuilder_free(long long handle);
// Json_parse declared in module declarations block above
char* Json_get(long long root, const char* key);
long long Json_get_int(long long root, const char* key);
long long Json_array_len(long long node);
long long Json_array_get(long long node, long long index);
char* Json_node_str(long long node);
char* Json_keys(long long root);
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

// Inline codegen helpers
static inline char* int_to_string(long long n) { static char __buf[32]; snprintf(__buf, sizeof(__buf), "%lld", n); return __buf; }
static inline char* float_to_string(double n) { static char __buf[64]; snprintf(__buf, sizeof(__buf), "%g", n); return __buf; }
static inline char* str_to_string(const char* s) { return (char*)s; }
static inline char* bool_to_string(bool b) { return b ? "true" : "false"; }

// _Generic macros for type-dispatched print/println/to_string
#define print_no_nl(x) _Generic((x), \
    int: print_int_no_nl, \
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

#define println(x) do { print(x); printf("\n"); } while(0)

#define to_string(x) _Generic((x), \
    int: int_to_string, \
    long: int_to_string, \
    long long: int_to_string, \
    double: float_to_string, \
    char*: str_to_string, \
    const char*: str_to_string, \
    bool: bool_to_string, \
    default: int_to_string)(x)

static inline void print_val(const char* s) { if(s) fputs(s, stdout); }
#endif
