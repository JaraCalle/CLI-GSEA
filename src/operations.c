#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/operations.h"
#include "../include/compression.h"
#include "../include/encryption.h"

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