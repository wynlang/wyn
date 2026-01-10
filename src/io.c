#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#include "io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
    #include <sys/stat.h>
    #include "windows_compat.h"
    #define access _access
    #define F_OK 0
    #define mkdir(path, mode) _mkdir(path)
    #define rmdir _rmdir
    #define getcwd _getcwd
    #define chdir _chdir
    #define stat _stat
    #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
    #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
    #define dirname _dirname
    #define basename _basename
#else
    #include <sys/stat.h>
    #include <unistd.h>
    #include <dirent.h>
    #include <libgen.h>
#endif

// Convert system errno to WynIoError
static WynIoError errno_to_wyn_error(int err) {
    switch (err) {
        case ENOENT: return WYN_IO_ERROR_NOT_FOUND;
        case EACCES: return WYN_IO_ERROR_PERMISSION_DENIED;
        case EEXIST: return WYN_IO_ERROR_ALREADY_EXISTS;
        case EINVAL: return WYN_IO_ERROR_INVALID_PATH;
        case ENOSPC: return WYN_IO_ERROR_DISK_FULL;
        case EROFS: return WYN_IO_ERROR_READ_ONLY;
        case EINTR: return WYN_IO_ERROR_INTERRUPTED;
        default: return WYN_IO_ERROR_UNKNOWN;
    }
}

// Convert WynOpenMode to fopen mode string
static const char* mode_to_string(WynOpenMode mode) {
    if (mode & WYN_OPEN_WRITE) {
        if (mode & WYN_OPEN_READ) {
            if (mode & WYN_OPEN_TRUNCATE) return "w+";
            if (mode & WYN_OPEN_APPEND) return "a+";
            return "r+";
        } else {
            if (mode & WYN_OPEN_APPEND) return "a";
            return "w";
        }
    } else {
        return "r";
    }
}

// File operations
WynFile* wyn_file_open(const char* path, WynOpenMode mode, WynIoError* error) {
    if (!path) {
        if (error) *error = WYN_IO_ERROR_INVALID_PATH;
        return NULL;
    }
    
    WynFile* file = malloc(sizeof(WynFile));
    if (!file) {
        if (error) *error = WYN_IO_ERROR_UNKNOWN;
        return NULL;
    }
    
    const char* fmode = mode_to_string(mode);
    file->handle = fopen(path, fmode);
    
    if (!file->handle) {
        if (error) *error = errno_to_wyn_error(errno);
        free(file);
        return NULL;
    }
    
    file->path = strdup(path);
    file->mode = mode;
    file->is_open = true;
    
    if (error) *error = WYN_IO_SUCCESS;
    return file;
}

WynFile* wyn_file_create(const char* path, WynIoError* error) {
    return wyn_file_open(path, WYN_OPEN_WRITE | WYN_OPEN_CREATE | WYN_OPEN_TRUNCATE, error);
}

WynIoError wyn_file_close(WynFile* file) {
    if (!file || !file->is_open) return WYN_IO_ERROR_INVALID_PATH;
    
    if (fclose(file->handle) != 0) {
        return errno_to_wyn_error(errno);
    }
    
    file->is_open = false;
    return WYN_IO_SUCCESS;
}

void wyn_file_free(WynFile* file) {
    if (!file) return;
    
    if (file->is_open) {
        wyn_file_close(file);
    }
    
    free(file->path);
    free(file);
}

// File I/O operations
WynIoError wyn_file_read(WynFile* file, void* buffer, size_t size, size_t* bytes_read) {
    if (!file || !file->is_open || !buffer) return WYN_IO_ERROR_INVALID_PATH;
    
    size_t read = fread(buffer, 1, size, file->handle);
    
    if (bytes_read) *bytes_read = read;
    
    if (read < size && ferror(file->handle)) {
        return errno_to_wyn_error(errno);
    }
    
    return WYN_IO_SUCCESS;
}

