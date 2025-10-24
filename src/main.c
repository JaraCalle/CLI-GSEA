#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/args_parser.h"
#include "../include/file_manager.h"

int execute_operations(const program_config_t *config) {
    printf("Ejecutando operaciones...\n");
    
    // Verificar que la configuración es válida
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
    
    unsigned char *output_data = input_data;
    size_t output_size = input_size;
    
    // Escribir archivo de salida
    printf("Escribiendo archivo de salida: %s\n", config->output_path);
    if (write_file(config->output_path, output_data, output_size) != 0) {
        fprintf(stderr, "Error: No se pudo escribir el archivo de salida\n");
        free(input_data);
        return -1;
    }
    
    printf("Archivo escrito correctamente (%zu bytes)\n", output_size);
    
    // Liberar memoria
    free(input_data);
    
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
    
    // Mostrar configuración (debug)
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