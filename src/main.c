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
    struct stat path_stat;
    
    if (stat(config->input_path, &path_stat) != 0) {
        fprintf(stderr, "Error: No se puede acceder a la ruta '%s'\n", config->input_path);
        return MODE_SINGLE_FILE; // Fallback
    }
    
    // Si es un directorio, siempre es modo directory para compresión
    if (S_ISDIR(path_stat.st_mode)) {
        return MODE_DIRECTORY;
    }
    
    // Si es un archivo, verificar si es un archive GSEA
    if (S_ISREG(path_stat.st_mode)) {
        // Para operaciones de DESCOMPRESIÓN/DESENCRIPTACIÓN, verificar si es archive
        if ((config->operations & OP_DECOMPRESS) || (config->operations & OP_DECRYPT)) {
            // Verificar extensiones comunes de archives
            const char *ext = strrchr(config->input_path, '.');
            if (ext != NULL) {
                if (strcmp(ext, ".gsea") == 0 || strcmp(ext, ".rle") == 0 || 
                    strcmp(ext, ".enc") == 0 || strcmp(ext, ".huff") == 0) {
                    // Podría ser un archive, verificar más a fondo
                    if (is_gsea_archive_file(config->input_path)) {
                        return MODE_ARCHIVE_EXTRACT;
                    }
                }
            }
        }
        
        return MODE_SINGLE_FILE;
    }
    return MODE_SINGLE_FILE;
}

int execute_single_file_operations(const program_config_t *config) {
    unsigned char *input_data = NULL;
    unsigned char *output_data = NULL;
    size_t input_size = 0;
    size_t output_size = 0;
    int result = 0;
    
    // Procesar path de salida automáticamente
    char *output_path = process_output_path(config);
    
    // Paso 1: Leer archivo de entrada
    printf("Paso 1: Leyendo archivo de entrada: %s\n", config->input_path);
    if (read_file(config->input_path, &input_data, &input_size) != 0) {
        fprintf(stderr, "Error: No se pudo leer el archivo de entrada '%s'\n", config->input_path);
        free(output_path);
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
        free(output_path);
        return -1;
    }
    
    // Paso 3: Escribir archivo de salida
    printf("Paso 3: Escribiendo archivo de salida: %s\n", output_path);
    if (write_file(output_path, output_data, output_size) != 0) {
        fprintf(stderr, "Error: No se pudo escribir el archivo de salida '%s'\n", output_path);
        free(output_data);
        free(output_path);
        return -1;
    }
    printf("  ✓ Archivo escrito correctamente (%zu bytes)\n", output_size);
    
    // Liberar memoria final
    free(output_data);
    free(output_path);
    
    printf("Procesamiento completado exitosamente\n");
    return 0;
}

int execute_directory_operations(const program_config_t *config) {
    // Procesar path de salida automáticamente
    char *output_path = process_output_path(config);
    int result = 0;
    
    // Comprobar todas las combinaciones posibles para directorios
    if ((config->operations & OP_COMPRESS) && (config->operations & OP_ENCRYPT)) {
        result = compress_and_encrypt_directory(config, output_path);
    } 
    else if ((config->operations & OP_DECRYPT) && (config->operations & OP_DECOMPRESS)) {
        result = decrypt_and_decompress_directory(config, output_path);
    }
    else if (config->operations & OP_COMPRESS) {
        // Solo comprimir
        if (strlen(config->key) > 0) {
            printf("Advertencia: Clave proporcionada pero no se usará (solo compresión)\n");
        }
        result = compress_directory_only(config, output_path);
    }
    else if (config->operations & OP_DECOMPRESS) {
        // Solo descomprimir
        if (strlen(config->key) > 0) {
            printf("Advertencia: Clave proporcionada pero no se usará (solo descompresión)\n");
        }
        result = decompress_directory_only(config, output_path);
    }
    else if (config->operations & OP_ENCRYPT) {
        // Solo encriptar
        if (strlen(config->key) == 0) {
            fprintf(stderr, "Error: Se requiere clave (-k) para encriptación\n");
            free(output_path);
            return -1;
        }
        result = encrypt_directory_only(config, output_path);
    }
    else if (config->operations & OP_DECRYPT) {
        // Solo desencriptar
        if (strlen(config->key) == 0) {
            fprintf(stderr, "Error: Se requiere clave (-k) para desencriptación\n");
            free(output_path);
            return -1;
        }
        result = decrypt_directory_only(config, output_path);
    }
    else {
        fprintf(stderr, "Error: No se especificaron operaciones válidas para directorio\n");
        fprintf(stderr, "Operaciones disponibles: -c, -d, -e, -u, -ce, -du\n");
        free(output_path);
        return -1;
    }
    
    free(output_path);
    return result;
}

int execute_operations(const program_config_t *config) {
    printf("Iniciando procesamiento...\n");
    
    if (!config->valid) {
        fprintf(stderr, "Error: Configuración inválida\n");
        return -1;
    }
    
    // Para archivos con extensión .huff, forzar modo archivo único
    const char *ext = strrchr(config->input_path, '.');
    if (ext != NULL && strcmp(ext, ".huff") == 0) {
        printf("Archivo Huffman detectado - Modo archivo único\n");
        return execute_single_file_operations(config);
    }
    
    operation_mode_t mode = detect_operation_mode(config);
    
    switch (mode) {
        case MODE_DIRECTORY:
            printf("Entrada detectada como directorio - Modo archive\n");
            return execute_directory_operations(config);
            
        case MODE_ARCHIVE_EXTRACT:
            printf("Entrada detectada como archive - Extrayendo a directorio\n");
            return execute_directory_operations(config);
            
        case MODE_SINGLE_FILE:
        default:
            printf("Entrada detectada como archivo único - Modo secuencial\n");
            return execute_single_file_operations(config);
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