WynIoError wyn_file_write(WynFile* file, const void* data, size_t size, size_t* bytes_written) {
    if (!file || !file->is_open || !data) return WYN_IO_ERROR_INVALID_PATH;
    
    size_t written = fwrite(data, 1, size, file->handle);
    
    if (bytes_written) *bytes_written = written;
    
    if (written < size) {
        return errno_to_wyn_error(errno);
    }
    
    return WYN_IO_SUCCESS;
}

WynIoError wyn_file_read_to_string(WynFile* file, char** content, size_t* length) {
    if (!file || !file->is_open || !content) return WYN_IO_ERROR_INVALID_PATH;
    
    // Get file size
    fseek(file->handle, 0, SEEK_END);
    long size = ftell(file->handle);
    fseek(file->handle, 0, SEEK_SET);
    
    if (size < 0) {
        return errno_to_wyn_error(errno);
    }
    
    // Allocate buffer
    char* buffer = malloc(size + 1);
    if (!buffer) {
        return WYN_IO_ERROR_UNKNOWN;
    }
    
    // Read file
    size_t bytes_read = fread(buffer, 1, size, file->handle);
    buffer[bytes_read] = '\0';
    
    *content = buffer;
    if (length) *length = bytes_read;
    
    return WYN_IO_SUCCESS;
}

WynIoError wyn_file_write_string(WynFile* file, const char* content) {
    if (!content) return WYN_IO_ERROR_INVALID_PATH;
    
    size_t len = strlen(content);
    size_t written;
    return wyn_file_write(file, content, len, &written);
}

WynIoError wyn_file_flush(WynFile* file) {
    if (!file || !file->is_open) return WYN_IO_ERROR_INVALID_PATH;
    
    if (fflush(file->handle) != 0) {
        return errno_to_wyn_error(errno);
    }
    
    return WYN_IO_SUCCESS;
}

WynIoError wyn_file_seek(WynFile* file, long offset, int whence) {
    if (!file || !file->is_open) return WYN_IO_ERROR_INVALID_PATH;
    
    if (fseek(file->handle, offset, whence) != 0) {
        return errno_to_wyn_error(errno);
    }
    
    return WYN_IO_SUCCESS;
}

long wyn_file_tell(WynFile* file) {
    if (!file || !file->is_open) return -1;
    return ftell(file->handle);
}

// File system operations
bool wyn_file_exists(const char* path) {
    if (!path) return false;
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES);
#else
    return access(path, F_OK) == 0;
#endif
}

