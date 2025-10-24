#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define FILE_READ_BUFFER_SIZE 4096
#define MAX_PATH_LENGTH 1024

int read_file(const char* path, unsigned char** buffer, size_t* size);
int write_file(const char* path, const unsigned char* data, size_t size);
int get_file_info(const char* path, struct stat* stat_buf);
int file_exists(const char* path);
int is_directory(const char* path);
int create_directory(const char* path);
off_t get_file_size(const char* path);
int copy_file_permissions(const char* src_path, const char* dest_path);

#endif