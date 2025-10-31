#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/args_parser.h"
#include "../include/file_manager.h"
#include "../include/compression.h"
#include "../include/encryption.h"
#include "../include/concurrency.h"

int execute_compression_operations(const program_config_t *config, 
                                  const unsigned char *input_data, 
                                  size_t input_size,
                                  unsigned char **output_data, 
                                  size_t *output_size) {
    
    if (config->operations & OP_COMPRESS) {
        printf("  → Comprimiendo con algoritmo: ");
        switch (config->comp_alg) {
            case COMP_ALG_RLE:
                printf("RLE\n");
                compression_result_t compressed = compress_rle(input_data, input_size);
                if (compressed.error != 0) {
                    fprintf(stderr, "Error: Fallo en compresión RLE (código: %d)\n", compressed.error);
                    return -1;
                }
                
                *output_data = compressed.data;
                *output_size = compressed.size;
                
                double ratio = compression_ratio(input_size, compressed.size);
                printf("    ✓ Compresión completada: %zu → %zu bytes (ratio: %.2f)\n", 
                       input_size, compressed.size, ratio);
                break;
                
            case COMP_ALG_HUFFMAN:
                printf("Huffman\n");
                fprintf(stderr, "Error: Algoritmo Huffman no implementado aún\n");
                return -1;
                
            default:
                fprintf(stderr, "Error: Algoritmo de compresión no válido\n");
                return -1;
        }
    }
    else if (config->operations & OP_DECOMPRESS) {
        printf("  → Descomprimiendo con algoritmo: ");
        switch (config->comp_alg) {
            case COMP_ALG_RLE:
                printf("RLE\n");
                compression_result_t decompressed = decompress_rle(input_data, input_size);
                if (decompressed.error != 0) {
                    fprintf(stderr, "Error: Fallo en descompresión RLE (código: %d)\n", decompressed.error);
                    return -1;
                }
                
                *output_data = decompressed.data;
                *output_size = decompressed.size;
                printf("    ✓ Descompresión completada: %zu → %zu bytes\n", 
                       input_size, decompressed.size);
                break;
                
            case COMP_ALG_HUFFMAN:
                printf("Huffman\n");
                fprintf(stderr, "Error: Algoritmo Huffman no implementado aún\n");
                return -1;
                
            default:
                fprintf(stderr, "Error: Algoritmo de compresión no válido\n");
                return -1;
        }
    }
    else {
        // Sin operación de compresión, copiar datos
        *output_data = (unsigned char *)malloc(input_size);
        if (*output_data == NULL) {
            fprintf(stderr, "Error: No se pudo asignar memoria para datos de compresión\n");
            return -1;
        }
        memcpy(*output_data, input_data, input_size);
        *output_size = input_size;
    }
    
    return 0;
}

int execute_encryption_operations(const program_config_t *config, 
                                 const unsigned char *input_data, 
                                 size_t input_size,
                                 unsigned char **output_data, 
                                 size_t *output_size) {
    
    if (config->operations & OP_ENCRYPT) {
        printf("  → Encriptando con algoritmo: ");
        switch (config->enc_alg) {
            case ENC_ALG_VIGENERE:
                printf("Vigenère\n");
                encryption_result_t encrypted = encrypt_vigenere(
                    input_data, input_size, 
                    (const unsigned char *)config->key, strlen(config->key)
                );
                if (encrypted.error != 0) {
                    fprintf(stderr, "Error: Fallo en encriptación Vigenère (código: %d)\n", encrypted.error);
                    return -1;
                }
                
                *output_data = encrypted.data;
                *output_size = encrypted.size;
                printf("    ✓ Encriptación completada: %zu → %zu bytes\n", 
                       input_size, encrypted.size);
                break;
                
            default:
                fprintf(stderr, "Error: Algoritmo de encriptación no válido\n");
                return -1;
        }
    }
    else if (config->operations & OP_DECRYPT) {
        printf("  → Desencriptando con algoritmo: ");
        switch (config->enc_alg) {
            case ENC_ALG_VIGENERE:
                printf("Vigenère\n");
                encryption_result_t decrypted = decrypt_vigenere(
                    input_data, input_size, 
                    (const unsigned char *)config->key, strlen(config->key)
                );
                if (decrypted.error != 0) {
                    fprintf(stderr, "Error: Fallo en desencriptación Vigenère (código: %d)\n", decrypted.error);
                    return -1;
                }
                
                *output_data = decrypted.data;
                *output_size = decrypted.size;
                printf("    ✓ Desencriptación completada: %zu → %zu bytes\n", 
                       input_size, decrypted.size);
                break;
                
            default:
                fprintf(stderr, "Error: Algoritmo de encriptación no válido\n");
                return -1;
        }
    }
    else {
        // Sin operación de encriptación, copiar datos
        *output_data = (unsigned char *)malloc(input_size);
        if (*output_data == NULL) {
            fprintf(stderr, "Error: No se pudo asignar memoria para datos de encriptación\n");
            return -1;
        }
        memcpy(*output_data, input_data, input_size);
        *output_size = input_size;
    }
    
    return 0;
}

