// Example usage of Wyn I/O System
// This demonstrates the I/O API in a C context

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/io.h"

// Example: Basic file operations
void example_file_operations() {
    printf("=== File Operations Example ===\n");
    
    const char* filename = "example.txt";
    const char* content = "Hello, Wyn I/O System!\nThis is a test file.\n";
    
    // Create and write to file
    printf("Creating file: %s\n", filename);
    WynIoError error;
    WynFile* file = wyn_file_create(filename, &error);
    
    if (file) {
        error = wyn_file_write_string(file, content);
        if (error == WYN_IO_SUCCESS) {
            printf("✓ Successfully wrote content to file\n");
        } else {
            printf("✗ Error writing to file: %s\n", wyn_io_error_string(error));
        }
        wyn_file_free(file);
    } else {
        printf("✗ Error creating file: %s\n", wyn_io_error_string(error));
        return;
    }
    
    // Check file properties
    printf("\nFile properties:\n");
    printf("  Exists: %s\n", wyn_file_exists(filename) ? "yes" : "no");
    printf("  Is file: %s\n", wyn_is_file(filename) ? "yes" : "no");
    printf("  Is directory: %s\n", wyn_is_directory(filename) ? "yes" : "no");
    printf("  Size: %zu bytes\n", wyn_file_size(filename));
    
    // Read file content
    printf("\nReading file content:\n");
    file = wyn_file_open(filename, WYN_OPEN_READ, &error);
    if (file) {
        char* read_content;
        size_t length;
        error = wyn_file_read_to_string(file, &read_content, &length);
        
        if (error == WYN_IO_SUCCESS) {
            printf("Content (%zu bytes):\n%s", length, read_content);
            free(read_content);
        } else {
            printf("✗ Error reading file: %s\n", wyn_io_error_string(error));
        }
        wyn_file_free(file);
    }
    
    // Cleanup
    wyn_file_remove(filename);
    printf("✓ File removed\n");
}

// Example: File copying and renaming
void example_file_management() {
    printf("\n=== File Management Example ===\n");
    
    const char* original = "original.txt";
    const char* copy = "copy.txt";
    const char* renamed = "renamed.txt";
    const char* content = "This file will be copied and renamed.";
    
    // Create original file
    WynIoError error;
    WynFile* file = wyn_file_create(original, &error);
    wyn_file_write_string(file, content);
    wyn_file_free(file);
    printf("Created original file: %s\n", original);
    
    // Copy file
    error = wyn_file_copy(original, copy);
    if (error == WYN_IO_SUCCESS) {
        printf("✓ Successfully copied %s to %s\n", original, copy);
        printf("  Original size: %zu bytes\n", wyn_file_size(original));
        printf("  Copy size: %zu bytes\n", wyn_file_size(copy));
    } else {
        printf("✗ Error copying file: %s\n", wyn_io_error_string(error));
    }
    
    // Rename file
    error = wyn_file_rename(copy, renamed);
    if (error == WYN_IO_SUCCESS) {
        printf("✓ Successfully renamed %s to %s\n", copy, renamed);
        printf("  %s exists: %s\n", copy, wyn_file_exists(copy) ? "yes" : "no");
        printf("  %s exists: %s\n", renamed, wyn_file_exists(renamed) ? "yes" : "no");
    } else {
        printf("✗ Error renaming file: %s\n", wyn_io_error_string(error));
    }
    
    // Cleanup
    wyn_file_remove(original);
    wyn_file_remove(renamed);
    printf("✓ Files cleaned up\n");
}

// Example: Directory operations
void example_directory_operations() {
    printf("\n=== Directory Operations Example ===\n");
    
    const char* dir_name = "example_directory";
    
    // Create directory
    WynIoError error = wyn_dir_create(dir_name);
    if (error == WYN_IO_SUCCESS) {
        printf("✓ Created directory: %s\n", dir_name);
    } else {
        printf("✗ Error creating directory: %s\n", wyn_io_error_string(error));
        return;
    }
    
    // Create some files in the directory
    const char* filenames[] = {"file1.txt", "file2.txt", "data.log"};
    for (int i = 0; i < 3; i++) {
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s/%s", dir_name, filenames[i]);
        
        WynFile* file = wyn_file_create(filepath, &error);
        if (file) {
            char content[100];
            snprintf(content, sizeof(content), "Content of %s\n", filenames[i]);
            wyn_file_write_string(file, content);
            wyn_file_free(file);
            printf("  Created: %s\n", filepath);
        }
    }
    
    // List directory contents
    printf("\nDirectory contents:\n");
    WynDir* dir = wyn_dir_open(dir_name, &error);
    if (dir) {
        WynDirEntry entry;
        while (wyn_dir_read(dir, &entry) == WYN_IO_SUCCESS) {
            if (strcmp(entry.name, ".") != 0 && strcmp(entry.name, "..") != 0) {
                printf("  %s %s (%zu bytes)\n", 
                       entry.is_directory ? "[DIR]" : "[FILE]",
                       entry.name, entry.size);
            }
            free(entry.name);
        }
        wyn_dir_free(dir);
    } else {
        printf("✗ Error opening directory: %s\n", wyn_io_error_string(error));
    }
    
    // Cleanup - remove files first
    for (int i = 0; i < 3; i++) {
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s/%s", dir_name, filenames[i]);
        wyn_file_remove(filepath);
    }
    
    // Remove directory
    error = wyn_dir_remove(dir_name);
    if (error == WYN_IO_SUCCESS) {
        printf("✓ Removed directory: %s\n", dir_name);
    } else {
        printf("✗ Error removing directory: %s\n", wyn_io_error_string(error));
    }
}

