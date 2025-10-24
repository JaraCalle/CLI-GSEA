#ifndef ARGS_PARSER_H
#define ARGS_PARSER_H

#define MAX_PATH_LENGTH 1024
#define MAX_KEY_LENGTH 256
#define MAX_ALG_NAME_LENGTH 50

// Operaciones disponibles
typedef enum {
    OP_NONE = 0,
    OP_COMPRESS = 1 << 0,
    OP_DECOMPRESS = 1 << 1,
    OP_ENCRYPT = 1 << 2,
    OP_DECRYPT = 1 << 3
} operation_t;

// Algoritmos de compresión disponibles
typedef enum {
    COMP_ALG_NONE,
    COMP_ALG_RLE,
    COMP_ALG_HUFFMAN
} compression_alg_t;

// Algoritmos de encriptación disponibles
typedef enum {
    ENC_ALG_NONE,
    ENC_ALG_VIGENERE
} encryption_alg_t;

// Estructura para almacenar la configuración del programa
typedef struct {
    operation_t operations;
    compression_alg_t comp_alg;
    encryption_alg_t enc_alg;
    char input_path[MAX_PATH_LENGTH];
    char output_path[MAX_PATH_LENGTH];
    char key[MAX_KEY_LENGTH];
    int valid;
} program_config_t;

int parse_arguments(int argc, char *argv[], program_config_t *config);
int validate_config(const program_config_t *config);
void print_usage(const char *program_name);
int parse_operations(const char *op_str, operation_t *operations);
compression_alg_t parse_compression_alg(const char *alg_str);
encryption_alg_t parse_encryption_alg(const char *alg_str);

#endif