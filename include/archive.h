#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <stddef.h>
#include "args_parser.h"

// Estructura para un archivo dentro del archive
typedef struct {
    char *path;
    unsigned char *data;
    size_t size;
    size_t path_length;
} archive_file_t;

// Estructura para el archive completo
typedef struct {
    archive_file_t *files;
    size_t file_count;
    size_t total_size;
} archive_t;


archive_t* create_archive_from_dir(const char *dir_path);
void free_archive(archive_t *archive);
int serialize_archive(const archive_t *archive, unsigned char **data, size_t *size);
archive_t* deserialize_archive(const unsigned char *data, size_t size);
int extract_archive(const archive_t *archive, const char *output_dir);
int compress_and_encrypt_directory(const program_config_t *config);
int decrypt_and_decompress_directory(const program_config_t *config);
int is_serialized_archive(const unsigned char *data, size_t size);
int is_gsea_archive_file(const char *file_path);

#endif