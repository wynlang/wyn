#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#include <setjmp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "wyn_interface.h"
#include "io.h"
#include "arc_runtime.h"
#include "concurrency.h"
#include "optional.h"
#include "result.h"
#include "hashmap.h"
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
bool wyn_c_init_lexer(const char* source);
void wyn_c_init_parser();
int wyn_c_parse_program();
void wyn_c_init_checker();
void wyn_c_check_program(int ast_ptr);
bool wyn_c_checker_had_error();
bool wyn_c_generate_code(int ast_ptr, const char* c_filename);
char* wyn_c_create_c_filename(const char* filename);
bool wyn_c_compile_to_binary(const char* c_filename, const char* wyn_filename);
bool wyn_c_remove_file(const char* filename);

const char* wyn_string_concat_safe(const char* left, const char* right);

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
    } data;
} WynValue;

typedef struct WynArray { WynValue* data; int count; int capacity; } WynArray;
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
    if (index < 0 || index >= arr.count) return 0;
    if (arr.data[index].type == WYN_TYPE_INT) return arr.data[index].data.int_val;
    return 0;
}
const char* array_get_str(WynArray arr, int index) {
    if (index < 0 || index >= arr.count) return "";
    if (arr.data[index].type == WYN_TYPE_STRING) return arr.data[index].data.string_val;
    return "";
}
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
bool string_contains(const char* str, const char* substr) {
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

bool map_has(WynMap map, const char* key) {
    for (int i = 0; i < map.count; i++) {
        if (strcmp((char*)map.keys[i], key) == 0) {
            return true;
        }
    }
    return false;
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
    memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);
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
    // For HTTPS, we'll use a simple approach - call curl if available
    char cmd[1024];
    snprintf(cmd, 1024, "curl -s '%s' 2>/dev/null", url);
    FILE* fp = popen(cmd, "r");
    if (!fp) return NULL;
    
    char* response = malloc(65536);
    size_t len = fread(response, 1, 65535, fp);
    response[len] = 0;
    pclose(fp);
    
    return len > 0 ? response : NULL;
}
char* https_post(const char* url, const char* data) {
    char cmd[2048];
    snprintf(cmd, 2048, "curl -s -X POST -d '%s' '%s' 2>/dev/null", data, url);
    FILE* fp = popen(cmd, "r");
    if (!fp) return NULL;
    
    char* response = malloc(65536);
    size_t len = fread(response, 1, 65535, fp);
    response[len] = 0;
    pclose(fp);
    
    return len > 0 ? response : NULL;
}

char* http_get(const char* url) { return http_request("GET", url, NULL); }
char* http_post(const char* url, const char* data) { return http_request("POST", url, data); }
char* http_put(const char* url, const char* data) { return http_request("PUT", url, data); }
char* http_delete(const char* url) { return http_request("DELETE", url, NULL); }

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

char* json_get_str(const char* json, const char* key) {
    char search[256];
    snprintf(search, 256, "\\\"%s\\\":", key);
    char* pos = strstr(json, search);
    if(!pos) return NULL;
    pos += strlen(search);
    while(*pos == ' ' || *pos == '\"') pos++;
    char* end = pos;
    while(*end && *end != '\"' && *end != ',' && *end != '}') end++;
    int len = end - pos;
    char* result = malloc(len + 1);
    strncpy(result, pos, len);
    result[len] = 0;
    return result;
}

int json_get_int(const char* json, const char* key) {
    char* val = json_get_str(json, key);
    return val ? atoi(val) : 0;
}

int json_get_bool(const char* json, const char* key) {
    char* val = json_get_str(json, key);
    if(!val) return 0;
    return strcmp(val, "true") == 0 || strcmp(val, "1") == 0;
}

int json_has_key(const char* json, const char* key) {
    char search[256];
    snprintf(search, 256, "\\\"%s\\\":", key);
    return strstr(json, search) != NULL;
}

