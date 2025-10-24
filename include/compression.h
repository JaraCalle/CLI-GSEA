#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <stddef.h>

// Estructura para resultados de compresión
typedef struct {
    unsigned char *data;
    size_t size;
    int error;
} compression_result_t;

compression_result_t compress_rle(const unsigned char *input, size_t input_size);
compression_result_t decompress_rle(const unsigned char *input, size_t input_size);
void free_compression_result(compression_result_t *result);
double compression_ratio(size_t original_size, size_t compressed_size);

#endif