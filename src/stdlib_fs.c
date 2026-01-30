// Stdlib Filesystem API Implementation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

typedef struct {
    char** files;
    int count;
} DirListing;

DirListing Fs_read_dir(const char* path) {
    DirListing listing = {NULL, 0};
    DIR* dir = opendir(path);
    
    if (!dir) return listing;
    
    // Count entries first
    struct dirent* entry;
    int count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            count++;
        }
    }
    
    // Allocate array
    listing.files = malloc(count * sizeof(char*));
    listing.count = count;
    
    // Read entries
    rewinddir(dir);
    int i = 0;
    
    while ((entry = readdir(dir)) != NULL && i < count) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            listing.files[i] = strdup(entry->d_name);
            i++;
        }
    }
    
    closedir(dir);
    return listing;
}

int Fs_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

int Fs_is_file(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode);
}

int Fs_is_dir(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}
