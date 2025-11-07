#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include "../include/archive.h"
#include "../include/file_manager.h"
#include "../include/compression.h"
#include "../include/compression_huffman.h"
#include "../include/encryption.h"
#include "../include/dir_utils.h"
#include "../include/concurrency.h"

// Formatos para serialización
#define ARCHIVE_MAGIC "GSEAARCHv1"
#define ARCHIVE_HEADER_SIZE 10

// Estructura para procesar archivos individualmente con hilos
typedef struct {
    const program_config_t *config;
    const char *input_dir;
    const char *output_dir;
    FileList *file_list;
    int *success_count;
    int *error_count;
    pthread_mutex_t *mutex;
} concurrent_processing_data_t;

char* process_output_path(const program_config_t *config) {
    // Si el usuario ya proporcionó un path de salida, usarlo
    if (strlen(config->output_path) > 0) {
        return strdup(config->output_path);
    }
    
    // Generar path de salida automático
    char *auto_path = generate_auto_output_path(config->input_path, config);
    if (auto_path) {
        printf("Salida automática generada: %s\n", auto_path);
        return auto_path;
    }
    
    // Fallback: usar nombre por defecto
    return strdup("salida.gsea");
}

const char* get_auto_extension(const program_config_t *config) {
    if ((config->operations & OP_COMPRESS) && (config->operations & OP_ENCRYPT)) {
        return ".gsea";
    } else if (config->operations & OP_COMPRESS) {
        return ".rle";
    } else if (config->operations & OP_ENCRYPT) {
        return ".enc";
    } else if (config->operations & OP_DECOMPRESS) {
        return "";
    } else if (config->operations & OP_DECRYPT) {
        return "";
    }
    return "";
}

char* generate_auto_output_path(const char *input_path, const program_config_t *config) {
    // Obtener el nombre base del archivo/directorio de entrada
    const char *base_name = strrchr(input_path, '/');
    if (base_name == NULL) {
        base_name = input_path;
    } else {
        base_name++; // Saltar el '/'
    }
    
    char *name_copy = NULL;
    
    // Si es un archivo (no directorio), quitar la extensión existente
    struct stat path_stat;
    if (stat(input_path, &path_stat) == 0 && S_ISREG(path_stat.st_mode)) {
        name_copy = strdup(base_name);
        if (name_copy) {
            char *dot = strrchr(name_copy, '.');
            if (dot) {
                *dot = '\0'; // Truncar en el punto
            }
            base_name = name_copy;
        }
    }
    
    // Generar la extensión automática
    const char *extension = get_auto_extension(config);
    
    // Construir el nombre completo
    size_t path_len = strlen(base_name) + strlen(extension) + 1;
    char *output_name = (char *)malloc(path_len);
    if (output_name) {
        snprintf(output_name, path_len, "%s%s", base_name, extension);
    }
    
    // Liberar memoria si hicimos copia
    if (name_copy) {
        free(name_copy);
    }
    
    return output_name;
}

void* process_file_for_archive(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    
    printf("Hilo %d: Procesando '%s'\n", data->thread_id, data->input_file);
    
    // Leer el archivo
    unsigned char *file_data = NULL;
    size_t file_size = 0;
    
    if (read_file(data->input_file, &file_data, &file_size) == 0) {
        data->success = 1;
        data->file_data = file_data;
        data->file_size = file_size;
        printf("Hilo %d: ✓ Leído '%s' (%zu bytes)\n", data->thread_id, data->input_file, file_size);
    } else {
        data->success = 0;
        data->file_data = NULL;
        data->file_size = 0;
        snprintf(data->error_message, sizeof(data->error_message), 
                 "Error leyendo '%s'", data->input_file);
        fprintf(stderr, "Hilo %d: ✗ Error leyendo '%s'\n", data->thread_id, data->input_file);
    }
    
    return NULL;
}

