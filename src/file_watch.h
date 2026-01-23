// File watching for wyn watch command
#ifndef FILE_WATCH_H
#define FILE_WATCH_H

#include <time.h>

typedef struct {
    char* path;
    time_t last_modified;
} WatchedFile;

typedef struct {
    WatchedFile* files;
    int count;
    int capacity;
} FileWatcher;

// Create file watcher
FileWatcher* file_watcher_create();

// Add file to watch
void file_watcher_add(FileWatcher* watcher, const char* path);

// Check if any files changed
int file_watcher_check(FileWatcher* watcher);

// Free watcher
void file_watcher_free(FileWatcher* watcher);

#endif // FILE_WATCH_H
