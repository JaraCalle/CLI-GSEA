#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include "../include/args_parser.h"

int parse_arguments(int argc, char *argv[], program_config_t *config) {
    // Inicializar configuración con valores por defecto
    memset(config, 0, sizeof(program_config_t));
    config->comp_alg = COMP_ALG_RLE;
    config->enc_alg = ENC_ALG_VIGENERE;
    
    if (argc < 2) {
        fprintf(stderr, "Error: Se requieren argumentos.\n");
        print_usage(argv[0]);
        return -1;
    }

    int i = 1;
    while (i < argc) {
        // Verificar si es una opción (empieza con -)
        if (argv[i][0] == '-') {
            // Operaciones combinadas (-ce, -du, etc.) - solo si son letras c,d,e,u
            if (argv[i][1] != '-' && strlen(argv[i]) > 1) {
                // Verificar si son solo operaciones válidas
                int is_operation = 1;
                for (int j = 1; argv[i][j] != '\0'; j++) {
                    char c = argv[i][j];
                    if (c != 'c' && c != 'd' && c != 'e' && c != 'u') {
                        is_operation = 0;
                        break;
                    }
                }
                
                if (is_operation) {
                    if (parse_operations(argv[i] + 1, &config->operations) != 0) {
                        return -1;
                    }
                    i++;
                } else {
                    // No son operaciones, tratar como opción normal
                    goto process_option;
                }
            } else {
                process_option:
                // Opciones con -- (algoritmos)
                if (strcmp(argv[i], "--comp-alg") == 0) {
                    if (i + 1 >= argc) {
                        fprintf(stderr, "Error: --comp-alg requiere un argumento.\n");
                        return -1;
                    }
                    config->comp_alg = parse_compression_alg(argv[i + 1]);
                    if (config->comp_alg == COMP_ALG_NONE) {
                        fprintf(stderr, "Error: Algoritmo de compresión desconocido '%s'\n", argv[i + 1]);
                        fprintf(stderr, "Algoritmos disponibles: rle, huffman\n");
                        return -1;
                    }
                    i += 2;
                }
                else if (strcmp(argv[i], "--enc-alg") == 0) {
                    if (i + 1 >= argc) {
                        fprintf(stderr, "Error: --enc-alg requiere un argumento.\n");
                        return -1;
                    }
                    config->enc_alg = parse_encryption_alg(argv[i + 1]);
                    if (config->enc_alg == ENC_ALG_NONE) {
                        fprintf(stderr, "Error: Algoritmo de encriptación desconocido '%s'\n", argv[i + 1]);
                        fprintf(stderr, "Algoritmos disponibles: vigenere\n");
                        return -1;
                    }
                    i += 2;
                }
                // Ruta de entrada
                else if (strcmp(argv[i], "-i") == 0) {
                    if (i + 1 >= argc) {
                        fprintf(stderr, "Error: -i requiere un argumento.\n");
                        return -1;
                    }
                    strncpy(config->input_path, argv[i + 1], MAX_PATH_LENGTH - 1);
                    config->input_path[MAX_PATH_LENGTH - 1] = '\0';
                    i += 2;
                }
                // Ruta de salida
                else if (strcmp(argv[i], "-o") == 0) {
                    if (i + 1 >= argc) {
                        fprintf(stderr, "Error: -o requiere un argumento\n");
                        return -1;
                    }
                    strncpy(config->output_path, argv[i + 1], MAX_PATH_LENGTH - 1);
                    config->output_path[MAX_PATH_LENGTH - 1] = '\0';
                    i += 2;
                }
                // Clave
                else if (strcmp(argv[i], "-k") == 0) {
                    if (i + 1 >= argc) {
                        fprintf(stderr, "Error: -k requiere un argumento.\n");
                        return -1;
                    }
                    strncpy(config->key, argv[i + 1], MAX_KEY_LENGTH - 1);
                    config->key[MAX_KEY_LENGTH - 1] = '\0';
                    i += 2;
                }
                // Ayuda
                else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                    print_usage(argv[0]);
                    exit(0);
                }
                // Opción desconocida
                else {
                    fprintf(stderr, "Error: Opción desconocida '%s'\n", argv[i]);
                    print_usage(argv[0]);
                    return -1;
                }
            }
        } else {
            fprintf(stderr, "Error: Argumento inesperado '%s'\n", argv[i]);
            print_usage(argv[0]);
            return -1;
        }
    }

    // Validar la configuración final
    if (validate_config(config) != 0) {
        return -1;
    }

    config->valid = 1;
    return 0;
}