archive_t* create_archive_from_dir_concurrent(const char *dir_path) {
    printf("Creando archive desde directorio usando HILOS: %s\n", dir_path);
    
    // Leer todos los archivos del directorio
    FileList file_list = {0};
    read_directory_recursive(dir_path, &file_list);
    
    if (file_list.count == 0) {
        return NULL;
    }
    
    printf("Encontrados %ld archivos, creando %ld hilos...\n", 
           (long)file_list.count, (long)file_list.count);
    
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
    
    // Inicializar pool de hilos
    thread_pool_t thread_pool;
    if (init_thread_pool(&thread_pool, file_list.count) != 0) {
        fprintf(stderr, "Error: No se pudo inicializar el pool de hilos\n");
        free(archive->files);
        free(archive);
        return NULL;
    }
    
    // Configurar datos para cada hilo
    size_t base_path_len = strlen(dir_path);
    
    for (size_t i = 0; i < file_list.count; i++) {
        thread_data_t *data = &thread_pool.thread_data[i];
        
        data->config = NULL; // No necesitamos config para solo lectura
        data->input_file = file_list.paths[i];
        data->thread_id = (int)i;
        data->success = 0;
        data->file_data = NULL;
        data->file_size = 0;
        
        // Calcular ruta relativa para el archive
        const char *full_path = file_list.paths[i];
        const char *relative_path = full_path + base_path_len;
        if (*relative_path == '/') relative_path++;
        
        data->output_file = strdup(relative_path); // Usamos output_file para almacenar ruta relativa
        
        // Crear hilo para leer el archivo
        if (pthread_create(&thread_pool.threads[i], NULL, process_file_for_archive, data) != 0) {
            fprintf(stderr, "Error: No se pudo crear hilo para '%s'\n", file_list.paths[i]);
            free(data->output_file);
            thread_pool.thread_data[i].success = 0;
            continue;
        }
        
        thread_pool.active_threads++;
    }
    
    // Esperar a que todos los hilos terminen
    printf("Esperando a que %d hilos terminen de leer archivos...\n", thread_pool.active_threads);
    
    int success_count = 0;
    int error_count = 0;
    
    for (size_t i = 0; i < file_list.count; i++) {
        if (thread_pool.threads[i] != 0) {
            if (pthread_join(thread_pool.threads[i], NULL) != 0) {
                fprintf(stderr, "Error: No se pudo unir hilo %ld\n", i);
                error_count++;
            } else {
                thread_data_t *data = &thread_pool.thread_data[i];
                
                if (data->success && data->file_data != NULL) {
                    // Configurar el archivo en el archive
                    archive_file_t *file = &archive->files[success_count];
                    file->path = data->output_file; // La ruta relativa
                    file->path_length = strlen(file->path);
                    file->data = data->file_data;
                    file->size = data->file_size;
                    
                    archive->total_size += file->size;
                    success_count++;
                    
                    printf("✓ Archivo %d: '%s' (%zu bytes)\n", success_count, file->path, file->size);
                } else {
                    error_count++;
                    if (data->output_file) free(data->output_file);
                    if (data->file_data) free(data->file_data);
                }
            }
        }
    }
    
    // Actualizar contador real de archivos (solo los exitosos)
    archive->file_count = success_count;
    
    // Liberar recursos del pool
    free_thread_pool(&thread_pool);
    
    // Liberar lista de archivos (las rutas ya fueron copiadas o liberadas)
    for (size_t i = 0; i < file_list.count; i++) {
        free(file_list.paths[i]);
    }
    free(file_list.paths);
    
    printf("Archive creado con %d archivos (%d errores), total: %ld bytes\n",
           success_count, error_count, (long)archive->total_size);
    
    if (success_count == 0) {
        free_archive(archive);
        return NULL;
    }
    
    return archive;
}

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
    // Usar versión concurrente por defecto para mejor rendimiento
    return create_archive_from_dir_concurrent(dir_path);
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
    
    printf("Extrayendo %ld archivos usando HILOS...\n", (long)archive->file_count);
    
    // Usar procesamiento concurrente para extracción
    thread_pool_t thread_pool;
    if (init_thread_pool(&thread_pool, archive->file_count) != 0) {
        fprintf(stderr, "Error: No se pudo inicializar el pool de hilos para extracción\n");
        return -1;
    }
    
    int success_count = 0;
    int error_count = 0;
    
    // Crear hilos para escribir archivos
    for (size_t i = 0; i < archive->file_count; i++) {
        thread_data_t *data = &thread_pool.thread_data[i];
        const archive_file_t *file = &archive->files[i];
        
        data->config = NULL;
        data->thread_id = (int)i;
        data->success = 0;
        
        // Construir ruta completa de salida
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", output_dir, file->path);
        data->output_file = strdup(full_path);
        
        // Copiar datos del archivo (en una estructura temporal)
        data->file_data = (unsigned char*)malloc(file->size);
        if (!data->file_data) {
            fprintf(stderr, "Error de memoria para archivo '%s'\n", file->path);
            free(data->output_file);
            error_count++;
            continue;
        }
        memcpy(data->file_data, file->data, file->size);
        data->file_size = file->size;
        data->input_file = file->path; // Para logging
        
        // Crear hilo para escribir el archivo
        if (pthread_create(&thread_pool.threads[i], NULL, process_file_write, data) != 0) {
            fprintf(stderr, "Error: No se pudo crear hilo para '%s'\n", file->path);
            free(data->output_file);
            free(data->file_data);
            error_count++;
            continue;
        }
        
        thread_pool.active_threads++;
    }
    
    // Esperar a que todos los hilos terminen
    printf("Esperando a que %d hilos terminen de escribir archivos...\n", thread_pool.active_threads);
    
    for (size_t i = 0; i < archive->file_count; i++) {
        if (thread_pool.threads[i] != 0) {
            if (pthread_join(thread_pool.threads[i], NULL) != 0) {
                fprintf(stderr, "Error: No se pudo unir hilo %ld\n", i);
                error_count++;
            } else {
                if (thread_pool.thread_data[i].success) {
                    success_count++;
                    printf("✓ Extraído: '%s'\n", archive->files[i].path);
                } else {
                    error_count++;
                    fprintf(stderr, "✗ Error extrayendo: '%s'\n", archive->files[i].path);
                }
            }
            
            // Liberar memoria
            if (thread_pool.thread_data[i].output_file) {
                free(thread_pool.thread_data[i].output_file);
            }
            if (thread_pool.thread_data[i].file_data) {
                free(thread_pool.thread_data[i].file_data);
            }
        }
    }
    
    free_thread_pool(&thread_pool);
    
    printf("Extracción completada: %d exitosos, %d errores\n", success_count, error_count);
    
    return (error_count == 0) ? 0 : -1;
}

