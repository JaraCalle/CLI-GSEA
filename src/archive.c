#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>
#include "../include/archive.h"
#include "../include/file_manager.h"
#include "../include/compression.h"
#include "../include/encryption.h"
#include "../include/dir_utils.h"

// Formatos para serialización
#define ARCHIVE_MAGIC "GSEAARCHv1"
#define ARCHIVE_HEADER_SIZE 10

size_t calculate_serialized_size(const archive_t *archive) {
    size_t total_size = ARCHIVE_HEADER_SIZE + sizeof(size_t);
    
    for (size_t i = 0; i < archive->file_count; i++) {
        total_size += sizeof(size_t);
        total_size += archive->files[i].path_length;
        total_size += sizeof(size_t);
        total_size += archive->files[i].size;
    }
    
    return total_size;
}

archive_t* create_archive_from_dir(const char *dir_path) {
    // Leer todos los archivos del directorio
    FileList file_list = {0};
    read_directory_recursive(dir_path, &file_list);
    
    if (file_list.count == 0) {
        return NULL;
    }
    
    // Crear estructura archive
    archive_t *archive = (archive_t*)malloc(sizeof(archive_t));
    if (!archive) {
        return NULL;
    }
    
    archive->files = (archive_file_t*)malloc(sizeof(archive_file_t) * file_list.count);
    if (!archive->files) {
        free(archive);
        return NULL;
    }
    
    archive->file_count = file_list.count;
    archive->total_size = 0;
    
    // Procesar cada archivo
    size_t base_path_len = strlen(dir_path);
    for (size_t i = 0; i < file_list.count; i++) {
        archive_file_t *file = &archive->files[i];
        
        // Calcular ruta relativa
        const char *full_path = file_list.paths[i];
        const char *relative_path = full_path + base_path_len;
        if (*relative_path == '/') relative_path++;
        
        file->path_length = strlen(relative_path);
        file->path = strdup(relative_path);
        if (!file->path) {
            // Liberar todo si hay error
            for (size_t j = 0; j < i; j++) {
                free(archive->files[j].path);
                free(archive->files[j].data);
            }
            free(archive->files);
            free(archive);
            return NULL;
        }
        
        // Leer datos del archivo
        if (read_file(full_path, &file->data, &file->size) != 0) {
            // Error leyendo archivo, continuar con el siguiente
            free(file->path);
            file->path = NULL;
            file->size = 0;
            continue;
        }
        
        archive->total_size += file->size;
    }
    
    // Liberar lista de archivos
    for (size_t i = 0; i < file_list.count; i++) {
        free(file_list.paths[i]);
    }
    free(file_list.paths);
    
    return archive;
}

void free_archive(archive_t *archive) {
    if (!archive) return;
    
    for (size_t i = 0; i < archive->file_count; i++) {
        free(archive->files[i].path);
        free(archive->files[i].data);
    }
    free(archive->files);
    free(archive);
}

int serialize_archive(const archive_t *archive, unsigned char **data, size_t *size) {
    if (!archive || !data || !size) return -1;
    
    *size = calculate_serialized_size(archive);
    *data = (unsigned char*)malloc(*size);
    if (!*data) return -1;
    
    unsigned char *ptr = *data;
    
    // Escribir magic number
    memcpy(ptr, ARCHIVE_MAGIC, ARCHIVE_HEADER_SIZE);
    ptr += ARCHIVE_HEADER_SIZE;
    
    // Escribir número de archivos
    memcpy(ptr, &archive->file_count, sizeof(size_t));
    ptr += sizeof(size_t);
    
    // Escribir cada archivo
    for (size_t i = 0; i < archive->file_count; i++) {
        const archive_file_t *file = &archive->files[i];
        
        // Escribir longitud de la ruta
        memcpy(ptr, &file->path_length, sizeof(size_t));
        ptr += sizeof(size_t);
        
        // Escribir ruta
        memcpy(ptr, file->path, file->path_length);
        ptr += file->path_length;
        
        // Escribir tamaño de datos
        memcpy(ptr, &file->size, sizeof(size_t));
        ptr += sizeof(size_t);
        
        // Escribir datos
        memcpy(ptr, file->data, file->size);
        ptr += file->size;
    }
    
    return 0;
}

