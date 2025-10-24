#include <stdio.h>
#include <stdlib.h>
#include "../include/args_parser.h"

int main(int argc, char *argv[]) {
    program_config_t config;

    printf("=== GSEA - Parser de Argumentos - Pruebas ===\n\n");

    if (parse_arguments(argc, argv, &config) != 0) {
        return EXIT_FAILURE;
    }

    printf("Configuración parseada correctamente:\n");
    printf("    Operaciones: ");
    if (config.operations & OP_COMPRESS) printf("COMPRESS ");
    if (config.operations & OP_DECOMPRESS) printf("DECOMPRESS ");
    if (config.operations & OP_ENCRYPT) printf("ENCRYPT ");
    if (config.operations & OP_DECRYPT) printf("DECRYPT ");
    printf("\n");

    printf("  Algoritmo compresión: ");
    switch (config.comp_alg) {
        case COMP_ALG_RLE: printf("RLE\n"); break;
        case COMP_ALG_HUFFMAN: printf("HUFFMAN\n"); break;
        default: printf("NONE\n"); break;
    }
    
    printf("  Algoritmo encriptación: ");
    switch (config.enc_alg) {
        case ENC_ALG_VIGENERE: printf("VIGENERE\n"); break;
        default: printf("NONE\n"); break;
    }
    
    printf("  Entrada: %s\n", config.input_path);
    printf("  Salida: %s\n", config.output_path);
    if (config.key[0] != '\0') {
        printf("  Clave: %s\n", config.key);
    }
    
    return EXIT_SUCCESS;
}