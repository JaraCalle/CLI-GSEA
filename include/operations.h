#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <stddef.h>
#include "args_parser.h"

int execute_operations_sequential(const program_config_t *config, 
                                 const unsigned char *input_data, 
                                 size_t input_size,
                                 unsigned char **output_data, 
                                 size_t *output_size);

int execute_compression_operations(const program_config_t *config, 
                                  const unsigned char *input_data, 
                                  size_t input_size,
                                  unsigned char **output_data, 
                                  size_t *output_size);

int execute_encryption_operations(const program_config_t *config, 
                                 const unsigned char *input_data, 
                                 size_t input_size,
                                 unsigned char **output_data, 
                                 size_t *output_size);

#endif