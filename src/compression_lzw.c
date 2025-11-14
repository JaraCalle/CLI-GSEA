#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../include/compression_lzw.h"

#define LZW_MAX_DICT_SIZE 4096
#define LZW_INVALID_PREFIX -1

typedef struct {
    int prefix;
    unsigned char value;
} lzw_entry_t;

typedef struct {
    unsigned char magic[4];
    uint32_t original_size;
    uint32_t code_count;
} lzw_header_t;

static void init_dictionary(lzw_entry_t *dict, int *size) {
    for (int i = 0; i < 256; ++i) {
        dict[i].prefix = LZW_INVALID_PREFIX;
        dict[i].value = (unsigned char)i;
    }
    for (int i = 256; i < LZW_MAX_DICT_SIZE; ++i) {
        dict[i].prefix = LZW_INVALID_PREFIX;
        dict[i].value = 0;
    }
    *size = 256;
}

static int find_in_dict(const lzw_entry_t *dict, int dict_size, int prefix, unsigned char value) {
    for (int i = 256; i < dict_size; ++i) {
        if (dict[i].prefix == prefix && dict[i].value == value) {
            return i;
        }
    }
    return -1;
}

static void write_u32(unsigned char *dst, uint32_t value) {
    dst[0] = (unsigned char)(value & 0xFFu);
    dst[1] = (unsigned char)((value >> 8) & 0xFFu);
    dst[2] = (unsigned char)((value >> 16) & 0xFFu);
    dst[3] = (unsigned char)((value >> 24) & 0xFFu);
}

