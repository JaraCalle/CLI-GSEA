#ifndef DIR_UTILS_H
#define DIR_UTILS_H

#include <stddef.h>

typedef struct {
    char **paths;
    size_t count;
} FileList;

void read_directory_recursive(const char *base_path, FileList *list);

#endif