#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include "../include/file_manager.h"

int read_file(const char* path, unsigned char** buffer, size_t* size) {
    if (path == NULL || buffer == NULL || size == NULL) {
        fprintf(stderr, "Error: Parámetros inválidos en read_file\n");
        return -1;
    }

    int fd = -1;
    struct stat file_stat;
    
    // Abrir archivo en modo lectura
    fd = open(path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error: No se pudo abrir archivo '%s' - %s\n", 
                path, strerror(errno));
        return -1;
    }

    // Obtener información del archivo
    if (fstat(fd, &file_stat) == -1) {
        fprintf(stderr, "Error: No se pudo obtener información de '%s' - %s\n", 
                path, strerror(errno));
        close(fd);
        return -1;
    }

    // Verificar que es un archivo regular
    if (!S_ISREG(file_stat.st_mode)) {
        fprintf(stderr, "Error: '%s' no es un archivo regular\n", path);
        close(fd);
        return -1;
    }

    // Asignar memoria para el buffer
    *size = (size_t)file_stat.st_size;
    *buffer = (unsigned char*)malloc(*size);
    if (*buffer == NULL) {
        fprintf(stderr, "Error: No se pudo asignar memoria para leer '%s'\n", path);
        close(fd);
        return -1;
    }

    // Leer archivo completo
    ssize_t total_read = 0;
    ssize_t bytes_read;
    
    while (total_read < (ssize_t)*size) {
        bytes_read = read(fd, *buffer + total_read, *size - total_read);
        
        if (bytes_read == -1) {
            if (errno == EINTR) {
                continue; // Interrupción de sistema, reintentar
            }
            fprintf(stderr, "Error: Fallo al leer archivo '%s' - %s\n", 
                    path, strerror(errno));
            free(*buffer);
            *buffer = NULL;
            close(fd);
            return -1;
        }
        
        if (bytes_read == 0) {
            break; // EOF
        }
        
        total_read += bytes_read;
    }

    // Verificar que se leyó todo el archivo
    if (total_read != (ssize_t)*size) {
        fprintf(stderr, "Error: Solo se leyeron %zd de %zu bytes de '%s'\n", 
                total_read, *size, path);
        free(*buffer);
        *buffer = NULL;
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int write_file(const char* path, const unsigned char* data, size_t size) {
    if (path == NULL || data == NULL) {
        fprintf(stderr, "Error: Parámetros inválidos en write_file\n");
        return -1;
    }

    int fd = -1;
    
    // Crear directorio padre si no existe
    char parent_dir[MAX_PATH_LENGTH];
    strncpy(parent_dir, path, MAX_PATH_LENGTH - 1);
    parent_dir[MAX_PATH_LENGTH - 1] = '\0';
    
    char* last_slash = strrchr(parent_dir, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';
        if (strlen(parent_dir) > 0) {
            create_directory(parent_dir);
        }
    }

    // Abrir archivo en modo escritura (crear si no existe, truncar si existe)
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        fprintf(stderr, "Error: No se pudo crear/abrir archivo '%s' - %s\n", 
                path, strerror(errno));
        return -1;
    }

    // Escribir datos
    ssize_t total_written = 0;
    ssize_t bytes_written;
    
    while (total_written < (ssize_t)size) {
        bytes_written = write(fd, data + total_written, size - total_written);
        
        if (bytes_written == -1) {
            if (errno == EINTR) {
                continue; // Interrupción de sistema, reintentar
            }
            fprintf(stderr, "Error: Fallo al escribir archivo '%s' - %s\n", 
                    path, strerror(errno));
            close(fd);
            return -1;
        }
        
        total_written += bytes_written;
    }

    // Sincronizar datos con disco
    if (fsync(fd) == -1) {
        fprintf(stderr, "Warning: No se pudo sincronizar '%s' - %s\n", 
                path, strerror(errno));
        // No consideramos esto un error fatal
    }

    close(fd);
    return 0;
}

int get_file_info(const char* path, struct stat* stat_buf) {
    if (path == NULL || stat_buf == NULL) {
        return -1;
    }
    
    return stat(path, stat_buf);
}

int file_exists(const char* path) {
    if (path == NULL) {
        return 0;
    }
    
    struct stat stat_buf;
    return (stat(path, &stat_buf) == 0);
}

int is_directory(const char* path) {
    if (path == NULL) {
        return 0;
    }
    
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0) {
        return 0;
    }
    
    return S_ISDIR(stat_buf.st_mode);
}

int create_directory(const char* path) {
    if (path == NULL) {
        return -1;
    }
    
    // Verificar si ya existe
    if (file_exists(path)) {
        if (is_directory(path)) {
            return 0; // Ya existe y es directorio
        } else {
            fprintf(stderr, "Error: '%s' existe pero no es un directorio\n", path);
            return -1;
        }
    }

    // Crear directorio con permisos 755
    if (mkdir(path, 0755) == -1) {
        // Si falla, intentar crear directorios padres
        char parent_dir[MAX_PATH_LENGTH];
        strncpy(parent_dir, path, MAX_PATH_LENGTH - 1);
        parent_dir[MAX_PATH_LENGTH - 1] = '\0';
        
        char* last_slash = strrchr(parent_dir, '/');
        if (last_slash != NULL) {
            *last_slash = '\0';
            if (strlen(parent_dir) > 0) {
                if (create_directory(parent_dir) != 0) {
                    return -1;
                }
            }
            // Reintentar crear el directorio
            if (mkdir(path, 0755) == -1) {
                fprintf(stderr, "Error: No se pudo crear directorio '%s' - %s\n", 
                        path, strerror(errno));
                return -1;
            }
        } else {
            fprintf(stderr, "Error: No se pudo crear directorio '%s' - %s\n", 
                    path, strerror(errno));
            return -1;
        }
    }
    
    return 0;
}

off_t get_file_size(const char* path) {
    if (path == NULL) {
        return -1;
    }
    
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0) {
        return -1;
    }
    
    if (!S_ISREG(stat_buf.st_mode)) {
        return -1;
    }
    
    return stat_buf.st_size;
}

int copy_file_permissions(const char* src_path, const char* dest_path) {
    if (src_path == NULL || dest_path == NULL) {
        return -1;
    }
    
    struct stat src_stat;
    if (stat(src_path, &src_stat) != 0) {
        return -1;
    }
    
    if (chmod(dest_path, src_stat.st_mode & 0777) != 0) {
        fprintf(stderr, "Warning: No se pudieron copiar permisos de '%s' a '%s' - %s\n",
                src_path, dest_path, strerror(errno));
        return -1;
    }
    
    return 0;
}