int execute_operations_sequential(const program_config_t *config, 
                                 const unsigned char *input_data, 
                                 size_t input_size,
                                 unsigned char **final_output, 
                                 size_t *final_size) {
    
    unsigned char *stage1_data = NULL;
    unsigned char *stage2_data = NULL;
    size_t stage1_size = 0;
    size_t stage2_size = 0;
    int result = 0;
    
    // Determinar el orden de operaciones basado en las flags
    if ((config->operations & OP_COMPRESS) && (config->operations & OP_ENCRYPT)) {
        printf("Orden de ejecución: COMPRIMIR → ENCRIPTAR\n");
        
        // Etapa 1: Compresión
        result = execute_compression_operations(config, input_data, input_size, 
                                              &stage1_data, &stage1_size);
        if (result != 0) {
            goto cleanup;
        }
        
        // Etapa 2: Encriptación
        result = execute_encryption_operations(config, stage1_data, stage1_size,
                                             &stage2_data, &stage2_size);
        if (result != 0) {
            goto cleanup;
        }
        
        *final_output = stage2_data;
        *final_size = stage2_size;
        stage2_data = NULL; // Transferir ownership
    }
    else if ((config->operations & OP_DECRYPT) && (config->operations & OP_DECOMPRESS)) {
        printf("Orden de ejecución: DESENCRIPTAR → DESCOMPRIMIR\n");
        
        // Etapa 1: Desencriptación
        result = execute_encryption_operations(config, input_data, input_size,
                                             &stage1_data, &stage1_size);
        if (result != 0) {
            goto cleanup;
        }
        
        // Etapa 2: Descompresión
        result = execute_compression_operations(config, stage1_data, stage1_size,
                                              &stage2_data, &stage2_size);
        if (result != 0) {
            goto cleanup;
        }
        
        *final_output = stage2_data;
        *final_size = stage2_size;
        stage2_data = NULL; // Transferir ownership
    }
    else {
        // Solo una operación o combinación simple
        printf("Orden de ejecución: OPERACIÓN ÚNICA\n");
        
        if (config->operations & (OP_COMPRESS | OP_DECOMPRESS)) {
            result = execute_compression_operations(config, input_data, input_size,
                                                  final_output, final_size);
        } else if (config->operations & (OP_ENCRYPT | OP_DECRYPT)) {
            result = execute_encryption_operations(config, input_data, input_size,
                                                 final_output, final_size);
        } else {
            // Sin operaciones específicas, copiar datos
            *final_output = (unsigned char *)malloc(input_size);
            if (*final_output == NULL) {
                fprintf(stderr, "Error: No se pudo asignar memoria para datos de salida\n");
                result = -1;
                goto cleanup;
            }
            memcpy(*final_output, input_data, input_size);
            *final_size = input_size;
        }
    }
    
cleanup:
    // Liberar memoria intermedia
    if (stage1_data != NULL) {
        free(stage1_data);
    }
    if (stage2_data != NULL) {
        free(stage2_data);
    }
    
    return result;
}

