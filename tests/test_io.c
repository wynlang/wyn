#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../src/io.h"

void test_file_operations() {
    printf("Testing file operations...\n");
    
    const char* test_file = "test_file.txt";
    const char* test_content = "Hello, Wyn I/O System!";
    
    // Test file creation and writing
    WynIoError error;
    WynFile* file = wyn_file_create(test_file, &error);
    assert(file != NULL);
    assert(error == WYN_IO_SUCCESS);
    
    error = wyn_file_write_string(file, test_content);
    assert(error == WYN_IO_SUCCESS);
    
    wyn_file_free(file);
    
    // Test file existence
    assert(wyn_file_exists(test_file) == true);
    assert(wyn_is_file(test_file) == true);
    assert(wyn_is_directory(test_file) == false);
    
    // Test file reading
    file = wyn_file_open(test_file, WYN_OPEN_READ, &error);
    assert(file != NULL);
    assert(error == WYN_IO_SUCCESS);
    
    char* content;
    size_t length;
    error = wyn_file_read_to_string(file, &content, &length);
    assert(error == WYN_IO_SUCCESS);
    assert(strcmp(content, test_content) == 0);
    assert(length == strlen(test_content));
    
    free(content);
    wyn_file_free(file);
    
    // Test file size
    size_t size = wyn_file_size(test_file);
    assert(size == strlen(test_content));
    
    // Test file removal
    error = wyn_file_remove(test_file);
    assert(error == WYN_IO_SUCCESS);
    assert(wyn_file_exists(test_file) == false);
    
    printf("✓ File operations test passed\n");
}

void test_file_copy() {
    printf("Testing file copy operations...\n");
    
    const char* src_file = "source.txt";
    const char* dst_file = "destination.txt";
    const char* test_content = "Content to copy";
    
    // Create source file
    WynIoError error;
    WynFile* file = wyn_file_create(src_file, &error);
    assert(file != NULL);
    
    wyn_file_write_string(file, test_content);
    wyn_file_free(file);
    
    // Copy file
    error = wyn_file_copy(src_file, dst_file);
    assert(error == WYN_IO_SUCCESS);
    
    // Verify copy
    assert(wyn_file_exists(dst_file) == true);
    assert(wyn_file_size(dst_file) == strlen(test_content));
    
    file = wyn_file_open(dst_file, WYN_OPEN_READ, &error);
    char* content;
    wyn_file_read_to_string(file, &content, NULL);
    assert(strcmp(content, test_content) == 0);
    
    free(content);
    wyn_file_free(file);
    
    // Cleanup
    wyn_file_remove(src_file);
    wyn_file_remove(dst_file);
    
    printf("✓ File copy test passed\n");
}

void test_file_rename() {
    printf("Testing file rename operations...\n");
    
    const char* old_name = "old_file.txt";
    const char* new_name = "new_file.txt";
    const char* test_content = "Rename test content";
    
    // Create file
    WynIoError error;
    WynFile* file = wyn_file_create(old_name, &error);
    wyn_file_write_string(file, test_content);
    wyn_file_free(file);
    
    // Rename file
    error = wyn_file_rename(old_name, new_name);
    assert(error == WYN_IO_SUCCESS);
    
    // Verify rename
    assert(wyn_file_exists(old_name) == false);
    assert(wyn_file_exists(new_name) == true);
    
    // Verify content preserved
    file = wyn_file_open(new_name, WYN_OPEN_READ, &error);
    char* content;
    wyn_file_read_to_string(file, &content, NULL);
    assert(strcmp(content, test_content) == 0);
    
    free(content);
    wyn_file_free(file);
    wyn_file_remove(new_name);
    
    printf("✓ File rename test passed\n");
}

