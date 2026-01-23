// Package manager stub for wyn install
#ifndef PACKAGE_H
#define PACKAGE_H

// Install packages from wyn.toml
int package_install(const char* project_dir);

// List installed packages
int package_list();

#endif // PACKAGE_H
