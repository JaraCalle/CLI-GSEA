#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/compression.h"

#define RLE_MIN_RUN 3
#define RLE_LITERAL_MAX 128
#define RLE_RUN_FLAG 0x80
#define RLE_LITERAL_FLAG 0x00
#define RLE_RUN_EXTRA_MASK 0x7F
#define RLE_MAX_RUN_CHUNK (RLE_MIN_RUN + RLE_RUN_EXTRA_MASK)

static void flush_literal_buffer(unsigned char **compressed,
                                 size_t *compressed_index,
                                 size_t *buffer_capacity,
                                 unsigned char *literal_buffer,
                                 size_t *literal_len) {
    if (*literal_len == 0) {
        return;
    }

    if (*compressed_index + 1 + *literal_len > *buffer_capacity) {
        size_t new_capacity = (*buffer_capacity + (*buffer_capacity / 2) + *literal_len + 1);
        unsigned char *new_data = realloc(*compressed, new_capacity);
        if (!new_data) {
            return; // manejo en caller mediante chequear literal_len luego
        }
        *compressed = new_data;
        *buffer_capacity = new_capacity;
    }

    (*compressed)[(*compressed_index)++] = (unsigned char)(RLE_LITERAL_FLAG | ((unsigned char)(*literal_len - 1) & RLE_RUN_EXTRA_MASK));
    memcpy(&(*compressed)[*compressed_index], literal_buffer, *literal_len);
    *compressed_index += *literal_len;
    *literal_len = 0;
}

compression_result_t compress_rle(const unsigned char *input, size_t input_size) {
    compression_result_t result = {NULL, 0, 0};
    
    if (input == NULL || input_size == 0) {
        result.error = -1;
        return result;
    }
    
    // Buffer temporal para la compresión
    size_t buffer_capacity = input_size * 2;
    if (buffer_capacity == 0) {
        buffer_capacity = 2;
    }
    unsigned char *compressed = (unsigned char *)malloc(buffer_capacity);
    if (compressed == NULL) {
        result.error = -2;
        return result;
    }
    
    size_t compressed_index = 0;
    size_t i = 0;
    unsigned char literal_buffer[RLE_LITERAL_MAX];
    size_t literal_len = 0;
    
    while (i < input_size) {
        unsigned char current_byte = input[i];
        size_t run_length = 1;
        
        while (i + run_length < input_size && 
               input[i + run_length] == current_byte) {
            run_length++;
        }

        if (run_length >= RLE_MIN_RUN) {
            flush_literal_buffer(&compressed, &compressed_index, &buffer_capacity, literal_buffer, &literal_len);
            if (literal_len != 0) {
                free(compressed);
                result.error = -3;
                return result;
            }

            size_t remaining = run_length;
            while (remaining > 0) {
                size_t chunk = remaining;
                if (chunk > RLE_MAX_RUN_CHUNK) {
                    chunk = RLE_MAX_RUN_CHUNK;
                }

                if (compressed_index + 2 > buffer_capacity) {
                    size_t new_capacity = buffer_capacity + (buffer_capacity / 2) + 2;
                    unsigned char *new_data = realloc(compressed, new_capacity);
                    if (!new_data) {
                        free(compressed);
                        result.error = -3;
                        return result;
                    }
                    compressed = new_data;
                    buffer_capacity = new_capacity;
                }

                compressed[compressed_index++] = (unsigned char)(RLE_RUN_FLAG | ((chunk - RLE_MIN_RUN) & RLE_RUN_EXTRA_MASK));
                compressed[compressed_index++] = current_byte;
                remaining -= chunk;
            }
            i += run_length;
        } else {
            literal_buffer[literal_len++] = current_byte;
            if (literal_len == RLE_LITERAL_MAX) {
                flush_literal_buffer(&compressed, &compressed_index, &buffer_capacity, literal_buffer, &literal_len);
                if (literal_len != 0) {
                    free(compressed);
                    result.error = -3;
                    return result;
                }
            }
            i++;
        }
    }

    flush_literal_buffer(&compressed, &compressed_index, &buffer_capacity, literal_buffer, &literal_len);
    if (literal_len != 0) {
        free(compressed);
        result.error = -3;
        return result;
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
    
    if (input == NULL || input_size == 0) {
        result.error = -1;
        return result;
    }
    
    // Primera pasada: calcular el tamaño descomprimido
    size_t decompressed_size = 0;
    size_t i = 0;
    while (i < input_size) {
        unsigned char tag = input[i++];
        if (tag & RLE_RUN_FLAG) {
            if (i >= input_size) {
                result.error = -3;
                return result;
            }
            size_t run_len = (tag & RLE_RUN_EXTRA_MASK) + RLE_MIN_RUN;
            decompressed_size += run_len;
            i += 1; // skip byte
        } else {
            size_t literal_len = (tag & RLE_RUN_EXTRA_MASK) + 1;
            if (i + literal_len > input_size) {
                result.error = -3;
                return result;
            }
            decompressed_size += literal_len;
            i += literal_len;
        }
    }

    unsigned char *decompressed = (unsigned char *)malloc(decompressed_size);
    if (decompressed == NULL) {
        result.error = -2;
        return result;
    }

    size_t decompressed_index = 0;
    i = 0;
    while (i < input_size) {
        unsigned char tag = input[i++];
        if (tag & RLE_RUN_FLAG) {
            if (i >= input_size) {
                free(decompressed);
                result.error = -4;
                return result;
            }
            size_t run_len = (tag & RLE_RUN_EXTRA_MASK) + RLE_MIN_RUN;
            unsigned char value = input[i++];
            if (decompressed_index + run_len > decompressed_size) {
                free(decompressed);
                result.error = -4;
                return result;
            }
            memset(&decompressed[decompressed_index], value, run_len);
            decompressed_index += run_len;
        } else {
            size_t literal_len = (tag & RLE_RUN_EXTRA_MASK) + 1;
            if (i + literal_len > input_size || decompressed_index + literal_len > decompressed_size) {
                free(decompressed);
                result.error = -4;
                return result;
            }
            memcpy(&decompressed[decompressed_index], &input[i], literal_len);
            decompressed_index += literal_len;
            i += literal_len;
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