int parse_operations(const char *op_str, operation_t *operations) {
    *operations = OP_NONE;
    
    for (int i = 0; op_str[i] != '\0'; i++) {
        switch (op_str[i]) {
            case 'c':
                if (*operations & OP_COMPRESS) {
                    fprintf(stderr, "Error: Operación 'c' (comprimir) repetida\n");
                    return -1;
                }
                *operations |= OP_COMPRESS;
                break;
            case 'd':
                if (*operations & OP_DECOMPRESS) {
                    fprintf(stderr, "Error: Operación 'd' (descomprimir) repetida\n");
                    return -1;
                }
                *operations |= OP_DECOMPRESS;
                break;
            case 'e':
                if (*operations & OP_ENCRYPT) {
                    fprintf(stderr, "Error: Operación 'e' (encriptar) repetida\n");
                    return -1;
                }
                *operations |= OP_ENCRYPT;
                break;
            case 'u':
                if (*operations & OP_DECRYPT) {
                    fprintf(stderr, "Error: Operación 'u' (desencriptar) repetida\n");
                    return -1;
                }
                *operations |= OP_DECRYPT;
                break;
            default:
                // Esto no debería pasar debido a la validación anterior
                fprintf(stderr, "Error: Operación desconocida '%c'\n", op_str[i]);
                fprintf(stderr, "Operaciones válidas: c, d, e, u\n");
                return -1;
        }
    }
    return 0;
}

compression_alg_t parse_compression_alg(const char *alg_str) {
    if (strcmp(alg_str, "rle") == 0) {
        return COMP_ALG_RLE;
    } else if (strcmp(alg_str, "huffman") == 0) {
        return COMP_ALG_HUFFMAN;
    }
    return COMP_ALG_NONE;
}

encryption_alg_t parse_encryption_alg(const char *alg_str) {
    if (strcmp(alg_str, "vigenere") == 0) {
        return ENC_ALG_VIGENERE;
    }
    return ENC_ALG_NONE;
}

int validate_config(const program_config_t *config) {
    // Verificar que se especificó al menos una operación
    if (config->operations == OP_NONE) {
        fprintf(stderr, "Error: Debe especificar al menos una operación (-c, -d, -e, -u)\n");
        return -1;
    }

    // Verificar que no se combinen operaciones contradictorias
    if ((config->operations & OP_COMPRESS) && (config->operations & OP_DECOMPRESS)) {
        fprintf(stderr, "Error: No se puede comprimir y descomprimir al mismo tiempo\n");
        return -1;
    }
    
    if ((config->operations & OP_ENCRYPT) && (config->operations & OP_DECRYPT)) {
        fprintf(stderr, "Error: No se puede encriptar y desencriptar al mismo tiempo\n");
        return -1;
    }

    // Verificar ruta de entrada
    if (strlen(config->input_path) == 0) {
        fprintf(stderr, "Error: Debe especificar una ruta de entrada (-i)\n");
        return -1;
    }

    // Verificar clave para operaciones de encriptación/desencriptación
    if ((config->operations & OP_ENCRYPT) || (config->operations & OP_DECRYPT)) {
        if (strlen(config->key) == 0) {
            fprintf(stderr, "Error: Se requiere una clave (-k) para operaciones de encriptación/desencriptación\n");
            return -1;
        }
    }

    return 0;
}

void print_usage(const char *program_name) {
    printf("Uso: %s [OPERACIONES] [OPCIONES]\n\n", program_name);
    printf("OPERACIONES (pueden combinarse):\n");
    printf("  -c          Comprimir\n");
    printf("  -d          Descomprimir\n");
    printf("  -e          Encriptar\n");
    printf("  -u          Desencriptar\n");
    printf("  Ejemplo: -ce para comprimir y luego encriptar, -du para descomprimir y desencriptar\n\n");
    
    printf("OPCIONES:\n");
    printf("  --comp-alg ALGORITMO  Algoritmo de compresión (rle, huffman)\n");
    printf("  --enc-alg ALGORITMO   Algoritmo de encriptación (vigenere)\n");
    printf("  -i RUTA               Ruta de entrada (archivo o directorio)\n");
    printf("  -o RUTA               Ruta de salida (archivo o directorio)\n");
    printf("  -k CLAVE              Clave para encriptación/desencriptación\n");
    printf("  -h, --help            Mostrar esta ayuda\n\n");
    
    printf("EJEMPLOS:\n");
    printf("  %s -ce --comp-alg rle --enc-alg vigenere -i entrada.txt -o salida.dat -k mi_clave\n", program_name);
    printf("  %s -du --comp-alg huffman --enc-alg vigenere -i archivo.dat -o resultado.txt -k mi_clave\n", program_name);
    printf("  %s -c --comp-alg rle -i archivo.txt -o archivo.rle\n", program_name);
    printf("  %s -e --enc-alg vigenere -i datos.txt -o datos.enc -k clave123\n", program_name);
}