// Header for Wyn compiler C interface functions
#ifndef WYN_INTERFACE_H
#define WYN_INTERFACE_H

#include <stdbool.h>

// Initialize command line arguments
void wyn_init_args(int argc, char** argv);

// Get argument count
int wyn_get_argc(void);

// Get argument by index
const char* wyn_get_argv(int index);

// Read file contents (returns NULL on error)
char* wyn_read_file(const char* path);

// Write file contents (returns 1 on success, 0 on failure)
int wyn_write_file(const char* path, const char* content);

// Store argument in global and return validity
int wyn_store_argv(int index);

// Get stored filename validity
int wyn_get_filename_valid(void);

// Store file content in global and return validity
int wyn_store_file_content(const char* path);

// Get stored content validity
int wyn_get_content_valid(void);

// Stub implementations for v1.4 showcase (TODO: implement properly)
// Note: WynArray is defined in codegen, we just forward declare for stubs
typedef struct WynArray WynArray;

int _write(const char* path, const char* content);
int _exists(const char* path);
char* _read(const char* path);
long _file_size(const char* path);
char* _get_cwd(void);
long _now(void);
WynArray _list_dir(const char* path);
int _is_dir(const char* path);
int _is_file(const char* path);
const char* _extension(const char* path);
char* _path_join(const char* dir, const char* file);
const char* _basename(const char* path);
char* _dirname(const char* path);
const char* _env(const char* name);
int _set_env(const char* name, const char* value);
const char* _format(long timestamp);
WynArray _args(void);
int _exec_code(const char* cmd);

#endif // WYN_INTERFACE_H
