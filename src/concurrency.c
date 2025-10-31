#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>
#include "../include/concurrency.h"
#include "../include/file_manager.h"
#include "../include/compression.h"
#include "../include/encryption.h"
#include "../include/dir_utils.h"

int process_file_operations(const program_config_t *config, const char *input_path, const char *output_path) {
    // Leer archivo de entrada
    unsigned char *input_data = NULL;
    size_t input_size = 0;
    
    if (read_file(input_path, &input_data, &input_size) != 0) {
        fprintf(stderr, "Error: No se pudo leer el archivo '%s'\n", input_path);
        return -1;
    }
    
    unsigned char *processed_data = NULL;
    size_t processed_size = 0;
    
    // Determinar el orden de operaciones
    if ((config->operations & OP_COMPRESS) && (config->operations & OP_ENCRYPT)) {
        // Comprimir → Encriptar
        unsigned char *compressed_data = NULL;
        size_t compressed_size = 0;
        
        // Compresión
        compression_result_t compressed = compress_rle(input_data, input_size);
        if (compressed.error != 0) {
            fprintf(stderr, "Error en compresión RLE para '%s'\n", input_path);
            free(input_data);
            return -1;
        }
        compressed_data = compressed.data;
        compressed_size = compressed.size;
        
        // Encriptación
        encryption_result_t encrypted = encrypt_vigenere(
            compressed_data, compressed_size,
            (const unsigned char *)config->key, strlen(config->key)
        );
        if (encrypted.error != 0) {
            fprintf(stderr, "Error en encriptación Vigenère para '%s'\n", input_path);
            free(input_data);
            free_compression_result(&compressed);
            return -1;
        }
        
        processed_data = encrypted.data;
        processed_size = encrypted.size;
        free_compression_result(&compressed);
    }
    else if ((config->operations & OP_DECRYPT) && (config->operations & OP_DECOMPRESS)) {
        // Desencriptar → Descomprimir
        unsigned char *decrypted_data = NULL;
        size_t decrypted_size = 0;
        
        // Desencriptación
        encryption_result_t decrypted = decrypt_vigenere(
            input_data, input_size,
            (const unsigned char *)config->key, strlen(config->key)
        );
        if (decrypted.error != 0) {
            fprintf(stderr, "Error en desencriptación Vigenère para '%s'\n", input_path);
            free(input_data);
            return -1;
        }
        decrypted_data = decrypted.data;
        decrypted_size = decrypted.size;
        
        // Descompresión
        compression_result_t decompressed = decompress_rle(decrypted_data, decrypted_size);
        if (decompressed.error != 0) {
            fprintf(stderr, "Error en descompresión RLE para '%s'\n", input_path);
            free(input_data);
            free_encryption_result(&decrypted);
            return -1;
        }
        
        processed_data = decompressed.data;
        processed_size = decompressed.size;
        free_encryption_result(&decrypted);
    }
    else if (config->operations & (OP_COMPRESS | OP_DECOMPRESS)) {
        // Solo operaciones de compresión
        if (config->operations & OP_COMPRESS) {
            compression_result_t compressed = compress_rle(input_data, input_size);
            if (compressed.error != 0) {
                fprintf(stderr, "Error en compresión RLE para '%s'\n", input_path);
                free(input_data);
                return -1;
            }
            processed_data = compressed.data;
            processed_size = compressed.size;
        } else {
            compression_result_t decompressed = decompress_rle(input_data, input_size);
            if (decompressed.error != 0) {
                fprintf(stderr, "Error en descompresión RLE para '%s'\n", input_path);
                free(input_data);
                return -1;
            }
            processed_data = decompressed.data;
            processed_size = decompressed.size;
        }
    }
    else if (config->operations & (OP_ENCRYPT | OP_DECRYPT)) {
        // Solo operaciones de encriptación
        if (config->operations & OP_ENCRYPT) {
            encryption_result_t encrypted = encrypt_vigenere(
                input_data, input_size,
                (const unsigned char *)config->key, strlen(config->key)
            );
            if (encrypted.error != 0) {
                fprintf(stderr, "Error en encriptación Vigenère para '%s'\n", input_path);
                free(input_data);
                return -1;
            }
            processed_data = encrypted.data;
            processed_size = encrypted.size;
        } else {
            encryption_result_t decrypted = decrypt_vigenere(
                input_data, input_size,
                (const unsigned char *)config->key, strlen(config->key)
            );
            if (decrypted.error != 0) {
                fprintf(stderr, "Error en desencriptación Vigenère para '%s'\n", input_path);
                free(input_data);
                return -1;
            }
            processed_data = decrypted.data;
            processed_size = decrypted.size;
        }
    }
    else {
        // Sin operaciones específicas, copiar datos
        processed_data = (unsigned char *)malloc(input_size);
        if (processed_data == NULL) {
            fprintf(stderr, "Error de memoria para '%s'\n", input_path);
            free(input_data);
            return -1;
        }
        memcpy(processed_data, input_data, input_size);
        processed_size = input_size;
    }
    
    // Liberar datos de entrada
    free(input_data);
    
    // Escribir archivo de salida
    if (write_file(output_path, processed_data, processed_size) != 0) {
        fprintf(stderr, "Error: No se pudo escribir el archivo '%s'\n", output_path);
        free(processed_data);
        return -1;
    }
    
    free(processed_data);
    return 0;
}

