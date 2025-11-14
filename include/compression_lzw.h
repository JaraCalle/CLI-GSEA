#ifndef COMPRESSION_LZW_H
#define COMPRESSION_LZW_H

#include <stddef.h>
#include "compression.h"

compression_result_t compress_lzw(const unsigned char *input, size_t input_size);
compression_result_t decompress_lzw(const unsigned char *input, size_t input_size);

#endif
