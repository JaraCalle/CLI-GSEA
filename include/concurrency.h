#ifndef CONCURRENCY_H
#define CONCURRENCY_H

#include <pthread.h>
#include "args_parser.h"

// Estructura para pasar datos a cada hilo
typedef struct {
    const program_config_t *config;
    char *input_file;
    char *output_file;
    int thread_id;
    int success;
    char error_message[256];
} thread_data_t;

// Estructura para gestionar todos los hilos
typedef struct {
    pthread_t *threads;
    thread_data_t *thread_data;
    int num_threads;
    int active_threads;
    pthread_mutex_t mutex;
} thread_pool_t;

// Funciones existentes
void* process_single_file(void *arg);
int process_directory_concurrent(const program_config_t *config);
char* generate_output_path(const char *input_path, const char *output_dir, const program_config_t *config);
int init_thread_pool(thread_pool_t *pool, int num_threads);
void free_thread_pool(thread_pool_t *pool);

#endif