// Example: Path manipulation
void example_path_operations() {
    printf("\n=== Path Operations Example ===\n");
    
    const char* sample_path = "/home/user/documents/project/file.txt";
    
    printf("Original path: %s\n", sample_path);
    
    // Extract components
    char* parent = wyn_path_parent(sample_path);
    char* filename = wyn_path_filename(sample_path);
    char* extension = wyn_path_extension(sample_path);
    
    printf("  Parent: %s\n", parent);
    printf("  Filename: %s\n", filename);
    printf("  Extension: %s\n", extension);
    printf("  Is absolute: %s\n", wyn_path_is_absolute(sample_path) ? "yes" : "no");
    
    free(parent);
    free(filename);
    free(extension);
    
    // Path joining
    const char* base = "/home/user";
    const char* components[] = {"documents", "projects", "wyn", "src"};
    
    char* current_path = strdup(base);
    printf("\nPath building:\n");
    printf("  Start: %s\n", current_path);
    
    for (int i = 0; i < 4; i++) {
        char* new_path = wyn_path_join(current_path, components[i]);
        free(current_path);
        current_path = new_path;
        printf("  + %s -> %s\n", components[i], current_path);
    }
    
    free(current_path);
    
    // Path normalization
    const char* messy_path = "/home//user///documents/./project/../file.txt";
    char* normalized = wyn_path_normalize(messy_path);
    printf("\nPath normalization:\n");
    printf("  Original: %s\n", messy_path);
    printf("  Normalized: %s\n", normalized);
    free(normalized);
}

// Example: File seeking and positioning
void example_file_seeking() {
    printf("\n=== File Seeking Example ===\n");
    
    const char* filename = "seek_example.txt";
    const char* content = "0123456789ABCDEFGHIJ";
    
    // Create file with content
    WynIoError error;
    WynFile* file = wyn_file_create(filename, &error);
    wyn_file_write_string(file, content);
    wyn_file_free(file);
    
    printf("Created file with content: %s\n", content);
    
    // Open for reading and demonstrate seeking
    file = wyn_file_open(filename, WYN_OPEN_READ, &error);
    if (file) {
        char buffer[6];
        size_t bytes_read;
        
        // Read from beginning
        printf("\nReading from different positions:\n");
        wyn_file_read(file, buffer, 5, &bytes_read);
        buffer[5] = '\0';
        printf("  Position 0-4: %s (position: %ld)\n", buffer, wyn_file_tell(file));
        
        // Seek to middle and read
        wyn_file_seek(file, 10, SEEK_SET);
        wyn_file_read(file, buffer, 5, &bytes_read);
        buffer[5] = '\0';
        printf("  Position 10-14: %s (position: %ld)\n", buffer, wyn_file_tell(file));
        
        // Seek to end and check position
        wyn_file_seek(file, 0, SEEK_END);
        printf("  End position: %ld\n", wyn_file_tell(file));
        
        // Seek back 5 characters from end
        wyn_file_seek(file, -5, SEEK_CUR);
        wyn_file_read(file, buffer, 5, &bytes_read);
        buffer[5] = '\0';
        printf("  Last 5 chars: %s\n", buffer);
        
        wyn_file_free(file);
    }
    
    // Cleanup
    wyn_file_remove(filename);
}

// Example: Error handling
void example_error_handling() {
    printf("\n=== Error Handling Example ===\n");
    
    WynIoError error;
    
    // Try to open non-existent file
    printf("Attempting to open non-existent file:\n");
    WynFile* file = wyn_file_open("does_not_exist.txt", WYN_OPEN_READ, &error);
    if (!file) {
        printf("  ✓ Expected error: %s\n", wyn_io_error_string(error));
    }
    
    // Try to open non-existent directory
    printf("\nAttempting to open non-existent directory:\n");
    WynDir* dir = wyn_dir_open("does_not_exist", &error);
    if (!dir) {
        printf("  ✓ Expected error: %s\n", wyn_io_error_string(error));
    }
    
    // Try to remove non-existent file
    printf("\nAttempting to remove non-existent file:\n");
    error = wyn_file_remove("does_not_exist.txt");
    if (error != WYN_IO_SUCCESS) {
        printf("  ✓ Expected error: %s\n", wyn_io_error_string(error));
    }
    
    // Show all error types
    printf("\nAll error types:\n");
    WynIoError errors[] = {
        WYN_IO_SUCCESS, WYN_IO_ERROR_NOT_FOUND, WYN_IO_ERROR_PERMISSION_DENIED,
        WYN_IO_ERROR_ALREADY_EXISTS, WYN_IO_ERROR_INVALID_PATH, WYN_IO_ERROR_DISK_FULL,
        WYN_IO_ERROR_READ_ONLY, WYN_IO_ERROR_INTERRUPTED, WYN_IO_ERROR_UNKNOWN
    };
    
    for (int i = 0; i < 9; i++) {
        printf("  %d: %s\n", errors[i], wyn_io_error_string(errors[i]));
    }
}

int main() {
    printf("Wyn I/O System Examples\n");
    printf("=======================\n");
    
    example_file_operations();
    example_file_management();
    example_directory_operations();
    example_path_operations();
    example_file_seeking();
    example_error_handling();
    
    printf("\n✅ All I/O examples completed successfully!\n");
    return 0;
}
