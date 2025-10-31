#include "../include/dir_utils.h"
#include <stdio.h>    
#include <stdlib.h>   
#include <string.h>   
#include <dirent.h>   
#include <sys/stat.h> 
#include <errno.h>

void read_directory_recursive(const char *base_path, FileList *list) {
    DIR *dir = opendir(base_path);
    if (!dir) {
        perror("Error: no se pudo abrir el directorio");
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);
        
        struct stat st;
        if (stat(path, &st) == -1) {
            perror("Error: no se pudo obtener el estado del archivo");
            continue;
        }

        if (S_ISREG(st.st_mode)) {
            list->paths = realloc(list->paths, sizeof(char*) * (list->count + 1));
            list->paths[list->count] = strdup(path);
            list->count++;
        } else if (S_ISDIR(st.st_mode)) {
            read_directory_recursive(path, list);
        }
    }
    closedir(dir);
}        
