#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "args_parser.h"

int execute_file_pipeline(const program_config_t *config,
                          const char *input_path,
                          const char *output_path);

#endif