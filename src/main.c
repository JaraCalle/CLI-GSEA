#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/args_parser.h"
#include "../include/file_manager.h"
#include "../include/compression.h"
#include "../include/encryption.h"
#include "../include/concurrency.h"
#include "../include/archive.h"
#include "../include/operations.h"

typedef enum {
    MODE_SINGLE_FILE,
    MODE_DIRECTORY,
    MODE_ARCHIVE_EXTRACT
} operation_mode_t;

operation_mode_t detect_operation_mode(const program_config_t *config) {
    struct stat input_stat;
    if (stat(config->input_path, &input_stat) != 0) {
        return MODE_SINGLE_FILE; // Fallback por defecto
    }
    
    if (S_ISDIR(input_stat.st_mode)) {
        return MODE_DIRECTORY;
    }
    
    if (S_ISREG(input_stat.st_mode)) {
        // Si estamos descomprimiendo+desencriptando
        if ((config->operations & OP_DECRYPT) && (config->operations & OP_DECOMPRESS)) {
            // Verificar si la entrada es un archive GSEA
            if (is_gsea_archive_file(config->input_path)) {
                return MODE_ARCHIVE_EXTRACT;
            }
            
            // También verificar si la salida es un directorio existente
            struct stat output_stat;
            if (stat(config->output_path, &output_stat) == 0 && S_ISDIR(output_stat.st_mode)) {
                return MODE_ARCHIVE_EXTRACT;
            }
        }
    }
    
    return MODE_SINGLE_FILE;
}

int execute_single_file_operations(const program_config_t *config) {
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
}

int execute_operations(const program_config_t *config) {
    printf("Iniciando procesamiento...\n");
    
    if (!config->valid) {
        fprintf(stderr, "Error: Configuración inválida\n");
        return -1;
    }
    
    operation_mode_t mode = detect_operation_mode(config);
    
    switch (mode) {
        case MODE_DIRECTORY:
            printf("Entrada detectada como directorio - Modo archive único\n");
            if ((config->operations & OP_COMPRESS) && (config->operations & OP_ENCRYPT)) {
                return compress_and_encrypt_directory(config);
            } else if ((config->operations & OP_DECRYPT) && (config->operations & OP_DECOMPRESS)) {
                return decrypt_and_decompress_directory(config);
            } else {
                fprintf(stderr, "Error: Para directorios solo se permiten combinaciones -ce o -du\n");
                return -1;
            }
            break;
            
        case MODE_ARCHIVE_EXTRACT:
            printf("Entrada detectada como archive GSEA - Extrayendo a directorio\n");
            return decrypt_and_decompress_directory(config);
            break;
            
        case MODE_SINGLE_FILE:
        default:
            printf("Entrada detectada como archivo único - Modo secuencial\n");
            return execute_single_file_operations(config);
            break;
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