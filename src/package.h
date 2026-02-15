#ifndef PACKAGE_H
#define PACKAGE_H

typedef struct {
    char name[128];
    char version[32];
    char description[256];
    char author[128];
} PackageInfo;

// Read package.wyn manifest
PackageInfo* read_package_manifest(const char* module_path);

// Free package info
void free_package_info(PackageInfo* info);

#endif // PACKAGE_H