archive_t* deserialize_archive(const unsigned char *data, size_t size) {
    if (!data || size < ARCHIVE_HEADER_SIZE + sizeof(size_t)) {
        return NULL;
    }
    
    const unsigned char *ptr = data;
    
    // Verificar header number
    if (memcmp(ptr, ARCHIVE_MAGIC, ARCHIVE_HEADER_SIZE) != 0) {
        return NULL;
    }
    ptr += ARCHIVE_HEADER_SIZE;
    
    // Leer número de archivos
    size_t file_count;
    memcpy(&file_count, ptr, sizeof(size_t));
    ptr += sizeof(size_t);
    
    // Crear archive
    archive_t *archive = (archive_t*)malloc(sizeof(archive_t));
    if (!archive) return NULL;
    
    archive->files = (archive_file_t*)malloc(sizeof(archive_file_t) * file_count);
    if (!archive->files) {
        free(archive);
        return NULL;
    }
    
    archive->file_count = file_count;
    archive->total_size = 0;
    
    // Leer cada archivo
    for (size_t i = 0; i < file_count; i++) {
        archive_file_t *file = &archive->files[i];
        
        // Leer longitud de la ruta
        memcpy(&file->path_length, ptr, sizeof(size_t));
        ptr += sizeof(size_t);
        
        if (ptr + file->path_length > data + size) {
            free_archive(archive);
            return NULL;
        }
        
        // Leer ruta
        file->path = (char*)malloc(file->path_length + 1);
        if (!file->path) {
            free_archive(archive);
            return NULL;
        }
        memcpy(file->path, ptr, file->path_length);
        file->path[file->path_length] = '\0';
        ptr += file->path_length;
        
        // Leer tamaño de datos
        memcpy(&file->size, ptr, sizeof(size_t));
        ptr += sizeof(size_t);
        
        if (ptr + file->size > data + size) {
            free_archive(archive);
            return NULL;
        }
        
        // Leer datos
        file->data = (unsigned char*)malloc(file->size);
        if (!file->data) {
            free_archive(archive);
            return NULL;
        }
        memcpy(file->data, ptr, file->size);
        ptr += file->size;
        
        archive->total_size += file->size;
    }
    
    return archive;
}

int extract_archive(const archive_t *archive, const char *output_dir) {
    if (!archive || !output_dir) return -1;
    
    // Crear directorio base si no existe
    if (create_directory(output_dir) != 0) {
        return -1;
    }
    
    for (size_t i = 0; i < archive->file_count; i++) {
        const archive_file_t *file = &archive->files[i];
        
        // Construir ruta completa de salida
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", output_dir, file->path);
        
        // Crear directorios padres si es necesario
        char *dir_path = strdup(full_path);
        char *dir_name = dirname(dir_path);
        if (create_directory(dir_name) != 0) {
            free(dir_path);
            continue;
        }
        free(dir_path);
        
        // Escribir archivo
        if (write_file(full_path, file->data, file->size) != 0) {
            fprintf(stderr, "Error escribiendo archivo: %s\n", full_path);
            continue;
        }
    }
    
    return 0;
}

int compress_and_encrypt_directory(const program_config_t *config) {
    printf("Creando archive del directorio: %s\n", config->input_path);
    
    // Crear archive desde directorio
    archive_t *archive = create_archive_from_dir(config->input_path);
    if (!archive) {
        fprintf(stderr, "Error: No se pudo crear archive del directorio\n");
        return -1;
    }
    
    printf("Archive creado: %ld archivos, %ld bytes totales\n", 
           (long)archive->file_count, (long)archive->total_size);
    
    // Serializar archive
    unsigned char *serialized_data = NULL;
    size_t serialized_size = 0;
    if (serialize_archive(archive, &serialized_data, &serialized_size) != 0) {
        fprintf(stderr, "Error: No se pudo serializar archive\n");
        free_archive(archive);
        return -1;
    }
    
    free_archive(archive);
    
    printf("Archive serializado: %ld bytes\n", (long)serialized_size);
    
    // Comprimir datos serializados
    compression_result_t compressed = compress_rle(serialized_data, serialized_size);
    free(serialized_data);
    
    if (compressed.error != 0) {
        fprintf(stderr, "Error: No se pudo comprimir archive\n");
        return -1;
    }
    
    printf("Archive comprimido: %ld → %ld bytes (ratio: %.2f)\n",
           (long)serialized_size, (long)compressed.size,
           (double)compressed.size / serialized_size);
    
    // Encriptar datos comprimidos
    encryption_result_t encrypted = encrypt_vigenere(
        compressed.data, compressed.size,
        (const unsigned char *)config->key, strlen(config->key)
    );
    free_compression_result(&compressed);
    
    if (encrypted.error != 0) {
        fprintf(stderr, "Error: No se pudo encriptar archive\n");
        return -1;
    }
    
    printf("Archive encriptado: %ld bytes\n", (long)encrypted.size);
    
    // Escribir archivo final
    if (write_file(config->output_path, encrypted.data, encrypted.size) != 0) {
        fprintf(stderr, "Error: No se pudo escribir archivo de salida\n");
        free_encryption_result(&encrypted);
        return -1;
    }
    
    free_encryption_result(&encrypted);
    
    printf("Archive guardado en: %s\n", config->output_path);
    return 0;
}