static uint32_t read_u32(const unsigned char *src) {
    return (uint32_t)src[0] |
           ((uint32_t)src[1] << 8) |
           ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

static void write_u16(unsigned char *dst, uint16_t value) {
    dst[0] = (unsigned char)(value & 0xFFu);
    dst[1] = (unsigned char)((value >> 8) & 0xFFu);
}

static uint16_t read_u16(const unsigned char *src) {
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

compression_result_t compress_lzw(const unsigned char *input, size_t input_size) {
    compression_result_t result = {NULL, 0, 0};

    if (input == NULL || input_size == 0) {
        result.error = -1;
        return result;
    }

    lzw_entry_t dictionary[LZW_MAX_DICT_SIZE];
    int dict_size = 0;
    init_dictionary(dictionary, &dict_size);

    size_t codes_capacity = input_size ? input_size : 1;
    uint16_t *codes = (uint16_t *)malloc(codes_capacity * sizeof(uint16_t));
    if (!codes) {
        result.error = -2;
        return result;
    }

    int current_code = input[0];
    size_t code_count = 0;

    for (size_t i = 1; i < input_size; ++i) {
        unsigned char c = input[i];
        int next_code = find_in_dict(dictionary, dict_size, current_code, c);

        if (next_code != -1) {
            current_code = next_code;
        } else {
            if (code_count >= codes_capacity) {
                size_t new_capacity = codes_capacity * 2;
                uint16_t *new_codes = (uint16_t *)realloc(codes, new_capacity * sizeof(uint16_t));
                if (!new_codes) {
                    free(codes);
                    result.error = -3;
                    return result;
                }
                codes = new_codes;
                codes_capacity = new_capacity;
            }

            codes[code_count++] = (uint16_t)current_code;

            if (dict_size < LZW_MAX_DICT_SIZE) {
                dictionary[dict_size].prefix = current_code;
                dictionary[dict_size].value = c;
                dict_size++;
            }

            current_code = c;
        }
    }

    if (code_count >= codes_capacity) {
        size_t new_capacity = codes_capacity + 1;
        uint16_t *new_codes = (uint16_t *)realloc(codes, new_capacity * sizeof(uint16_t));
        if (!new_codes) {
            free(codes);
            result.error = -3;
            return result;
        }
        codes = new_codes;
        codes_capacity = new_capacity;
    }
    codes[code_count++] = (uint16_t)current_code;

    size_t header_size = sizeof(lzw_header_t);
    size_t payload_size = code_count * sizeof(uint16_t);
    size_t total_size = header_size + payload_size;

    unsigned char *output = (unsigned char *)malloc(total_size);
    if (!output) {
        free(codes);
        result.error = -4;
        return result;
    }

    lzw_header_t header;
    header.magic[0] = 'L';
    header.magic[1] = 'Z';
    header.magic[2] = 'W';
    header.magic[3] = '1';
    header.original_size = (uint32_t)input_size;
    header.code_count = (uint32_t)code_count;

    memcpy(output, header.magic, 4);
    write_u32(output + 4, header.original_size);
    write_u32(output + 8, header.code_count);

    unsigned char *payload = output + header_size;
    for (size_t i = 0; i < code_count; ++i) {
        write_u16(payload + i * 2, codes[i]);
    }

    free(codes);

    result.data = output;
    result.size = total_size;
    result.error = 0;
    return result;
}

static size_t build_sequence(const lzw_entry_t *dict, int code, unsigned char *buffer) {
    unsigned char temp[LZW_MAX_DICT_SIZE];
    size_t length = 0;

    while (code != LZW_INVALID_PREFIX && length < LZW_MAX_DICT_SIZE) {
        temp[length++] = dict[code].value;
        code = dict[code].prefix;
    }

    for (size_t i = 0; i < length; ++i) {
        buffer[i] = temp[length - 1 - i];
    }

    return length;
}

compression_result_t decompress_lzw(const unsigned char *input, size_t input_size) {
    compression_result_t result = {NULL, 0, 0};

    if (!input || input_size < sizeof(lzw_header_t)) {
        result.error = -1;
        return result;
    }

    if (input[0] != 'L' || input[1] != 'Z' || input[2] != 'W' || input[3] != '1') {
        result.error = -2;
        return result;
    }

    uint32_t original_size = read_u32(input + 4);
    uint32_t code_count = read_u32(input + 8);
    size_t header_size = sizeof(lzw_header_t);

    if (header_size + (size_t)code_count * sizeof(uint16_t) > input_size) {
        result.error = -3;
        return result;
    }

    if (original_size == 0 || code_count == 0) {
        result.error = -4;
        return result;
    }

    unsigned char *output = (unsigned char *)malloc(original_size);
    if (!output) {
        result.error = -5;
        return result;
    }

    lzw_entry_t dictionary[LZW_MAX_DICT_SIZE];
    int dict_size = 0;
    init_dictionary(dictionary, &dict_size);

    const unsigned char *payload = input + header_size;
    uint16_t old_code = read_u16(payload);
    if (old_code >= (uint16_t)dict_size) {
        free(output);
        result.error = -6;
        return result;
    }

    unsigned char sequence[LZW_MAX_DICT_SIZE + 1];
    size_t seq_len = build_sequence(dictionary, old_code, sequence);
    if (seq_len == 0 || seq_len > original_size) {
        free(output);
        result.error = -7;
        return result;
    }

    memcpy(output, sequence, seq_len);
    size_t out_index = seq_len;
    unsigned char first_char = sequence[0];

    for (uint32_t i = 1; i < code_count; ++i) {
        uint16_t new_code = read_u16(payload + i * 2);
        size_t entry_len;

        if (new_code < (uint16_t)dict_size) {
            entry_len = build_sequence(dictionary, new_code, sequence);
            if (entry_len == 0) {
                free(output);
                result.error = -8;
                return result;
            }
        } else if (new_code == (uint16_t)dict_size && dict_size < LZW_MAX_DICT_SIZE) {
            entry_len = build_sequence(dictionary, old_code, sequence);
            if (entry_len == 0 || entry_len + 1 > LZW_MAX_DICT_SIZE) {
                free(output);
                result.error = -9;
                return result;
            }
            sequence[entry_len] = first_char;
            entry_len += 1;
        } else {
            free(output);
            result.error = -10;
            return result;
        }

        if (out_index + entry_len > original_size) {
            free(output);
            result.error = -11;
            return result;
        }

        memcpy(output + out_index, sequence, entry_len);
        out_index += entry_len;

        if (dict_size < LZW_MAX_DICT_SIZE) {
            dictionary[dict_size].prefix = old_code;
            dictionary[dict_size].value = sequence[0];
            dict_size++;
        }

        old_code = new_code;
        first_char = sequence[0];
    }

    if (out_index != original_size) {
        free(output);
        result.error = -12;
        return result;
    }

    result.data = output;
    result.size = original_size;
    result.error = 0;
    return result;
}
