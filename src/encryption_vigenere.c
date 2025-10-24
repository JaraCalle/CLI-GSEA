#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/encryption.h"

static unsigned char vigenere_transform(unsigned char byte, unsigned char key_byte, int encrypt) {
    // Para TODOS los bytes (0-255), usar XOR que es reversible y preserva todos los valores
    if (encrypt) {
        // Encriptar: C = P XOR K
        return byte ^ key_byte;
    } else {
        // Desencriptar: P = C XOR K (misma operación)
        return byte ^ key_byte;
    }
}


static unsigned char *expand_key(const unsigned char *key, size_t key_size, size_t output_size) {
    if (key_size == 0 || output_size == 0) {
        return NULL;
    }
    
    unsigned char *expanded_key = (unsigned char *)malloc(output_size);
    if (expanded_key == NULL) {
        return NULL;
    }
    
    for (size_t i = 0; i < output_size; i++) {
        expanded_key[i] = key[i % key_size];
    }
    
    return expanded_key;
}

int validate_key(const unsigned char *key, size_t key_size) {
    if (key == NULL || key_size == 0) {
        fprintf(stderr, "Error: Clave nula o vacía\n");
        return -1;
    }
    
    // Verificar que la clave tenga al menos un byte no nulo
    int has_non_zero = 0;
    for (size_t i = 0; i < key_size; i++) {
        if (key[i] != 0) {
            has_non_zero = 1;
            break;
        }
    }
    
    if (!has_non_zero) {
        fprintf(stderr, "Error: Clave contiene solo bytes nulos\n");
        return -1;
    }
    
    return 0;
}

encryption_result_t encrypt_vigenere(const unsigned char *input, size_t input_size, 
                                   const unsigned char *key, size_t key_size) {
    encryption_result_t result = {NULL, 0, 0};
    
    // Validaciones
    if (input == NULL || input_size == 0) {
        result.error = -1;
        fprintf(stderr, "Error: Datos de entrada inválidos\n");
        return result;
    }
    
    if (validate_key(key, key_size) != 0) {
        result.error = -2;
        return result;
    }
    
    // Preparar buffer de salida
    unsigned char *encrypted = (unsigned char *)malloc(input_size);
    if (encrypted == NULL) {
        result.error = -3;
        fprintf(stderr, "Error: No se pudo asignar memoria para encriptación\n");
        return result;
    }
    
    // Expandir clave al tamaño de los datos
    unsigned char *expanded_key = expand_key(key, key_size, input_size);
    if (expanded_key == NULL) {
        free(encrypted);
        result.error = -4;
        fprintf(stderr, "Error: No se pudo expandir la clave\n");
        return result;
    }
    
    // Aplicar cifrado Vigenère con XOR
    for (size_t i = 0; i < input_size; i++) {
        encrypted[i] = vigenere_transform(input[i], expanded_key[i], 1);
    }
    
    result.data = encrypted;
    result.size = input_size;
    result.error = 0;
    
    free(expanded_key);
    return result;
}


encryption_result_t decrypt_vigenere(const unsigned char *input, size_t input_size, 
                                   const unsigned char *key, size_t key_size) {
    encryption_result_t result = {NULL, 0, 0};
    
    // Validaciones
    if (input == NULL || input_size == 0) {
        result.error = -1;
        fprintf(stderr, "Error: Datos de entrada inválidos\n");
        return result;
    }
    
    if (validate_key(key, key_size) != 0) {
        result.error = -2;
        return result;
    }
    
    // Preparar buffer de salida
    unsigned char *decrypted = (unsigned char *)malloc(input_size);
    if (decrypted == NULL) {
        result.error = -3;
        fprintf(stderr, "Error: No se pudo asignar memoria para desencriptación\n");
        return result;
    }
    
    // Expandir clave al tamaño de los datos
    unsigned char *expanded_key = expand_key(key, key_size, input_size);
    if (expanded_key == NULL) {
        free(decrypted);
        result.error = -4;
        fprintf(stderr, "Error: No se pudo expandir la clave\n");
        return result;
    }
    
    // Aplicar descifrado Vigenère con XOR (misma operación que encriptación)
    for (size_t i = 0; i < input_size; i++) {
        decrypted[i] = vigenere_transform(input[i], expanded_key[i], 0);
    }
    
    result.data = decrypted;
    result.size = input_size;
    result.error = 0;
    
    free(expanded_key);
    return result;
}

void free_encryption_result(encryption_result_t *result) {
    if (result != NULL && result->data != NULL) {
        free(result->data);
        result->data = NULL;
        result->size = 0;
        result->error = 0;
    }
}