int decrypt_and_decompress_directory(const program_config_t *config) {
    printf("Cargando archive: %s\n", config->input_path);
    
    // Leer archivo encriptado
    unsigned char *encrypted_data = NULL;
    size_t encrypted_size = 0;
    if (read_file(config->input_path, &encrypted_data, &encrypted_size) != 0) {
        fprintf(stderr, "Error: No se pudo leer archivo de entrada\n");
        return -1;
    }
    
    printf("Archive leído: %ld bytes\n", (long)encrypted_size);
    
    // Desencriptar
    encryption_result_t decrypted = decrypt_vigenere(
        encrypted_data, encrypted_size,
        (const unsigned char *)config->key, strlen(config->key)
    );
    free(encrypted_data);
    
    if (decrypted.error != 0) {
        fprintf(stderr, "Error: No se pudo desencriptar archive\n");
        return -1;
    }
    
    printf("Archive desencriptado: %ld bytes\n", (long)decrypted.size);
    
    // Descomprimir
    compression_result_t decompressed = decompress_rle(decrypted.data, decrypted.size);
    free_encryption_result(&decrypted);
    
    if (decompressed.error != 0) {
        fprintf(stderr, "Error: No se pudo descomprimir archive\n");
        return -1;
    }
    
    printf("Archive descomprimido: %ld → %ld bytes\n",
           (long)decrypted.size, (long)decompressed.size);
    
    // Deserializar archive
    archive_t *archive = deserialize_archive(decompressed.data, decompressed.size);
    free_compression_result(&decompressed);
    
    if (!archive) {
        fprintf(stderr, "Error: No se pudo deserializar archive (formato inválido)\n");
        return -1;
    }
    
    printf("Archive deserializado: %ld archivos\n", (long)archive->file_count);
    
    // Extraer archive a directorio
    if (extract_archive(archive, config->output_path) != 0) {
        fprintf(stderr, "Error: No se pudo extraer archive\n");
        free_archive(archive);
        return -1;
    }
    
    free_archive(archive);
    
    printf("Archive extraído en: %s\n", config->output_path);
    return 0;
}

int is_serialized_archive(const unsigned char *data, size_t size) {
    if (!data || size < ARCHIVE_HEADER_SIZE) {
        return 0;
    }
    
    return (memcmp(data, ARCHIVE_MAGIC, ARCHIVE_HEADER_SIZE) == 0);
}

int is_gsea_archive_file(const char *file_path) {
    unsigned char *data = NULL;
    size_t size = 0;
    int result = 0;
    
    if (read_file(file_path, &data, &size) == 0) {
        // Primero verificar si es un archive serializado sin encriptar
        if (is_serialized_archive(data, size)) {
            result = 1;
        } else {
            // Si no, podría estar encriptado - intentar una verificación simple
            // Los archives GSEA suelen tener un tamaño mínimo razonable
            // y ciertos patrones después de desencriptar
            if (size > 50) { // Tamaño mínimo razonable para un archive
                // Podemos agregar más heurísticas aquí si es necesario
                result = 1; // Por ahora, asumir que cualquier archivo de tamaño decente es un archive
            }
        }
        free(data);
    }
    
    printf("DEBUG archive.c: is_gsea_archive_file('%s') = %d\n", file_path, result);
    return result;
}