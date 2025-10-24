#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/compression.h"

#define RLE_MAX_RUN 255  // Máxima longitud de secuencia (1 byte)

compression_result_t compress_rle(const unsigned char *input, size_t input_size) {
    compression_result_t result = {NULL, 0, 0};
    
    if (input == NULL || input_size == 0) {
        result.error = -1;
        return result;
    }
    
    // Buffer temporal para la compresión (peor caso: 2x el tamaño original)
    unsigned char *compressed = (unsigned char *)malloc(input_size * 2);
    if (compressed == NULL) {
        result.error = -2;
        return result;
    }
    
    size_t compressed_index = 0;
    size_t i = 0;
    
    while (i < input_size) {
        unsigned char current_byte = input[i];
        size_t run_length = 1;
        
        // Contar secuencias repetidas (máximo RLE_MAX_RUN)
        while (i + run_length < input_size && 
               input[i + run_length] == current_byte && 
               run_length < RLE_MAX_RUN) {
            run_length++;
        }
        
        // Escribir el par <count><byte>
        compressed[compressed_index++] = (unsigned char)run_length;
        compressed[compressed_index++] = current_byte;
        
        i += run_length;
    }
    
    // Reducir el buffer al tamaño exacto
    unsigned char *final_compressed = (unsigned char *)realloc(compressed, compressed_index);
    if (final_compressed == NULL) {
        // Si falla el realloc, usar el buffer original
        final_compressed = compressed;
    }
    
    result.data = final_compressed;
    result.size = compressed_index;
    result.error = 0;
    
    return result;
}

compression_result_t decompress_rle(const unsigned char *input, size_t input_size) {
    compression_result_t result = {NULL, 0, 0};
    
    if (input == NULL || input_size == 0 || input_size % 2 != 0) {
        // El tamaño debe ser par (pares de <count><byte>)
        result.error = -1;
        return result;
    }
    
    // Primera pasada: calcular el tamaño descomprimido
    size_t decompressed_size = 0;
    for (size_t i = 0; i < input_size; i += 2) {
        if (i + 1 >= input_size) {
            result.error = -3; // Formato inválido
            return result;
        }
        
        unsigned char count = input[i];
        decompressed_size += count;
    }
    
    // Segunda pasada: descomprimir
    unsigned char *decompressed = (unsigned char *)malloc(decompressed_size);
    if (decompressed == NULL) {
        result.error = -2;
        return result;
    }
    
    size_t decompressed_index = 0;
    
    for (size_t i = 0; i < input_size; i += 2) {
        unsigned char count = input[i];
        unsigned char byte = input[i + 1];
        
        // Verificar que no excedamos el buffer
        if (decompressed_index + count > decompressed_size) {
            free(decompressed);
            result.error = -4; // Corrupción de datos
            return result;
        }
        
        // Repetir el byte 'count' veces
        for (unsigned char j = 0; j < count; j++) {
            decompressed[decompressed_index++] = byte;
        }
    }
    
    result.data = decompressed;
    result.size = decompressed_size;
    result.error = 0;
    
    return result;
}

void free_compression_result(compression_result_t *result) {
    if (result != NULL && result->data != NULL) {
        free(result->data);
        result->data = NULL;
        result->size = 0;
        result->error = 0;
    }
}

double compression_ratio(size_t original_size, size_t compressed_size) {
    if (original_size == 0) return 0.0;
    return (double)compressed_size / (double)original_size;
}