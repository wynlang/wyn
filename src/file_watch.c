// File watching implementation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "file_watch.h"

FileWatcher* file_watcher_create() {
    FileWatcher* watcher = malloc(sizeof(FileWatcher));
    watcher->files = malloc(sizeof(WatchedFile) * 16);
    watcher->count = 0;
    watcher->capacity = 16;
    return watcher;
}

void file_watcher_add(FileWatcher* watcher, const char* path) {
    if (watcher->count >= watcher->capacity) {
        watcher->capacity *= 2;
        watcher->files = realloc(watcher->files, sizeof(WatchedFile) * watcher->capacity);
    }
    
    struct stat st;
    if (stat(path, &st) != 0) return;
    
    watcher->files[watcher->count].path = strdup(path);
    watcher->files[watcher->count].last_modified = st.st_mtime;
    watcher->count++;
}

int file_watcher_check(FileWatcher* watcher) {
    int changed = 0;
    for (int i = 0; i < watcher->count; i++) {
        struct stat st;
        if (stat(watcher->files[i].path, &st) != 0) continue;
        
        if (st.st_mtime > watcher->files[i].last_modified) {
            watcher->files[i].last_modified = st.st_mtime;
            changed = 1;
        }
    }
    return changed;
}

void file_watcher_free(FileWatcher* watcher) {
    if (!watcher) return;
    for (int i = 0; i < watcher->count; i++) {
        free(watcher->files[i].path);
    }
    free(watcher->files);
    free(watcher);
}
