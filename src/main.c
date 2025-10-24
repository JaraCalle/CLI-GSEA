#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/args_parser.h"
#include "../include/file_manager.h"
#include "../include/compression.h"

int execute_compression_operations(const program_config_t *config, 
                                  const unsigned char *input_data, 
                                  size_t input_size,
                                  unsigned char **output_data, 
                                  size_t *output_size) {
    
    if (config->operations & OP_COMPRESS) {
        printf("Comprimiendo con algoritmo: ");
        switch (config->comp_alg) {
            case COMP_ALG_RLE:
                printf("RLE\n");
                compression_result_t compressed = compress_rle(input_data, input_size);
                if (compressed.error != 0) {
                    fprintf(stderr, "Error en compresión RLE: %d\n", compressed.error);
                    return -1;
                }
                
                *output_data = compressed.data;
                *output_size = compressed.size;
                
                double ratio = compression_ratio(input_size, compressed.size);
                printf("Compresión completada: %zu -> %zu bytes (ratio: %.2f)\n", 
                       input_size, compressed.size, ratio);
                break;
                
            case COMP_ALG_HUFFMAN:
                printf("Huffman (no implementado aún)\n");
                // TODO: Implementar Huffman
                *output_data = (unsigned char *)malloc(input_size);
                memcpy(*output_data, input_data, input_size);
                *output_size = input_size;
                break;
                
            default:
                fprintf(stderr, "Error: Algoritmo de compresión no válido\n");
                return -1;
        }
    }
    else if (config->operations & OP_DECOMPRESS) {
        printf("Descomprimiendo con algoritmo: ");
        switch (config->comp_alg) {
            case COMP_ALG_RLE:
                printf("RLE\n");
                compression_result_t decompressed = decompress_rle(input_data, input_size);
                if (decompressed.error != 0) {
                    fprintf(stderr, "Error en descompresión RLE: %d\n", decompressed.error);
                    return -1;
                }
                
                *output_data = decompressed.data;
                *output_size = decompressed.size;
                printf("Descompresión completada: %zu -> %zu bytes\n", 
                       input_size, decompressed.size);
                break;
                
            case COMP_ALG_HUFFMAN:
                printf("Huffman (no implementado aún)\n");
                *output_data = (unsigned char *)malloc(input_size);
                memcpy(*output_data, input_data, input_size);
                *output_size = input_size;
                break;
                
            default:
                fprintf(stderr, "Error: Algoritmo de compresión no válido\n");
                return -1;
        }
    }
    else {
        // Sin operación de compresión, copiar datos
        *output_data = (unsigned char *)malloc(input_size);
        memcpy(*output_data, input_data, input_size);
        *output_size = input_size;
    }
    
    return 0;
}

int execute_operations(const program_config_t *config) {
    printf("Ejecutando operaciones...\n");
    
    if (!config->valid) {
        fprintf(stderr, "Error: Configuración inválida\n");
        return -1;
    }
    
    // Leer archivo de entrada
    unsigned char *input_data = NULL;
    size_t input_size = 0;
    
    printf("Leyendo archivo de entrada: %s\n", config->input_path);
    if (read_file(config->input_path, &input_data, &input_size) != 0) {
        fprintf(stderr, "Error: No se pudo leer el archivo de entrada\n");
        return -1;
    }
    
    printf("Archivo leído correctamente (%zu bytes)\n", input_size);
    
    // Procesar datos (compresión/descompresión)
    unsigned char *processed_data = NULL;
    size_t processed_size = 0;
    
    if (execute_compression_operations(config, input_data, input_size, 
                                      &processed_data, &processed_size) != 0) {
        free(input_data);
        return -1;
    }
    
    // Liberar datos de entrada (ya procesados)
    free(input_data);
        
    // Escribir archivo de salida
    printf("Escribiendo archivo de salida: %s\n", config->output_path);
    if (write_file(config->output_path, processed_data, processed_size) != 0) {
        fprintf(stderr, "Error: No se pudo escribir el archivo de salida\n");
        free(processed_data);
        return -1;
    }
    
    printf("Archivo escrito correctamente (%zu bytes)\n", processed_size);
    
    // Liberar memoria
    free(processed_data);
    
    printf("Operaciones completadas exitosamente\n");
    return 0;
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
    if (execute_operations(&config) != 0) {
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}