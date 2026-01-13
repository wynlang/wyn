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

#endif // WYN_INTERFACE_H