int execute_operations(const program_config_t *config) {
    printf("Iniciando procesamiento...\n");
    
    if (!config->valid) {
        fprintf(stderr, "Error: Configuración inválida\n");
        return -1;
    }
    
    // Verificar si la entrada es un directorio
    struct stat path_stat;
    if (stat(config->input_path, &path_stat) != 0) {
        fprintf(stderr, "Error: No se puede acceder a '%s'\n", config->input_path);
        return -1;
    }
    
    if (S_ISDIR(path_stat.st_mode)) {
        // Modo directorio: procesamiento concurrente
        printf("Entrada detectada como directorio - Activando modo concurrente\n");
        return process_directory_concurrent(config);
    } else if (S_ISREG(path_stat.st_mode)) {
        // Modo archivo único: procesamiento secuencial
        printf("Entrada detectada como archivo único - Modo secuencial\n");
        
        unsigned char *input_data = NULL;
        unsigned char *output_data = NULL;
        size_t input_size = 0;
        size_t output_size = 0;
        int result = 0;
        
        // Paso 1: Leer archivo de entrada
        printf("Paso 1: Leyendo archivo de entrada: %s\n", config->input_path);
        if (read_file(config->input_path, &input_data, &input_size) != 0) {
            fprintf(stderr, "Error: No se pudo leer el archivo de entrada '%s'\n", config->input_path);
            return -1;
        }
        printf("  ✓ Archivo leído correctamente (%zu bytes)\n", input_size);
        
        // Paso 2: Procesar datos (compresión/encriptación)
        printf("Paso 2: Procesando datos...\n");
        result = execute_operations_sequential(config, input_data, input_size, 
                                             &output_data, &output_size);
        
        // Liberar datos de entrada inmediatamente después del procesamiento
        free(input_data);
        input_data = NULL;
        
        if (result != 0) {
            if (output_data != NULL) {
                free(output_data);
            }
            return -1;
        }
        
        // Paso 3: Escribir archivo de salida
        printf("Paso 3: Escribiendo archivo de salida: %s\n", config->output_path);
        if (write_file(config->output_path, output_data, output_size) != 0) {
            fprintf(stderr, "Error: No se pudo escribir el archivo de salida '%s'\n", config->output_path);
            free(output_data);
            return -1;
        }
        printf("  ✓ Archivo escrito correctamente (%zu bytes)\n", output_size);
        
        // Liberar memoria final
        free(output_data);
        
        printf("Procesamiento completado exitosamente\n");
        return 0;
    } else {
        fprintf(stderr, "Error: La entrada '%s' no es un archivo regular ni un directorio\n", 
                config->input_path);
        return -1;
    }
}

void show_usage(const char *program_name) {
    printf("GSEA - Utilidad de Gestión Segura y Eficiente de Archivos\n");
    printf("Uso: %s [OPERACIONES] [OPCIONES]\n", program_name);
    printf("Use %s -h para ayuda completa\n", program_name);
}

int main(int argc, char *argv[]) {
    // Si no hay argumentos, mostrar uso básico
    if (argc == 1) {
        show_usage(argv[0]);
        return EXIT_SUCCESS;
    }
    
    // Parsear argumentos
    program_config_t config;
    
    if (parse_arguments(argc, argv, &config) != 0) {
        return EXIT_FAILURE;
    }
    
    // Mostrar configuración
    printf("=== GSEA - Configuración ===\n");
    printf("Operaciones: ");
    if (config.operations & OP_COMPRESS) printf("COMPRESS ");
    if (config.operations & OP_DECOMPRESS) printf("DECOMPRESS ");
    if (config.operations & OP_ENCRYPT) printf("ENCRYPT ");
    if (config.operations & OP_DECRYPT) printf("DECRYPT ");
    printf("\n");
    
    printf("Algoritmo compresión: ");
    switch (config.comp_alg) {
        case COMP_ALG_RLE: printf("RLE\n"); break;
        case COMP_ALG_HUFFMAN: printf("HUFFMAN\n"); break;
        default: printf("NONE\n"); break;
    }
    
    printf("Algoritmo encriptación: ");
    switch (config.enc_alg) {
        case ENC_ALG_VIGENERE: printf("VIGENERE\n"); break;
        default: printf("NONE\n"); break;
    }
    
    printf("Entrada: %s\n", config.input_path);
    printf("Salida: %s\n", config.output_path);
    if (config.key[0] != '\0') {
        printf("Clave: %s\n", config.key);
    }
    printf("=============================\n\n");
    
    // Ejecutar operaciones
    int operation_result = execute_operations(&config);
    
    if (operation_result != 0) {
        fprintf(stderr, "\nX Procesamiento falló con errores\n");
        return EXIT_FAILURE;
    }
    
    printf("\n✓ Procesamiento completado exitosamente\n");
    return EXIT_SUCCESS;
}