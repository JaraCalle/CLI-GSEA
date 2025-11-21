#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/operations.h"
#include "../include/compression.h"
#include "../include/compression_huffman.h"
#include "../include/compression_lzw.h"
#include "../include/encryption.h"
#include "../include/file_manager.h"

#define STREAM_MAGIC "GSC1"
#define STREAM_VERSION 1
#define STREAM_HEADER_SIZE 12
#define CHUNK_HEADER_SIZE 8
#define DEFAULT_STREAM_CHUNK_SIZE (1024 * 1024)

typedef enum {
    STAGE_COMPRESS,
    STAGE_DECOMPRESS,
    STAGE_ENCRYPT,
    STAGE_DECRYPT,
    STAGE_COPY
} stage_type_t;

static size_t get_chunk_size(void) {
    return DEFAULT_STREAM_CHUNK_SIZE;
}

static void store_u16_le(unsigned char *dst, uint16_t value) {
    dst[0] = (unsigned char)(value & 0xFFu);
    dst[1] = (unsigned char)((value >> 8) & 0xFFu);
}

static void store_u32_le(unsigned char *dst, uint32_t value) {
    dst[0] = (unsigned char)(value & 0xFFu);
    dst[1] = (unsigned char)((value >> 8) & 0xFFu);
    dst[2] = (unsigned char)((value >> 16) & 0xFFu);
    dst[3] = (unsigned char)((value >> 24) & 0xFFu);
}