char* json_stringify_int(int val) {
    char* r = malloc(20);
    sprintf(r, "%d", val);
    return r;
}

char* json_stringify_str(const char* val) {
    char* r = malloc(strlen(val) + 3);
    sprintf(r, "\\\"%s\\\"", val);
    return r;
}

char* json_stringify_bool(int val) {
    return val ? "true" : "false";
}

char* json_array_stringify(int* arr, int len) {
    char* r = malloc(len * 20 + 10);
    strcpy(r, "[");
    for(int i = 0; i < len; i++) {
        char buf[20];
        sprintf(buf, "%d", arr[i]);
        strcat(r, buf);
        if(i < len-1) strcat(r, ",");
    }
    strcat(r, "]");
    return r;
}

int json_array_length(const char* json) {
    int count = 0;
    int depth = 0;
    for(const char* p = json; *p; p++) {
        if(*p == '[') depth++;
        else if(*p == ']') depth--;
        else if(*p == ',' && depth == 1) count++;
    }
    return count > 0 ? count + 1 : 0;
}

char* json_array_get(const char* json, int index) {
    int count = 0;
    int depth = 0;
    const char* start = NULL;
    for(const char* p = json; *p; p++) {
        if(*p == '[') { depth++; if(depth == 1 && count == index) start = p + 1; }
        else if(*p == ']') depth--;
        else if(*p == ',' && depth == 1) {
            if(count == index && start) {
                int len = p - start;
                char* r = malloc(len + 1);
                strncpy(r, start, len);
                r[len] = 0;
                return r;
            }
            count++;
            if(count == index) start = p + 1;
        }
    }
    return NULL;
}

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

void print_int(int x) { printf("%d\n", x); }
void print_float(double x) { printf("%f\n", x); }
void print_str(const char* s) { printf("%s\n", s); }
void print_bool(bool b) { printf("%s\n", b ? "true" : "false"); }
void print_int_no_nl(int x) { printf("%d", x); }
void print_float_no_nl(double x) { printf("%f", x); }
void print_str_no_nl(const char* s) { printf("%s", s); }
void print_bool_no_nl(bool b) { printf("%s", b ? "true" : "false"); }
void print_array(WynArray arr) { printf("["); for(int i = 0; i < arr.count; i++) { if(i > 0) printf(", "); printf("%d", array_get_int(arr, i)); } printf("]\n"); }
void print_array_no_nl(WynArray arr) { printf("["); for(int i = 0; i < arr.count; i++) { if(i > 0) printf(", "); printf("%d", array_get_int(arr, i)); } printf("]"); }
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
void println() { printf("\n"); }
void print_debug(const char* label, int val) { printf("%s: %d\n", label, val); }
#define print(x) _Generic((x), \
    int: print_int, \
    double: print_float, \
    char*: print_str, \
    const char*: print_str, \
    bool: print_bool, \
    WynArray: print_array, \
    default: print_int)(x)