void* process_file_write(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    
    // Crear directorios padres si es necesario
    char *dir_path = strdup(data->output_file);
    char *dir_name = dirname(dir_path);
    if (create_directory(dir_name) != 0) {
        fprintf(stderr, "Hilo %d: Error creando directorio para '%s'\n", 
                data->thread_id, data->output_file);
        data->success = 0;
        free(dir_path);
        return NULL;
    }
    free(dir_path);
    
    // Escribir archivo
    if (write_file(data->output_file, data->file_data, data->file_size) == 0) {
        data->success = 1;
    } else {
        data->success = 0;
        fprintf(stderr, "Hilo %d: Error escribiendo '%s'\n", 
                data->thread_id, data->output_file);
    }
    
    return NULL;
}

int compress_directory_only(const program_config_t *config, const char *output_path) {
    printf("Comprimiendo directorio CON HILOS: %s\n", config->input_path);
    
    // Crear archive desde directorio USANDO HILOS
    archive_t *archive = create_archive_from_dir_concurrent(config->input_path);
    if (!archive) {
        fprintf(stderr, "Error: No se pudo crear archive del directorio\n");
        return -1;
    }
    
    printf("Archive creado con hilos: %ld archivos, %ld bytes totales\n", 
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
    compression_result_t compressed;
    if (config->comp_alg == COMP_ALG_RLE) {
        compressed = compress_rle(serialized_data, serialized_size);
    } else {
        compressed = compress_huffman_wrapper(serialized_data, serialized_size);
    }
    
    free(serialized_data);
    
    if (compressed.error != 0) {
        fprintf(stderr, "Error: No se pudo comprimir archive\n");
        return -1;
    }
    
    printf("Archive comprimido: %ld → %ld bytes (ratio: %.2f)\n",
           (long)serialized_size, (long)compressed.size,
           (double)compressed.size / serialized_size);
    
    // Escribir archivo final
    if (write_file(output_path, compressed.data, compressed.size) != 0) {
        fprintf(stderr, "Error: No se pudo escribir archivo de salida '%s'\n", output_path);
        free_compression_result(&compressed);
        return -1;
    }
    
    free_compression_result(&compressed);
    
    printf("Archive comprimido guardado en: %s\n", output_path);
    return 0;
}

int decompress_directory_only(const program_config_t *config, const char *output_path) {
    printf("Descomprimiendo archive CON HILOS: %s\n", config->input_path);
    
    // Leer archivo comprimido
    unsigned char *compressed_data = NULL;
    size_t compressed_size = 0;
    if (read_file(config->input_path, &compressed_data, &compressed_size) != 0) {
        fprintf(stderr, "Error: No se pudo leer archivo de entrada\n");
        return -1;
    }
    
    printf("Archive leído: %ld bytes\n", (long)compressed_size);
    
    // Descomprimir
    compression_result_t decompressed;
    if (config->comp_alg == COMP_ALG_RLE) {
        decompressed = decompress_rle(compressed_data, compressed_size);
    } else {
        decompressed = decompress_huffman_wrapper(compressed_data, compressed_size);
    }
    
    free(compressed_data);
    
    if (decompressed.error != 0) {
        fprintf(stderr, "Error: No se pudo descomprimir archive\n");
        return -1;
    }
    
    printf("Archive descomprimido: %ld → %ld bytes\n",
           (long)compressed_size, (long)decompressed.size);
    
    // Deserializar archive
    archive_t *archive = deserialize_archive(decompressed.data, decompressed.size);
    free_compression_result(&decompressed);
    
    if (!archive) {
        fprintf(stderr, "Error: No se pudo deserializar archive (formato inválido)\n");
        return -1;
    }
    
    printf("Archive deserializado: %ld archivos\n", (long)archive->file_count);
    
    // Extraer archive a directorio USANDO HILOS
    if (extract_archive(archive, output_path) != 0) {
        fprintf(stderr, "Error: No se pudo extraer archive\n");
        free_archive(archive);
        return -1;
    }
    
    free_archive(archive);
    
    printf("Archive extraído en: %s\n", output_path);
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
            if (size > 50) {
                result = 1;
            }
        }
        free(data);
    }
    
    return result;
}