void* process_single_file(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    
    printf("Hilo %d: Procesando '%s' → '%s'\n", 
           data->thread_id, data->input_file, data->output_file);
    
    // Procesar el archivo
    if (process_file_operations(data->config, data->input_file, data->output_file) == 0) {
        data->success = 1;
        printf("Hilo %d: ✓ Completado '%s'\n", data->thread_id, data->input_file);
    } else {
        data->success = 0;
        snprintf(data->error_message, sizeof(data->error_message), 
                 "Error procesando '%s'", data->input_file);
        fprintf(stderr, "Hilo %d: ✗ Falló '%s'\n", data->thread_id, data->input_file);
    }
    
    return NULL;
}

char* generate_output_path(const char *input_path, const char *output_dir, const program_config_t *config) {
    // Obtener el nombre del archivo sin la ruta
    const char *filename = strrchr(input_path, '/');
    if (filename == NULL) {
        filename = input_path;
    } else {
        filename++; // Saltar el '/'
    }
    
    // Determinar extensión basada en las operaciones
    const char *extension = "";
    if ((config->operations & OP_COMPRESS) && (config->operations & OP_ENCRYPT)) {
        extension = ".gsea";
    } else if (config->operations & OP_COMPRESS) {
        extension = ".rle";
    } else if (config->operations & OP_ENCRYPT) {
        extension = ".enc";
    } else if (config->operations & OP_DECOMPRESS) {
        extension = ".txt"; // Asumir texto para descompresión
    } else if (config->operations & OP_DECRYPT) {
        extension = ".dec";
    }
    
    // Construir ruta completa
    size_t path_len = strlen(output_dir) + strlen(filename) + strlen(extension) + 2;
    char *output_path = (char *)malloc(path_len);
    if (output_path == NULL) {
        return NULL;
    }
    
    snprintf(output_path, path_len, "%s/%s%s", output_dir, filename, extension);
    return output_path;
}

int init_thread_pool(thread_pool_t *pool, int num_threads) {
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);
    if (pool->threads == NULL) {
        return -1;
    }
    
    pool->thread_data = (thread_data_t *)malloc(sizeof(thread_data_t) * num_threads);
    if (pool->thread_data == NULL) {
        free(pool->threads);
        return -1;
    }
    
    if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
        free(pool->threads);
        free(pool->thread_data);
        return -1;
    }
    
    pool->num_threads = num_threads;
    pool->active_threads = 0;
    
    return 0;
}