int input() { int x = 0; if (scanf("%d", &x) != 1) { while(getchar() != '\n') { /* clear input buffer */ } return 0; } return x; }
float input_float() { float x = 0.0f; if (scanf("%f", &x) != 1) { while(getchar() != '\n') { /* clear input buffer */ } return 0.0f; } return x; }
char* input_line() { static char buffer[1024]; if (fgets(buffer, sizeof(buffer), stdin)) { size_t len = strlen(buffer); if (len > 0 && buffer[len-1] == '\n') buffer[len-1] = '\0'; return buffer; } return ""; }
void printf_wyn(const char* format, ...) { va_list args; va_start(args, format); vprintf(format, args); va_end(args); }
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
char* str_repeat(const char* s, int count) { int len = strlen(s); char* r = malloc(len * count + 1); r[0] = 0; for(int i = 0; i < count; i++) strcat(r, s); return r; }
char* str_reverse(const char* s) { int len = strlen(s); char* r = malloc(len + 1); for(int i = 0; i < len; i++) r[i] = s[len-1-i]; r[len] = 0; return r; }
char* int_to_string(int x) { char* r = malloc(32); sprintf(r, "%d", x); return r; }
char* float_to_string(double x) { char* r = malloc(32); sprintf(r, "%g", x); return r; }
char* bool_to_string(bool x) { char* r = malloc(8); strcpy(r, x ? "true" : "false"); return r; }
char* str_to_string(const char* x) { return (char*)x; }
#define to_string(x) _Generic((x), \
    int: int_to_string, \
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
int str_parse_int(const char* s) { return atoi(s); }
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
int file_write(const char* path, const char* data) {
    last_error[0] = 0;
    FILE* f = fopen(path, "w");
    if(!f) { snprintf(last_error, 256, "Cannot write file: %s", path); return 0; }
    fputs(data, f);
    fclose(f);
    return 1;
}
int file_exists(const char* path) { FILE* f = fopen(path, "r"); if(f) { fclose(f); return 1; } return 0; }
int file_size(const char* path) {
    last_error[0] = 0;
    FILE* f = fopen(path, "r");
    if(!f) { snprintf(last_error, 256, "Cannot open file: %s", path); return -1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    return (int)sz;
}
int file_delete(const char* path) {
    last_error[0] = 0;
    int result = remove(path);
    if(result != 0) snprintf(last_error, 256, "Cannot delete file: %s", path);
    return result == 0;
}
int file_append(const char* path, const char* data) {
    last_error[0] = 0;
    FILE* f = fopen(path, "a");
    if(!f) { snprintf(last_error, 256, "Cannot append to file: %s", path); return 0; }
    fputs(data, f);
    fclose(f);
    return 1;
}
int file_copy(const char* src, const char* dst) {
    last_error[0] = 0;
    FILE* s = fopen(src, "r");
    if(!s) { snprintf(last_error, 256, "Cannot open source: %s", src); return 0; }
    FILE* d = fopen(dst, "w");
    if(!d) { snprintf(last_error, 256, "Cannot open destination: %s", dst); fclose(s); return 0; }
    char buf[4096];
    size_t n;
    while((n = fread(buf, 1, 4096, s)) > 0) fwrite(buf, 1, n, d);
    fclose(s);
    fclose(d);
    return 1;
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
int random_int(int max) { return rand() % max; }
int random_range(int min, int max) { return min + rand() % (max - min + 1); }
double random_float() { return (double)rand() / RAND_MAX; }
void seed_random(int seed) { srand(seed); }
int time_now() { return (int)time(NULL); }
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
char** str_split(const char* s, const char* delim, int* count) { char** r = malloc(100 * sizeof(char*)); *count = 0; char* copy = malloc(strlen(s) + 1); strcpy(copy, s); char* tok = strtok(copy, delim); while(tok && *count < 100) { r[*count] = malloc(strlen(tok) + 1); strcpy(r[*count], tok); (*count)++; tok = strtok(NULL, delim); } return r; }
char* str_join(char** arr, int len, const char* sep) { int total = 0; for(int i = 0; i < len; i++) total += strlen(arr[i]); total += (len - 1) * strlen(sep) + 1; char* r = malloc(total); r[0] = 0; for(int i = 0; i < len; i++) { if(i > 0) strcat(r, sep); strcat(r, arr[i]); } return r; }
char* int_to_str(int n) { char* r = malloc(12); sprintf(r, "%d", n); return r; }
int str_to_int(const char* s) { return atoi(s); }
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

typedef struct {
    int x;
    int y;
} Point;

void Point_cleanup(Point* obj) {
}

// Lambda functions (defined before use)
int Point_distance(int self);
int wyn_main();

int Point_distance(Point self) {
    return (self.x + self.y);
}

int wyn_main() {
    Point p = *(Point*)wyn_arc_new(sizeof(Point), &(Point){.x = 3, .y = 4})->data;
    ;
    return Point_distance(p);
}