void test_directory_operations() {
    printf("Testing directory operations...\n");
    
    const char* test_dir = "test_directory";
    
    // Create directory
    WynIoError error = wyn_dir_create(test_dir);
    assert(error == WYN_IO_SUCCESS);
    
    // Test directory existence
    assert(wyn_file_exists(test_dir) == true);
    assert(wyn_is_directory(test_dir) == true);
    assert(wyn_is_file(test_dir) == false);
    
    // Create some files in directory
    char file_path[256];
    for (int i = 0; i < 3; i++) {
        snprintf(file_path, sizeof(file_path), "%s/file%d.txt", test_dir, i);
        WynFile* file = wyn_file_create(file_path, &error);
        assert(file != NULL);
        wyn_file_write_string(file, "test content");
        wyn_file_free(file);
    }
    
    // Test directory reading
    WynDir* dir = wyn_dir_open(test_dir, &error);
    assert(dir != NULL);
    assert(error == WYN_IO_SUCCESS);
    
    WynDirEntry entry;
    int file_count = 0;
    
    while (wyn_dir_read(dir, &entry) == WYN_IO_SUCCESS) {
        if (strcmp(entry.name, ".") != 0 && strcmp(entry.name, "..") != 0) {
            file_count++;
            assert(entry.name != NULL);
            // Files should not be directories
            if (strstr(entry.name, "file") != NULL) {
                assert(entry.is_directory == false);
            }
        }
        free(entry.name);
    }
    
    assert(file_count == 3);
    wyn_dir_free(dir);
    
    // Cleanup - remove files first
    for (int i = 0; i < 3; i++) {
        snprintf(file_path, sizeof(file_path), "%s/file%d.txt", test_dir, i);
        wyn_file_remove(file_path);
    }
    
    // Remove directory
    error = wyn_dir_remove(test_dir);
    assert(error == WYN_IO_SUCCESS);
    assert(wyn_file_exists(test_dir) == false);
    
    printf("✓ Directory operations test passed\n");
}

void test_path_operations() {
    printf("Testing path operations...\n");
    
    // Test path normalization
    char* normalized = wyn_path_normalize("/path//to///file.txt");
    assert(strcmp(normalized, "/path/to/file.txt") == 0);
    free(normalized);
    
    // Test path joining
    char* joined = wyn_path_join("/home/user", "documents/file.txt");
    assert(strcmp(joined, "/home/user/documents/file.txt") == 0);
    free(joined);
    
    joined = wyn_path_join("/home/user/", "documents");
    assert(strcmp(joined, "/home/user/documents") == 0);
    free(joined);
    
    // Test filename extraction
    char* filename = wyn_path_filename("/path/to/file.txt");
    assert(strcmp(filename, "file.txt") == 0);
    free(filename);
    
    // Test parent directory
    char* parent = wyn_path_parent("/path/to/file.txt");
    assert(strcmp(parent, "/path/to") == 0);
    free(parent);
    
    // Test extension extraction
    char* ext = wyn_path_extension("/path/to/file.txt");
    assert(strcmp(ext, "txt") == 0);
    free(ext);
    
    ext = wyn_path_extension("/path/to/file");
    assert(strcmp(ext, "") == 0);
    free(ext);
    
    // Test absolute path detection
    assert(wyn_path_is_absolute("/absolute/path") == true);
    assert(wyn_path_is_absolute("relative/path") == false);
    assert(wyn_path_is_absolute("./relative") == false);
    
    printf("✓ Path operations test passed\n");
}

void test_file_seek_tell() {
    printf("Testing file seek/tell operations...\n");
    
    const char* test_file = "seek_test.txt";
    const char* test_content = "0123456789";
    
    // Create test file
    WynIoError error;
    WynFile* file = wyn_file_create(test_file, &error);
    wyn_file_write_string(file, test_content);
    wyn_file_free(file);
    
    // Open for reading
    file = wyn_file_open(test_file, WYN_OPEN_READ, &error);
    assert(file != NULL);
    
    // Test initial position
    long pos = wyn_file_tell(file);
    assert(pos == 0);
    
    // Seek to middle
    error = wyn_file_seek(file, 5, SEEK_SET);
    assert(error == WYN_IO_SUCCESS);
    
    pos = wyn_file_tell(file);
    assert(pos == 5);
    
    // Read from middle
    char buffer[6];
    size_t bytes_read;
    error = wyn_file_read(file, buffer, 5, &bytes_read);
    assert(error == WYN_IO_SUCCESS);
    assert(bytes_read == 5);
    buffer[5] = '\0';
    assert(strcmp(buffer, "56789") == 0);
    
    // Seek to end
    error = wyn_file_seek(file, 0, SEEK_END);
    assert(error == WYN_IO_SUCCESS);
    
    pos = wyn_file_tell(file);
    assert(pos == (long)strlen(test_content));
    
    wyn_file_free(file);
    wyn_file_remove(test_file);
    
    printf("✓ File seek/tell test passed\n");
}

