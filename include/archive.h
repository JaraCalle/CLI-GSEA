#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <stddef.h>
#include "args_parser.h"

// Funciones para nombres automáticos
const char* get_auto_extension(const program_config_t *config);
char* generate_auto_output_path(const char *input_path, const program_config_t *config);
char* process_output_path(const program_config_t *config);

// Funciones de directorio (actualizadas para aceptar output_path)
int compress_directory_only(const program_config_t *config, const char *output_path);
int encrypt_directory_only(const program_config_t *config, const char *output_path);
int compress_and_encrypt_directory(const program_config_t *config, const char *output_path);
int decompress_directory_only(const program_config_t *config, const char *output_path);
int decrypt_directory_only(const program_config_t *config, const char *output_path);
int decrypt_and_decompress_directory(const program_config_t *config, const char *output_path);

int is_gsea_archive_file(const char *file_path);
int create_directory_archive_file(const char *dir_path, const char *archive_path);
int extract_directory_archive_file(const char *archive_path, const char *output_dir);

#endif