#ifndef COMPRESSION_HUFFMAN_H
#define COMPRESSION_HUFFMAN_H

#include "compression.h"

compression_result_t compress_huffman_wrapper(const unsigned char *input, size_t input_size);
compression_result_t decompress_huffman_wrapper(const unsigned char *input, size_t input_size);

#endif