int compress_and_encrypt_directory(const program_config_t *config, const char *output_path) {
    printf("Comprimiendo y encriptando directorio CON HILOS: %s\n", config->input_path);
    
    // Crear archive desde directorio USANDO HILOS
    archive_t *archive = create_archive_from_dir_concurrent(config->input_path);
    if (!archive) {
        fprintf(stderr, "Error: No se pudo crear archive del directorio\n");
        return -1;
    }
    
    printf("Archive creado con hilos: %ld archivos, %ld bytes totales\n", 
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
    compression_result_t compressed;
    if (config->comp_alg == COMP_ALG_RLE) {
        compressed = compress_rle(serialized_data, serialized_size);
    } else {
        compressed = compress_huffman_wrapper(serialized_data, serialized_size);
    }
    
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
    if (write_file(output_path, encrypted.data, encrypted.size) != 0) {
        fprintf(stderr, "Error: No se pudo escribir archivo de salida '%s'\n", output_path);
        free_encryption_result(&encrypted);
        return -1;
    }
    
    free_encryption_result(&encrypted);
    
    printf("Archive guardado en: %s\n", output_path);
    return 0;
}

int decrypt_and_decompress_directory(const program_config_t *config, const char *output_path) {
    printf("Desencriptando y descomprimiendo archive CON HILOS: %s\n", config->input_path);
    
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
    
    printf("Archive desencriptado: %ld → %ld bytes\n",
           (long)encrypted_size, (long)decrypted.size);
    
    // Descomprimir
    compression_result_t decompressed;
    if (config->comp_alg == COMP_ALG_RLE) {
        decompressed = decompress_rle(decrypted.data, decrypted.size);
    } else {
        decompressed = decompress_huffman_wrapper(decrypted.data, decrypted.size);
    }
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
    
    // Extraer archive a directorio USANDO HILOS
    if (extract_archive(archive, output_path) != 0) {
        fprintf(stderr, "Error: No se pudo extraer archive\n");
        free_archive(archive);
        return -1;
    }
    
    free_archive(archive);
    
    printf("Archive extraído en: %s\n", output_path);
    return 0;
}

int encrypt_directory_only(const program_config_t *config, const char *output_path) {
    printf("Encriptando directorio CON HILOS: %s\n", config->input_path);
    
    // Crear archive desde directorio USANDO HILOS
    archive_t *archive = create_archive_from_dir_concurrent(config->input_path);
    if (!archive) {
        fprintf(stderr, "Error: No se pudo crear archive del directorio\n");
        return -1;
    }
    
    printf("Archive creado con hilos: %ld archivos, %ld bytes totales\n", 
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
    
    // Encriptar datos serializados
    encryption_result_t encrypted = encrypt_vigenere(
        serialized_data, serialized_size,
        (const unsigned char *)config->key, strlen(config->key)
    );
    free(serialized_data);
    
    if (encrypted.error != 0) {
        fprintf(stderr, "Error: No se pudo encriptar archive\n");
        return -1;
    }
    
    printf("Archive encriptado: %ld bytes\n", (long)encrypted.size);
    
    // Escribir archivo final
    if (write_file(output_path, encrypted.data, encrypted.size) != 0) {
        fprintf(stderr, "Error: No se pudo escribir archivo de salida '%s'\n", output_path);
        free_encryption_result(&encrypted);
        return -1;
    }
    
    free_encryption_result(&encrypted);
    
    printf("Archive encriptado guardado en: %s\n", output_path);
    return 0;
}

int decrypt_directory_only(const program_config_t *config, const char *output_path) {
    printf("Desencriptando archive CON HILOS: %s\n", config->input_path);
    
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
    
    printf("Archive desencriptado: %ld → %ld bytes\n",
           (long)encrypted_size, (long)decrypted.size);
    
    // Deserializar archive
    archive_t *archive = deserialize_archive(decrypted.data, decrypted.size);
    free_encryption_result(&decrypted);
    
    if (!archive) {
        fprintf(stderr, "Error: No se pudo deserializar archive (formato inválido)\n");
        return -1;
    }
    
    printf("Archive deserializado: %ld archivos\n", (long)archive->file_count);
    
    // Extraer archive a directorio USANDO HILOS
    if (extract_archive(archive, output_path) != 0) {
        fprintf(stderr, "Error: No se pudo extraer archive\n");
        free_archive(archive);
        return -1;
    }
    
    free_archive(archive);
    
    printf("Archive extraído en: %s\n", output_path);
    return 0;
}