void free_thread_pool(thread_pool_t *pool) {
    if (pool->threads != NULL) {
        free(pool->threads);
    }
    if (pool->thread_data != NULL) {
        free(pool->thread_data);
    }
    pthread_mutex_destroy(&pool->mutex);
}

int process_directory_concurrent(const program_config_t *config) {
    printf("Modo concurrente: Procesando directorio '%s'\n", config->input_path);
    
    // Leer todos los archivos del directorio
    FileList file_list = {0};
    read_directory_recursive(config->input_path, &file_list);
    
    if (file_list.count == 0) {
        printf("No se encontraron archivos en el directorio '%s'\n", config->input_path);
        return 0;
    }
    
    printf("Encontrados %ld archivos para procesar\n", (long)file_list.count);
    
    // Crear directorio de salida si no existe
    if (create_directory(config->output_path) != 0) {
        fprintf(stderr, "Error: No se pudo crear directorio de salida '%s'\n", config->output_path);
        
        // Liberar lista de archivos
        for (size_t i = 0; i < file_list.count; i++) {
            free(file_list.paths[i]);
        }
        free(file_list.paths);
        return -1;
    }
    
    // Inicializar pool de hilos
    thread_pool_t thread_pool;
    if (init_thread_pool(&thread_pool, file_list.count) != 0) {
        fprintf(stderr, "Error: No se pudo inicializar el pool de hilos\n");
        
        for (size_t i = 0; i < file_list.count; i++) {
            free(file_list.paths[i]);
        }
        free(file_list.paths);
        return -1;
    }
    
    // Crear hilos para cada archivo
    int success_count = 0;
    int error_count = 0;
    
    printf("Creando %ld hilos...\n", (long)file_list.count);
    
    for (size_t i = 0; i < file_list.count; i++) {
        thread_data_t *data = &thread_pool.thread_data[i];
        
        // Configurar datos del hilo
        data->config = config;  // Ahora es compatible con const
        data->input_file = file_list.paths[i];
        data->thread_id = (int)i;
        data->success = 0;
        
        // Generar ruta de salida
        data->output_file = generate_output_path(file_list.paths[i], config->output_path, config);
        if (data->output_file == NULL) {
            fprintf(stderr, "Error: No se pudo generar ruta de salida para '%s'\n", file_list.paths[i]);
            error_count++;
            continue;
        }
        
        // Crear hilo
        if (pthread_create(&thread_pool.threads[i], NULL, process_single_file, data) != 0) {
            fprintf(stderr, "Error: No se pudo crear hilo para '%s'\n", file_list.paths[i]);
            free(data->output_file);
            error_count++;
            continue;
        }
        
        thread_pool.active_threads++;
    }
    
    // Esperar a que todos los hilos terminen
    printf("Esperando a que %d hilos terminen...\n", thread_pool.active_threads);
    
    for (size_t i = 0; i < file_list.count; i++) {
        if (thread_pool.threads[i] != 0) { // Solo unir hilos que fueron creados
            if (pthread_join(thread_pool.threads[i], NULL) != 0) {
                fprintf(stderr, "Error: No se pudo unir hilo %ld\n", i);
                error_count++;
            } else {
                if (thread_pool.thread_data[i].success) {
                    success_count++;
                } else {
                    error_count++;
                }
            }
            
            // Liberar memoria de la ruta de salida
            if (thread_pool.thread_data[i].output_file != NULL) {
                free(thread_pool.thread_data[i].output_file);
            }
        }
    }
    
    // Liberar recursos
    for (size_t i = 0; i < file_list.count; i++) {
        free(file_list.paths[i]);
    }
    free(file_list.paths);
    free_thread_pool(&thread_pool);
    
    // Mostrar resumen
    printf("\n=== Resumen de procesamiento concurrente ===\n");
    printf("Archivos procesados exitosamente: %d\n", success_count);
    printf("Archivos con errores: %d\n", error_count);
    printf("Total: %ld\n", (long)file_list.count);
    
    if (error_count > 0) {
        return -1;
    }
    
    return 0;
}