void test_current_directory() {
    printf("Testing current directory operations...\n");
    
    // Get current directory
    char* original_dir = wyn_get_current_dir();
    assert(original_dir != NULL);
    
    // Create test directory
    const char* test_dir = "temp_test_dir";
    WynIoError error = wyn_dir_create(test_dir);
    assert(error == WYN_IO_SUCCESS);
    
    // Change to test directory
    error = wyn_set_current_dir(test_dir);
    assert(error == WYN_IO_SUCCESS);
    
    // Verify current directory changed
    char* current_dir = wyn_get_current_dir();
    assert(strstr(current_dir, test_dir) != NULL);
    
    // Change back to original directory
    error = wyn_set_current_dir(original_dir);
    assert(error == WYN_IO_SUCCESS);
    
    // Cleanup
    wyn_dir_remove(test_dir);
    
    free(original_dir);
    free(current_dir);
    
    printf("✓ Current directory test passed\n");
}

void test_error_handling() {
    printf("Testing error handling...\n");
    
    WynIoError error;
    
    // Test opening non-existent file
    WynFile* file = wyn_file_open("non_existent_file.txt", WYN_OPEN_READ, &error);
    assert(file == NULL);
    assert(error == WYN_IO_ERROR_NOT_FOUND);
    
    // Test error string conversion
    const char* error_str = wyn_io_error_string(WYN_IO_ERROR_NOT_FOUND);
    assert(strcmp(error_str, "File or directory not found") == 0);
    
    error_str = wyn_io_error_string(WYN_IO_SUCCESS);
    assert(strcmp(error_str, "Success") == 0);
    
    // Test removing non-existent file
    error = wyn_file_remove("non_existent_file.txt");
    assert(error == WYN_IO_ERROR_NOT_FOUND);
    
    // Test opening non-existent directory
    WynDir* dir = wyn_dir_open("non_existent_dir", &error);
    assert(dir == NULL);
    assert(error == WYN_IO_ERROR_NOT_FOUND);
    
    printf("✓ Error handling test passed\n");
}

void test_edge_cases() {
    printf("Testing edge cases...\n");
    
    // Test NULL parameters
    assert(wyn_file_exists(NULL) == false);
    assert(wyn_is_file(NULL) == false);
    assert(wyn_is_directory(NULL) == false);
    assert(wyn_path_is_absolute(NULL) == false);
    
    WynIoError error;
    WynFile* file = wyn_file_open(NULL, WYN_OPEN_READ, &error);
    assert(file == NULL);
    assert(error == WYN_IO_ERROR_INVALID_PATH);
    
    // Test operations on closed file
    file = wyn_file_create("temp.txt", &error);
    assert(file != NULL);
    wyn_file_close(file);
    
    size_t bytes_read;
    char buffer[10];
    error = wyn_file_read(file, buffer, sizeof(buffer), &bytes_read);
    assert(error == WYN_IO_ERROR_INVALID_PATH);
    
    wyn_file_free(file);
    wyn_file_remove("temp.txt");
    
    printf("✓ Edge cases test passed\n");
}

int main() {
    printf("Running I/O System Tests...\n\n");
    
    test_file_operations();
    test_file_copy();
    test_file_rename();
    test_directory_operations();
    test_path_operations();
    test_file_seek_tell();
    test_current_directory();
    test_error_handling();
    test_edge_cases();
    
    printf("\n✅ All I/O system tests passed!\n");
    return 0;
}
