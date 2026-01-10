#ifndef WYN_IO_H
#define WYN_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// Forward declarations
typedef struct WynFile WynFile;
typedef struct WynPath WynPath;
typedef struct WynDir WynDir;

// I/O Error types
typedef enum {
    WYN_IO_SUCCESS = 0,
    WYN_IO_ERROR_NOT_FOUND,
    WYN_IO_ERROR_PERMISSION_DENIED,
    WYN_IO_ERROR_ALREADY_EXISTS,
    WYN_IO_ERROR_INVALID_PATH,
    WYN_IO_ERROR_DISK_FULL,
    WYN_IO_ERROR_READ_ONLY,
    WYN_IO_ERROR_INTERRUPTED,
    WYN_IO_ERROR_UNKNOWN
} WynIoError;

// File open modes
typedef enum {
    WYN_OPEN_READ = 1,
    WYN_OPEN_WRITE = 2,
    WYN_OPEN_APPEND = 4,
    WYN_OPEN_CREATE = 8,
    WYN_OPEN_TRUNCATE = 16
} WynOpenMode;

// File structure
typedef struct WynFile {
    FILE* handle;
    char* path;
    WynOpenMode mode;
    bool is_open;
} WynFile;

// Path structure
typedef struct WynPath {
    char* path;
    bool is_absolute;
} WynPath;

// Directory entry
typedef struct {
    char* name;
    bool is_directory;
    size_t size;
} WynDirEntry;

// Directory structure
typedef struct WynDir {
    char* path;
    void* handle;  // Platform-specific directory handle
    bool is_open;
} WynDir;

// File operations
WynFile* wyn_file_open(const char* path, WynOpenMode mode, WynIoError* error);
WynFile* wyn_file_create(const char* path, WynIoError* error);
WynIoError wyn_file_close(WynFile* file);
void wyn_file_free(WynFile* file);

// File I/O operations
WynIoError wyn_file_read(WynFile* file, void* buffer, size_t size, size_t* bytes_read);
WynIoError wyn_file_write(WynFile* file, const void* data, size_t size, size_t* bytes_written);
WynIoError wyn_file_read_to_string(WynFile* file, char** content, size_t* length);
WynIoError wyn_file_write_string(WynFile* file, const char* content);
WynIoError wyn_file_flush(WynFile* file);
WynIoError wyn_file_seek(WynFile* file, long offset, int whence);
long wyn_file_tell(WynFile* file);

// File system operations
bool wyn_file_exists(const char* path);
bool wyn_is_directory(const char* path);
bool wyn_is_file(const char* path);
WynIoError wyn_file_remove(const char* path);
WynIoError wyn_file_rename(const char* old_path, const char* new_path);
WynIoError wyn_file_copy(const char* src_path, const char* dst_path);
size_t wyn_file_size(const char* path);

// Directory operations
WynIoError wyn_dir_create(const char* path);
WynIoError wyn_dir_remove(const char* path);
WynDir* wyn_dir_open(const char* path, WynIoError* error);
WynIoError wyn_dir_close(WynDir* dir);
void wyn_dir_free(WynDir* dir);
WynIoError wyn_dir_read(WynDir* dir, WynDirEntry* entry);

// Path operations
WynPath* wyn_path_new(const char* path);
void wyn_path_free(WynPath* path);
char* wyn_path_normalize(const char* path);
char* wyn_path_join(const char* base, const char* component);
char* wyn_path_parent(const char* path);
char* wyn_path_filename(const char* path);
char* wyn_path_extension(const char* path);
char* wyn_path_absolute(const char* path);
bool wyn_path_is_absolute(const char* path);

// Utility functions
const char* wyn_io_error_string(WynIoError error);
char* wyn_get_current_dir(void);
WynIoError wyn_set_current_dir(const char* path);

#endif // WYN_IO_H