bool wyn_is_directory(const char* path) {
    if (!path) return false;
    
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

bool wyn_is_file(const char* path) {
    if (!path) return false;
    
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

WynIoError wyn_file_remove(const char* path) {
    if (!path) return WYN_IO_ERROR_INVALID_PATH;
    
    if (remove(path) != 0) {
        return errno_to_wyn_error(errno);
    }
    
    return WYN_IO_SUCCESS;
}

WynIoError wyn_file_rename(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) return WYN_IO_ERROR_INVALID_PATH;
    
    if (rename(old_path, new_path) != 0) {
        return errno_to_wyn_error(errno);
    }
    
    return WYN_IO_SUCCESS;
}

WynIoError wyn_file_copy(const char* src_path, const char* dst_path) {
    if (!src_path || !dst_path) return WYN_IO_ERROR_INVALID_PATH;
    
    WynIoError error;
    WynFile* src = wyn_file_open(src_path, WYN_OPEN_READ, &error);
    if (!src) return error;
    
    WynFile* dst = wyn_file_create(dst_path, &error);
    if (!dst) {
        wyn_file_free(src);
        return error;
    }
    
    // Copy in chunks
    char buffer[4096];
    size_t bytes_read, bytes_written;
    
    while (true) {
        error = wyn_file_read(src, buffer, sizeof(buffer), &bytes_read);
        if (error != WYN_IO_SUCCESS || bytes_read == 0) break;
        
        error = wyn_file_write(dst, buffer, bytes_read, &bytes_written);
        if (error != WYN_IO_SUCCESS || bytes_written != bytes_read) break;
    }
    
    wyn_file_free(src);
    wyn_file_free(dst);
    
    return error;
}

size_t wyn_file_size(const char* path) {
    if (!path) return 0;
    
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return st.st_size;
}

// Directory operations
WynIoError wyn_dir_create(const char* path) {
    if (!path) return WYN_IO_ERROR_INVALID_PATH;
    
    if (mkdir(path, 0755) != 0) {
        return errno_to_wyn_error(errno);
    }
    
    return WYN_IO_SUCCESS;
}

WynIoError wyn_dir_remove(const char* path) {
    if (!path) return WYN_IO_ERROR_INVALID_PATH;
    
    if (rmdir(path) != 0) {
        return errno_to_wyn_error(errno);
    }
    
    return WYN_IO_SUCCESS;
}

WynDir* wyn_dir_open(const char* path, WynIoError* error) {
    if (!path) {
        if (error) *error = WYN_IO_ERROR_INVALID_PATH;
        return NULL;
    }
    
    WynDir* dir = malloc(sizeof(WynDir));
    if (!dir) {
        if (error) *error = WYN_IO_ERROR_UNKNOWN;
        return NULL;
    }
    
    DIR* handle = opendir(path);
    if (!handle) {
        if (error) *error = errno_to_wyn_error(errno);
        free(dir);
        return NULL;
    }
    
    dir->path = strdup(path);
    dir->handle = handle;
    dir->is_open = true;
    
    if (error) *error = WYN_IO_SUCCESS;
    return dir;
}

WynIoError wyn_dir_close(WynDir* dir) {
    if (!dir || !dir->is_open) return WYN_IO_ERROR_INVALID_PATH;
    
    if (closedir((DIR*)dir->handle) != 0) {
        return errno_to_wyn_error(errno);
    }
    
    dir->is_open = false;
    return WYN_IO_SUCCESS;
}

void wyn_dir_free(WynDir* dir) {
    if (!dir) return;
    
    if (dir->is_open) {
        wyn_dir_close(dir);
    }
    
    free(dir->path);
    free(dir);
}

WynIoError wyn_dir_read(WynDir* dir, WynDirEntry* entry) {
    if (!dir || !dir->is_open || !entry) return WYN_IO_ERROR_INVALID_PATH;
    
#ifdef _WIN32
    WIN32_FIND_DATA* find_data = (WIN32_FIND_DATA*)dir->handle;
    if (!FindNextFile((HANDLE)dir->handle, find_data)) {
        return WYN_IO_ERROR_NOT_FOUND;  // End of directory
    }
    
    entry->name = strdup(find_data->cFileName);
    entry->is_directory = (find_data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    
    // Get file size if it's a regular file
    if (!entry->is_directory) {
        entry->size = ((uint64_t)find_data->nFileSizeHigh << 32) | find_data->nFileSizeLow;
    } else {
        entry->size = 0;
    }
#else
    struct dirent* ent = readdir((DIR*)dir->handle);
    if (!ent) {
        return WYN_IO_ERROR_NOT_FOUND;  // End of directory
    }
    
    entry->name = strdup(ent->d_name);
    entry->is_directory = (ent->d_type == DT_DIR);
    
    // Get file size if it's a regular file
    if (!entry->is_directory) {
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir->path, ent->d_name);
        entry->size = wyn_file_size(full_path);
    } else {
        entry->size = 0;
    }
#endif
    
    return WYN_IO_SUCCESS;
}

// Path operations
WynPath* wyn_path_new(const char* path) {
    if (!path) return NULL;
    
    WynPath* p = malloc(sizeof(WynPath));
    if (!p) return NULL;
    
    p->path = wyn_path_normalize(path);
    p->is_absolute = wyn_path_is_absolute(path);
    
    return p;
}

void wyn_path_free(WynPath* path) {
    if (!path) return;
    free(path->path);
    free(path);
}

char* wyn_path_normalize(const char* path) {
    if (!path) return NULL;
    
    // Simple normalization - remove redundant slashes
    size_t len = strlen(path);
    char* normalized = malloc(len + 1);
    if (!normalized) return NULL;
    
    size_t j = 0;
    bool prev_slash = false;
    
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '/') {
            if (!prev_slash) {
                normalized[j++] = '/';
                prev_slash = true;
            }
        } else {
            normalized[j++] = path[i];
            prev_slash = false;
        }
    }
    
    normalized[j] = '\0';
    return normalized;
}

