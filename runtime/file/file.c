// File Module Implementation
#include "../wyn_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

char* wyn_file_read(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }
    
    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);
    
    return content;
}

int wyn_file_write(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    
    return (written == len) ? 0 : -1;
}

int wyn_file_exists(const char* path) {
    return access(path, F_OK) == 0 ? 1 : 0;
}

int wyn_file_is_file(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode) ? 1 : 0;
}

int wyn_file_is_dir(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

char* wyn_file_join(const char* a, const char* b) {
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    
    // Check if a ends with /
    int needs_slash = (len_a > 0 && a[len_a - 1] != '/') ? 1 : 0;
    
    char* result = malloc(len_a + len_b + needs_slash + 1);
    if (!result) return NULL;
    
    strcpy(result, a);
    if (needs_slash) strcat(result, "/");
    strcat(result, b);
    
    return result;
}

char* wyn_file_basename(const char* path) {
    char* path_copy = strdup(path);
    char* base = basename(path_copy);
    char* result = strdup(base);
    free(path_copy);
    return result;
}

char* wyn_file_dirname(const char* path) {
    char* path_copy = strdup(path);
    char* dir = dirname(path_copy);
    char* result = strdup(dir);
    free(path_copy);
    return result;
}

char* wyn_file_extension(const char* path) {
    const char* dot = strrchr(path, '.');
    const char* slash = strrchr(path, '/');
    
    // No extension or dot is part of directory name
    if (!dot || (slash && dot < slash)) {
        return strdup("");
    }
    
    return strdup(dot + 1);
}

char* wyn_file_cwd(void) {
    char* buf = malloc(4096);
    if (!buf) return NULL;
    
    if (getcwd(buf, 4096) == NULL) {
        free(buf);
        return NULL;
    }
    
    return buf;
}