static uint16_t load_u16_le(const unsigned char *src) {
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

static uint32_t load_u32_le(const unsigned char *src) {
    return (uint32_t)src[0] |
           ((uint32_t)src[1] << 8) |
           ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

static int write_all(int fd, const unsigned char *data, size_t size) {
    size_t total_written = 0;
    while (total_written < size) {
        ssize_t written = write(fd, data + total_written, size - total_written);
        if (written == -1) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        total_written += (size_t)written;
    }
    return 0;
}

static int ensure_parent_directory(const char *path) {
    char parent[MAX_PATH_LENGTH];
    strncpy(parent, path, MAX_PATH_LENGTH - 1);
    parent[MAX_PATH_LENGTH - 1] = '\0';

    char *last_slash = strrchr(parent, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';
        if (strlen(parent) > 0) {
            if (create_directory(parent) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

static int create_temp_path(char *buffer, size_t length) {
    if (length < 1) {
        return -1;
    }

    snprintf(buffer, length, "/tmp/gsea-stage-%d-XXXXXX", getpid());
    int fd = mkstemp(buffer);
    if (fd == -1) {
        return -1;
    }
    close(fd);
    return 0;
}

static int write_stream_header(int fd, compression_alg_t alg, uint32_t chunk_size) {
    unsigned char header[12];
    memcpy(header, STREAM_MAGIC, 4);
    header[4] = (unsigned char)alg;
    header[5] = 0;
    store_u16_le(header + 6, STREAM_VERSION);
    store_u32_le(header + 8, chunk_size);
    return write_all(fd, header, sizeof(header));
}

static int read_stream_header(int fd, compression_alg_t *alg, uint32_t *chunk_size) {
    unsigned char header[12];
    ssize_t read_bytes = read(fd, header, sizeof(header));
    if (read_bytes == -1) {
        return -1;
    }
    if (read_bytes != (ssize_t)sizeof(header)) {
        if (lseek(fd, 0, SEEK_SET) == -1) {
            return -1;
        }
        return 0;
    }

    if (memcmp(header, STREAM_MAGIC, 4) != 0) {
        if (lseek(fd, 0, SEEK_SET) == -1) {
            return -1;
        }
        return 0;
    }

    uint16_t version = load_u16_le(header + 6);
    if (version != STREAM_VERSION) {
        fprintf(stderr, "Error: versión de stream incompatible (%u)\n", version);
        return -1;
    }

    *alg = (compression_alg_t)header[4];
    *chunk_size = load_u32_le(header + 8);
    if (*chunk_size == 0) {
        *chunk_size = DEFAULT_STREAM_CHUNK_SIZE;
    }
    return 1;
}

static int write_chunk_header(int fd, uint32_t original_size, uint32_t compressed_size) {
    unsigned char buffer[8];
    store_u32_le(buffer, original_size);
    store_u32_le(buffer + 4, compressed_size);
    return write_all(fd, buffer, sizeof(buffer));
}

static int read_chunk_header(int fd, uint32_t *original_size, uint32_t *compressed_size, int *eof) {
    unsigned char buffer[8];
    ssize_t read_bytes = read(fd, buffer, sizeof(buffer));
    if (read_bytes == 0) {
        *eof = 1;
        return 0;
    }
    if (read_bytes == -1) {
        return -1;
    }
    if (read_bytes != (ssize_t)sizeof(buffer)) {
        return -2;
    }

    *original_size = load_u32_le(buffer);
    *compressed_size = load_u32_le(buffer + 4);
    *eof = 0;
    return 0;
}

static compression_result_t run_compress_chunk(compression_alg_t alg,
                                               const unsigned char *data,
                                               size_t size) {
    switch (alg) {
        case COMP_ALG_RLE:
            return compress_rle(data, size);
        case COMP_ALG_HUFFMAN:
            return compress_huffman_wrapper(data, size);
        case COMP_ALG_LZW:
            return compress_lzw(data, size);
        default: {
            compression_result_t invalid = {NULL, 0, -99};
            return invalid;
        }
    }
}

static compression_result_t run_decompress_chunk(compression_alg_t alg,
                                                 const unsigned char *data,
                                                 size_t size) {
    switch (alg) {
        case COMP_ALG_RLE:
            return decompress_rle(data, size);
        case COMP_ALG_HUFFMAN:
            return decompress_huffman_wrapper(data, size);
        case COMP_ALG_LZW:
            return decompress_lzw(data, size);
        default: {
            compression_result_t invalid = {NULL, 0, -99};
            return invalid;
        }
    }
}

static int compress_file_chunked(const program_config_t *config,
                                 const char *input_path,
                                 const char *output_path,
                                 size_t chunk_size) {
    if (ensure_parent_directory(output_path) != 0) {
        fprintf(stderr, "Error: No se pudo preparar el directorio de salida '%s'\n", output_path);
        return -1;
    }

    int in_fd = open(input_path, O_RDONLY);
    if (in_fd == -1) {
        fprintf(stderr, "Error: No se pudo abrir '%s' para lectura - %s\n",
                input_path, strerror(errno));
        return -1;
    }

    int out_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1) {
        fprintf(stderr, "Error: No se pudo abrir '%s' para escritura - %s\n",
                output_path, strerror(errno));
        close(in_fd);
        return -1;
    }

    unsigned char *buffer = (unsigned char *)malloc(chunk_size);
    if (!buffer) {
        fprintf(stderr, "Error: No se pudo asignar buffer de %zu bytes\n", chunk_size);
        close(in_fd);
        close(out_fd);
        return -1;
    }

    if (write_stream_header(out_fd, config->comp_alg, (uint32_t)chunk_size) != 0) {
        fprintf(stderr, "Error: No se pudo escribir header de stream en '%s'\n", output_path);
        free(buffer);
        close(in_fd);
        close(out_fd);
        return -1;
    }

    size_t total_input_bytes = 0;
    size_t total_output_bytes = STREAM_HEADER_SIZE;
    size_t total_payload_bytes = 0;

    ssize_t bytes_read;
    while ((bytes_read = read(in_fd, buffer, chunk_size)) > 0) {
        total_input_bytes += (size_t)bytes_read;
        compression_result_t compressed = run_compress_chunk(config->comp_alg, buffer, (size_t)bytes_read);
        if (compressed.error != 0) {
            fprintf(stderr, "Error: Falló la compresión del chunk (código %d)\n", compressed.error);
            free(buffer);
            close(in_fd);
            close(out_fd);
            return -1;
        }

        if (write_chunk_header(out_fd, (uint32_t)bytes_read, (uint32_t)compressed.size) != 0) {
            fprintf(stderr, "Error: No se pudo escribir header de chunk en '%s'\n", output_path);
            free(buffer);
            free_compression_result(&compressed);
            close(in_fd);
            close(out_fd);
            return -1;
        }

        total_output_bytes += CHUNK_HEADER_SIZE;

        total_payload_bytes += compressed.size;
        total_output_bytes += compressed.size;

        if (compressed.size > 0) {
            if (write_all(out_fd, compressed.data, compressed.size) != 0) {
                fprintf(stderr, "Error: No se pudo escribir datos de chunk en '%s'\n", output_path);
                free(buffer);
                free_compression_result(&compressed);
                close(in_fd);
                close(out_fd);
                return -1;
            }
        }

        free_compression_result(&compressed);
    }

    if (bytes_read == -1) {
        fprintf(stderr, "Error: Falló la lectura de '%s' - %s\n", input_path, strerror(errno));
        free(buffer);
        close(in_fd);
        close(out_fd);
        return -1;
    }

    double ratio = 0.0;
    if (total_input_bytes > 0) {
        ratio = (double)total_output_bytes / (double)total_input_bytes;
    }

    printf("    ✓ Compresión completada: %zu → %zu bytes (ratio: %.2f)\n",
           total_input_bytes, total_output_bytes, ratio);
    if (total_payload_bytes != total_output_bytes) {
        printf("      Detalle: datos comprimidos = %zu bytes, overhead = %zu bytes\n",
               total_payload_bytes, total_output_bytes - total_payload_bytes);
    }

    free(buffer);
    close(in_fd);
    close(out_fd);
    return 0;
}

static int decompress_legacy_file(const program_config_t *config,
                                  const char *input_path,
                                  const char *output_path) {
    unsigned char *input_data = NULL;
    size_t input_size = 0;
    if (read_file(input_path, &input_data, &input_size) != 0) {
        fprintf(stderr, "Error: No se pudo leer '%s' para descompresión legacy\n", input_path);
        return -1;
    }

    compression_result_t result = run_decompress_chunk(config->comp_alg, input_data, input_size);
    free(input_data);

    if (result.error != 0) {
        fprintf(stderr, "Error: Descompresión legacy falló (código %d)\n", result.error);
        return -1;
    }

    if (write_file(output_path, result.data, result.size) != 0) {
        fprintf(stderr, "Error: No se pudo escribir salida legacy '%s'\n", output_path);
        free_compression_result(&result);
        return -1;
    }

    printf("    ✓ Descompresión completada: %zu → %zu bytes\n", input_size, result.size);

    free_compression_result(&result);
    return 0;
}

static int decompress_file_chunked(const program_config_t *config,
                                   const char *input_path,
                                   const char *output_path,
                                   size_t chunk_size) {
    if (ensure_parent_directory(output_path) != 0) {
        fprintf(stderr, "Error: No se pudo preparar directorio para '%s'\n", output_path);
        return -1;
    }

    int in_fd = open(input_path, O_RDONLY);
    if (in_fd == -1) {
        fprintf(stderr, "Error: No se pudo abrir '%s' - %s\n", input_path, strerror(errno));
        return -1;
    }

    compression_alg_t header_alg = COMP_ALG_NONE;
    uint32_t header_chunk_size = 0;
    int header_status = read_stream_header(in_fd, &header_alg, &header_chunk_size);
    if (header_status == -1) {
        close(in_fd);
        return -1;
    }

    if (header_status == 0) {
        close(in_fd);
        return decompress_legacy_file(config, input_path, output_path);
    }

    if (header_alg != config->comp_alg) {
        fprintf(stderr, "Error: Algoritmo del archivo (%d) no coincide con configuración (%d)\n",
                header_alg, config->comp_alg);
        close(in_fd);
        return -1;
    }

    if (header_chunk_size > 0) {
        chunk_size = header_chunk_size;
    }

    int out_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1) {
        fprintf(stderr, "Error: No se pudo abrir '%s' para escritura - %s\n",
                output_path, strerror(errno));
        close(in_fd);
        return -1;
    }

    unsigned char *compressed_buffer = NULL;
    size_t compressed_capacity = chunk_size * 2;
    if (compressed_capacity < chunk_size + 256) {
        compressed_capacity = chunk_size + 256;
    }

    compressed_buffer = (unsigned char *)malloc(compressed_capacity);
    if (!compressed_buffer) {
        fprintf(stderr, "Error: No se pudo asignar buffer de descompresión\n");
        close(in_fd);
        close(out_fd);
        return -1;
    }

    size_t total_compressed_bytes = STREAM_HEADER_SIZE;
    size_t total_decompressed_bytes = 0;

    while (1) {
        uint32_t raw_size = 0;
        uint32_t compressed_size = 0;
        int eof = 0;
        int rc = read_chunk_header(in_fd, &raw_size, &compressed_size, &eof);
        if (rc == -1) {
            fprintf(stderr, "Error: No se pudo leer header de chunk - %s\n", strerror(errno));
            free(compressed_buffer);
            close(in_fd);
            close(out_fd);
            return -1;
        }
        if (rc == -2) {
            fprintf(stderr, "Error: Archivo comprimido truncado\n");
            free(compressed_buffer);
            close(in_fd);
            close(out_fd);
            return -1;
        }
        if (eof) {
            break;
        }

        if (compressed_size > compressed_capacity) {
            unsigned char *bigger = (unsigned char *)realloc(compressed_buffer, compressed_size);
            if (!bigger) {
                fprintf(stderr, "Error: No se pudo ampliar buffer de compresión\n");
                free(compressed_buffer);
                close(in_fd);
                close(out_fd);
                return -1;
            }
            compressed_buffer = bigger;
            compressed_capacity = compressed_size;
        }

        size_t total_read = 0;
        while (total_read < compressed_size) {
            ssize_t chunk_read = read(in_fd, compressed_buffer + total_read, compressed_size - total_read);
            if (chunk_read == -1) {
                if (errno == EINTR) {
                    continue;
                }
                fprintf(stderr, "Error: Lectura fallida de chunk - %s\n", strerror(errno));
                free(compressed_buffer);
                close(in_fd);
                close(out_fd);
                return -1;
            }
            if (chunk_read == 0) {
                fprintf(stderr, "Error: Archivo comprimido incompleto\n");
                free(compressed_buffer);
                close(in_fd);
                close(out_fd);
                return -1;
            }
            total_read += (size_t)chunk_read;
        }

        total_compressed_bytes += CHUNK_HEADER_SIZE + compressed_size;

        compression_result_t decompressed = run_decompress_chunk(config->comp_alg,
                                                                  compressed_buffer,
                                                                  compressed_size);
        if (decompressed.error != 0) {
            fprintf(stderr, "Error: Descompresión de chunk falló (código %d)\n", decompressed.error);
            free(compressed_buffer);
            close(in_fd);
            close(out_fd);
            return -1;
        }

        if (raw_size != 0 && decompressed.size != raw_size) {
            /* Mantener compatibilidad aun si tamaños difieren, pero advertir. */
            if (decompressed.size != raw_size) {
                fprintf(stderr, "Advertencia: Tamaño chunk esperado %u, obtenido %zu\n",
                        raw_size, decompressed.size);
            }
        }

        total_decompressed_bytes += decompressed.size;

        if (decompressed.size > 0) {
            if (write_all(out_fd, decompressed.data, decompressed.size) != 0) {
                fprintf(stderr, "Error: No se pudo escribir salida descomprimida - %s\n",
                        strerror(errno));
                free(compressed_buffer);
                free_compression_result(&decompressed);
                close(in_fd);
                close(out_fd);
                return -1;
            }
        }

        free_compression_result(&decompressed);
    }

    printf("    ✓ Descompresión completada: %zu → %zu bytes\n",
           total_compressed_bytes, total_decompressed_bytes);

    free(compressed_buffer);
    close(in_fd);
    close(out_fd);
    return 0;
}

static int encrypt_file_stream(const program_config_t *config,
                               const char *input_path,
                               const char *output_path,
                               int encrypt) {
    (void)encrypt; /* Operación simétrica */

    if (ensure_parent_directory(output_path) != 0) {
        fprintf(stderr, "Error: No se pudo preparar directorio para '%s'\n", output_path);
        return -1;
    }

    size_t key_len = strlen(config->key);
    if (key_len == 0) {
        fprintf(stderr, "Error: Clave vacía para operación de encriptación\n");
        return -1;
    }

    int in_fd = open(input_path, O_RDONLY);
    if (in_fd == -1) {
        fprintf(stderr, "Error: No se pudo abrir '%s' - %s\n", input_path, strerror(errno));
        return -1;
    }

    int out_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1) {
        fprintf(stderr, "Error: No se pudo abrir '%s' - %s\n", output_path, strerror(errno));
        close(in_fd);
        return -1;
    }

    size_t chunk_size = get_chunk_size();
    unsigned char *buffer = (unsigned char *)malloc(chunk_size);
    unsigned char *output = (unsigned char *)malloc(chunk_size);
    if (!buffer || !output) {
        fprintf(stderr, "Error: No se pudo asignar buffers para encriptación\n");
        free(buffer);
        free(output);
        close(in_fd);
        close(out_fd);
        return -1;
    }

    size_t key_index = 0;
    ssize_t bytes_read;
    while ((bytes_read = read(in_fd, buffer, chunk_size)) > 0) {
        for (ssize_t i = 0; i < bytes_read; ++i) {
            output[i] = buffer[i] ^ (unsigned char)config->key[key_index];
            key_index = (key_index + 1) % key_len;
        }

        if (write_all(out_fd, output, (size_t)bytes_read) != 0) {
            fprintf(stderr, "Error: No se pudo escribir datos en '%s'\n", output_path);
            free(buffer);
            free(output);
            close(in_fd);
            close(out_fd);
            return -1;
        }
    }

    if (bytes_read == -1) {
        fprintf(stderr, "Error: Falló la lectura de '%s' - %s\n", input_path, strerror(errno));
        free(buffer);
        free(output);
        close(in_fd);
        close(out_fd);
        return -1;
    }

    free(buffer);
    free(output);
    close(in_fd);
    close(out_fd);
    return 0;
}

static int copy_file_stream(const char *input_path, const char *output_path) {
    if (ensure_parent_directory(output_path) != 0) {
        fprintf(stderr, "Error: No se pudo preparar directorio para '%s'\n", output_path);
        return -1;
    }

    int in_fd = open(input_path, O_RDONLY);
    if (in_fd == -1) {
        fprintf(stderr, "Error: No se pudo abrir '%s' - %s\n", input_path, strerror(errno));
        return -1;
    }

    int out_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1) {
        fprintf(stderr, "Error: No se pudo abrir '%s' - %s\n", output_path, strerror(errno));
        close(in_fd);
        return -1;
    }

    size_t chunk_size = get_chunk_size();
    unsigned char *buffer = (unsigned char *)malloc(chunk_size);
    if (!buffer) {
        fprintf(stderr, "Error: No se pudo asignar buffer de copia\n");
        close(in_fd);
        close(out_fd);
        return -1;
    }

    ssize_t bytes_read;
    while ((bytes_read = read(in_fd, buffer, chunk_size)) > 0) {
        if (write_all(out_fd, buffer, (size_t)bytes_read) != 0) {
            fprintf(stderr, "Error: No se pudo escribir en '%s'\n", output_path);
            free(buffer);
            close(in_fd);
            close(out_fd);
            return -1;
        }
    }

    if (bytes_read == -1) {
        fprintf(stderr, "Error: Falló la lectura de '%s' - %s\n", input_path, strerror(errno));
        free(buffer);
        close(in_fd);
        close(out_fd);
        return -1;
    }

    free(buffer);
    close(in_fd);
    close(out_fd);
    return 0;
}

static int run_stage(stage_type_t stage,
                     const program_config_t *config,
                     const char *input_path,
                     const char *output_path) {
    switch (stage) {
        case STAGE_COMPRESS:
            printf("  → Comprimiendo '%s' → '%s'\n", input_path, output_path);
            return compress_file_chunked(config, input_path, output_path, get_chunk_size());
        case STAGE_DECOMPRESS:
            printf("  → Descomprimiendo '%s' → '%s'\n", input_path, output_path);
            return decompress_file_chunked(config, input_path, output_path, get_chunk_size());
        case STAGE_ENCRYPT:
            printf("  → Encriptando '%s' → '%s'\n", input_path, output_path);
            return encrypt_file_stream(config, input_path, output_path, 1);
        case STAGE_DECRYPT:
            printf("  → Desencriptando '%s' → '%s'\n", input_path, output_path);
            return encrypt_file_stream(config, input_path, output_path, 0);
        case STAGE_COPY:
            printf("  → Copiando '%s' → '%s'\n", input_path, output_path);
            return copy_file_stream(input_path, output_path);
    }
    return -1;
}

int execute_file_pipeline(const program_config_t *config,
                          const char *input_path,
                          const char *output_path) {
    stage_type_t stages[3];
    size_t stage_count = 0;

    if ((config->operations & OP_COMPRESS) && (config->operations & OP_ENCRYPT)) {
        stages[stage_count++] = STAGE_COMPRESS;
        stages[stage_count++] = STAGE_ENCRYPT;
        printf("Orden de ejecución: COMPRIMIR → ENCRIPTAR\n");
    } else if ((config->operations & OP_DECRYPT) && (config->operations & OP_DECOMPRESS)) {
        stages[stage_count++] = STAGE_DECRYPT;
        stages[stage_count++] = STAGE_DECOMPRESS;
        printf("Orden de ejecución: DESENCRIPTAR → DESCOMPRIMIR\n");
    } else if (config->operations & OP_COMPRESS) {
        stages[stage_count++] = STAGE_COMPRESS;
        printf("Orden de ejecución: SOLO COMPRIMIR\n");
    } else if (config->operations & OP_DECOMPRESS) {
        stages[stage_count++] = STAGE_DECOMPRESS;
        printf("Orden de ejecución: SOLO DESCOMPRIMIR\n");
    } else if (config->operations & OP_ENCRYPT) {
        stages[stage_count++] = STAGE_ENCRYPT;
        printf("Orden de ejecución: SOLO ENCRIPTAR\n");
    } else if (config->operations & OP_DECRYPT) {
        stages[stage_count++] = STAGE_DECRYPT;
        printf("Orden de ejecución: SOLO DESENCRIPTAR\n");
    } else {
        stages[stage_count++] = STAGE_COPY;
        printf("Orden de ejecución: COPIAR\n");
    }

    const char *current_input = input_path;
    char temp_path[MAX_PATH_LENGTH];
    int temp_active = 0;

    for (size_t i = 0; i < stage_count; ++i) {
        int is_last = (i == stage_count - 1);
        char next_temp[MAX_PATH_LENGTH];
        const char *target_path = NULL;

        if (is_last) {
            target_path = output_path;
        } else {
            if (create_temp_path(next_temp, sizeof(next_temp)) != 0) {
                fprintf(stderr, "Error: No se pudo crear archivo temporal\n");
                if (temp_active) {
                    unlink(temp_path);
                }
                return -1;
            }
            target_path = next_temp;
        }

        if (run_stage(stages[i], config, current_input, target_path) != 0) {
            if (!is_last) {
                unlink(target_path);
            }
            if (temp_active) {
                unlink(temp_path);
            }
            return -1;
        }

        if (temp_active) {
            unlink(temp_path);
            temp_active = 0;
        }

        if (!is_last) {
            strncpy(temp_path, target_path, sizeof(temp_path));
            temp_path[sizeof(temp_path) - 1] = '\0';
            temp_active = 1;
            current_input = temp_path;
        } else {
            current_input = output_path;
        }
    }

    if (temp_active) {
        unlink(temp_path);
    }

    return 0;
}