char* wyn_path_join(const char* base, const char* component) {
    if (!base || !component) return NULL;
    
    size_t base_len = strlen(base);
    size_t comp_len = strlen(component);
    bool needs_slash = (base_len > 0 && base[base_len - 1] != '/');
    
    size_t total_len = base_len + comp_len + (needs_slash ? 1 : 0);
    char* joined = malloc(total_len + 1);
    if (!joined) return NULL;
    
    strcpy(joined, base);
    if (needs_slash) {
        strcat(joined, "/");
    }
    strcat(joined, component);
    
    return joined;
}

char* wyn_path_parent(const char* path) {
    if (!path) return NULL;
    
    char* path_copy = strdup(path);
    if (!path_copy) return NULL;
    
    char* parent = dirname(path_copy);
    char* result = strdup(parent);
    
    free(path_copy);
    return result;
}

char* wyn_path_filename(const char* path) {
    if (!path) return NULL;
    
    char* path_copy = strdup(path);
    if (!path_copy) return NULL;
    
    char* filename = basename(path_copy);
    char* result = strdup(filename);
    
    free(path_copy);
    return result;
}

char* wyn_path_extension(const char* path) {
    if (!path) return NULL;
    
    const char* dot = strrchr(path, '.');
    const char* slash = strrchr(path, '/');
    
    // Make sure the dot is in the filename, not directory
    if (dot && (!slash || dot > slash)) {
        return strdup(dot + 1);
    }
    
    return strdup("");
}

char* wyn_path_absolute(const char* path) {
    if (!path) return NULL;
    
#ifdef _WIN32
    char* abs_path = _fullpath(NULL, path, 0);
    return abs_path;  // _fullpath allocates memory
#else
    char* abs_path = realpath(path, NULL);
    return abs_path;  // realpath allocates memory
#endif
}

bool wyn_path_is_absolute(const char* path) {
#ifdef _WIN32
    return path && (path[0] == '\\' || (path[1] == ':' && path[2] == '\\'));
#else
    return path && path[0] == '/';
#endif
}

// Utility functions
const char* wyn_io_error_string(WynIoError error) {
    switch (error) {
        case WYN_IO_SUCCESS: return "Success";
        case WYN_IO_ERROR_NOT_FOUND: return "File or directory not found";
        case WYN_IO_ERROR_PERMISSION_DENIED: return "Permission denied";
        case WYN_IO_ERROR_ALREADY_EXISTS: return "File already exists";
        case WYN_IO_ERROR_INVALID_PATH: return "Invalid path";
        case WYN_IO_ERROR_DISK_FULL: return "Disk full";
        case WYN_IO_ERROR_READ_ONLY: return "Read-only file system";
        case WYN_IO_ERROR_INTERRUPTED: return "Operation interrupted";
        default: return "Unknown error";
    }
}

char* wyn_get_current_dir(void) {
    return getcwd(NULL, 0);  // getcwd allocates memory
}

WynIoError wyn_set_current_dir(const char* path) {
    if (!path) return WYN_IO_ERROR_INVALID_PATH;
    
    if (chdir(path) != 0) {
        return errno_to_wyn_error(errno);
    }
    
    return WYN_IO_SUCCESS;
}
