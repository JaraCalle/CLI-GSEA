#ifndef ENCRYPTION_H
#define ENCRYPTION_H

#include <stddef.h>

typedef struct
{
    unsigned char *data;
    size_t size;
    int error;
} encryption_result_t;

encryption_result_t encrypt_vigenere(const unsigned char *input, size_t input_size, const unsigned char *key, size_t key_size);
encryption_result_t decrypt_vigenere(const unsigned char *input, size_t input_size, const unsigned char *key, size_t key_size);
void free_encryption_result(encryption_result_t *result);
int validate_key(const unsigned char *key, size_t key_size);

#endif