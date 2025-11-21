#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../include/archive.h"
#include "../include/dir_utils.h"
#include "../include/file_manager.h"
#include "../include/operations.h"

#define ARCHIVE_MAGIC "GSEAARCHv1"
#define ARCHIVE_HEADER_SIZE 10
#define ARCHIVE_IO_BUFFER (64 * 1024)

typedef struct {
    unsigned char data[ARCHIVE_IO_BUFFER];
} io_buffer_t;

static int create_temp_archive_file(char *buffer, size_t length) {
    static const char template_path[] = "/tmp/gsea-archive-XXXXXX";
    if (length <= sizeof(template_path)) {
        return -1;
    }

    strncpy(buffer, template_path, length);
    buffer[length - 1] = '\0';

    int fd = mkstemp(buffer);
    if (fd == -1) {
        fprintf(stderr, "Error: no se pudo crear archivo temporal - %s\n", strerror(errno));
        return -1;
    }

    close(fd);
    return 0;
}

static void free_file_list(FileList *list) {
    if (!list || !list->paths) {
        return;
    }

    for (size_t i = 0; i < list->count; ++i) {
        free(list->paths[i]);
    }

    free(list->paths);
    list->paths = NULL;
    list->count = 0;
}

static const char *compute_relative_path(const char *base, const char *absolute) {
    size_t base_len = strlen(base);
    if (base_len > 0 && strncmp(base, absolute, base_len) == 0) {
        const char *relative = absolute + base_len;
        if (*relative == '/') {
            relative++;
        }
        return relative;
    }

    return absolute;
}

static FILE *open_stream(const char *path, const char *mode, const char *action) {
    FILE *stream = fopen(path, mode);
    if (!stream) {
        fprintf(stderr, "Error: no se pudo %s '%s' - %s\n", action, path, strerror(errno));
    }
    return stream;
}

static int ensure_parent_directory(const char *full_path) {
    char *path_copy = strdup(full_path);
    if (!path_copy) {
        fprintf(stderr, "Error: sin memoria para crear directorios de '%s'\n", full_path);
        return -1;
    }

    char *parent = dirname(path_copy);
    int status = create_directory(parent);
    free(path_copy);
    return status;
}

static int copy_stream(FILE *in, FILE *out, size_t total_bytes, io_buffer_t *buffer) {
    size_t remaining = total_bytes;

    while (remaining > 0) {
        size_t chunk = remaining > sizeof(buffer->data) ? sizeof(buffer->data) : remaining;
        size_t read_bytes = fread(buffer->data, 1, chunk, in);

        if (read_bytes == 0) {
            if (ferror(in)) {
                fprintf(stderr, "Error: fallo leyendo datos - %s\n", strerror(errno));
            } else {
                fprintf(stderr, "Error: fin de archivo inesperado\n");
            }
            return -1;
        }

        if (fwrite(buffer->data, 1, read_bytes, out) != read_bytes) {
            fprintf(stderr, "Error: no se pudo escribir datos - %s\n", strerror(errno));
            return -1;
        }

        remaining -= read_bytes;
    }

    return 0;
}

static int write_archive_header(FILE *out, size_t file_count) {
    if (fwrite(ARCHIVE_MAGIC, 1, ARCHIVE_HEADER_SIZE, out) != ARCHIVE_HEADER_SIZE) {
        fprintf(stderr, "Error: no se pudo escribir el encabezado del archive.\n");
        return -1;
    }

    if (fwrite(&file_count, sizeof(size_t), 1, out) != 1) {
        fprintf(stderr, "Error: no se pudo escribir el número de archivos en el archive.\n");
        return -1;
    }

    return 0;
}

static int read_archive_header(FILE *in, size_t *file_count) {
    unsigned char header[ARCHIVE_HEADER_SIZE];
    if (fread(header, 1, ARCHIVE_HEADER_SIZE, in) != ARCHIVE_HEADER_SIZE) {
        fprintf(stderr, "Error: encabezado inválido en archive.\n");
        return -1;
    }

    if (memcmp(header, ARCHIVE_MAGIC, ARCHIVE_HEADER_SIZE) != 0) {
        fprintf(stderr, "Error: el archivo no es un archive válido.\n");
        return -1;
    }

    if (fread(file_count, sizeof(size_t), 1, in) != 1) {
        fprintf(stderr, "Error: no se pudo leer el número de archivos del archive.\n");
        return -1;
    }

    return 0;
}

static int write_archive_entry(FILE *out,
                               const char *base_dir,
                               const char *absolute_path,
                               io_buffer_t *buffer) {
    const char *relative = compute_relative_path(base_dir, absolute_path);
    size_t path_len = strlen(relative);

    if (fwrite(&path_len, sizeof(size_t), 1, out) != 1) {
        fprintf(stderr, "Error: no se pudo escribir la longitud de la ruta en el archive.\n");
        return -1;
    }

    if (fwrite(relative, 1, path_len, out) != path_len) {
        fprintf(stderr, "Error: no se pudo escribir la ruta en el archive.\n");
        return -1;
    }

    struct stat st;
    if (stat(absolute_path, &st) != 0) {
        fprintf(stderr, "Error: no se pudo obtener tamaño de '%s' - %s\n", absolute_path, strerror(errno));
        return -1;
    }

    size_t file_size = (size_t)st.st_size;
    if (fwrite(&file_size, sizeof(size_t), 1, out) != 1) {
        fprintf(stderr, "Error: no se pudo escribir el tamaño de '%s' en el archive.\n", relative);
        return -1;
    }

    FILE *in = open_stream(absolute_path, "rb", "abrir");
    if (!in) {
        return -1;
    }

    int status = copy_stream(in, out, file_size, buffer);
    fclose(in);

    if (status == 0) {
        printf("  + Añadido '%s' (%zu bytes)\n", relative, file_size);
    }

    return status;
}

static int extract_archive_entry(FILE *in,
                                 const char *output_dir,
                                 io_buffer_t *buffer) {
    size_t path_len = 0;
    if (fread(&path_len, sizeof(size_t), 1, in) != 1) {
        fprintf(stderr, "Error: no se pudo leer la longitud de la ruta del archive.\n");
        return -1;
    }

    if (path_len == 0 || path_len >= MAX_PATH_LENGTH) {
        fprintf(stderr, "Error: ruta inválida dentro del archive.\n");
        return -1;
    }

    char *relative = (char *)malloc(path_len + 1);
    if (!relative) {
        fprintf(stderr, "Error: sin memoria para ruta del archivo dentro del archive.\n");
        return -1;
    }

    if (fread(relative, 1, path_len, in) != path_len) {
        fprintf(stderr, "Error: no se pudo leer ruta del archive.\n");
        free(relative);
        return -1;
    }
    relative[path_len] = '\0';

    size_t file_size = 0;
    if (fread(&file_size, sizeof(size_t), 1, in) != 1) {
        fprintf(stderr, "Error: no se pudo leer tamaño de archivo en archive.\n");
        free(relative);
        return -1;
    }

    char full_path[MAX_PATH_LENGTH];
    if (snprintf(full_path, sizeof(full_path), "%s/%s", output_dir, relative) >= (int)sizeof(full_path)) {
        fprintf(stderr, "Error: la ruta destino es demasiado larga.\n");
        free(relative);
        return -1;
    }

    if (ensure_parent_directory(full_path) != 0) {
        fprintf(stderr, "Error: no se pudo crear directorio para '%s'.\n", full_path);
        free(relative);
        return -1;
    }

    FILE *out = open_stream(full_path, "wb", "crear");
    if (!out) {
        free(relative);
        return -1;
    }

    int status = copy_stream(in, out, file_size, buffer);
    fclose(out);

    if (status == 0) {
        printf("  + Extraído '%s' (%zu bytes)\n", relative, file_size);
    }

    free(relative);
    return status;
}

char *process_output_path(const program_config_t *config) {
    if (strlen(config->output_path) > 0) {
        return strdup(config->output_path);
    }

    char *auto_path = generate_auto_output_path(config->input_path, config);
    if (auto_path) {
        printf("Salida automática generada: %s\n", auto_path);
        return auto_path;
    }

    return strdup("salida.gsea");
}

const char *get_auto_extension(const program_config_t *config) {
    if ((config->operations & OP_COMPRESS) && (config->operations & OP_ENCRYPT)) {
        return ".gsea";
    }

    if (config->operations & OP_COMPRESS) {
        return ".rle";
    }

    if (config->operations & OP_ENCRYPT) {
        return ".enc";
    }

    if (config->operations & OP_DECOMPRESS) {
        return "";
    }

    if (config->operations & OP_DECRYPT) {
        return "";
    }

    return "";
}

char *generate_auto_output_path(const char *input_path, const program_config_t *config) {
    const char *base_name = strrchr(input_path, '/');
    if (base_name == NULL) {
        base_name = input_path;
    } else {
        base_name++;
    }

    char *name_copy = NULL;
    struct stat path_stat;
    if (stat(input_path, &path_stat) == 0 && S_ISREG(path_stat.st_mode)) {
        name_copy = strdup(base_name);
        if (name_copy) {
            char *dot = strrchr(name_copy, '.');
            if (dot) {
                *dot = '\0';
            }

            base_name = name_copy;
        }
    }

    const char *extension = get_auto_extension(config);
    size_t path_len = strlen(base_name) + strlen(extension) + 1;
    char *output_name = (char *)malloc(path_len);
    if (output_name) {
        snprintf(output_name, path_len, "%s%s", base_name, extension);
    }

    free(name_copy);
    return output_name;
}

int create_directory_archive_file(const char *dir_path, const char *archive_path) {
    if (!dir_path || !archive_path) {
        return -1;
    }

    FileList list = {0};
    read_directory_recursive(dir_path, &list);

    if (list.count == 0) {
        fprintf(stderr, "Error: el directorio '%s' está vacío o no se pudo leer.\n", dir_path);
        free_file_list(&list);
        return -1;
    }

    FILE *out = open_stream(archive_path, "wb", "crear");
    if (!out) {
        free_file_list(&list);
        return -1;
    }

    io_buffer_t buffer = {0};
    int status = write_archive_header(out, list.count);

    for (size_t i = 0; status == 0 && i < list.count; ++i) {
        status = write_archive_entry(out, dir_path, list.paths[i], &buffer);
    }

    if (status == 0) {
        printf("Archive generado en '%s'\n", archive_path);
    } else {
        unlink(archive_path);
    }

    fclose(out);
    free_file_list(&list);
    return status;
}

int extract_directory_archive_file(const char *archive_path, const char *output_dir) {
    if (!archive_path || !output_dir) {
        return -1;
    }

    FILE *in = open_stream(archive_path, "rb", "abrir");
    if (!in) {
        return -1;
    }

    size_t file_count = 0;
    int status = read_archive_header(in, &file_count);

    if (status == 0) {
        status = create_directory(output_dir);
        if (status != 0) {
            fprintf(stderr, "Error: no se pudo preparar el directorio de salida '%s'.\n", output_dir);
        }
    }

    io_buffer_t buffer = {0};
    for (size_t i = 0; status == 0 && i < file_count; ++i) {
        status = extract_archive_entry(in, output_dir, &buffer);
    }

    if (status == 0) {
        printf("Archive extraído en '%s'\n", output_dir);
    }

    fclose(in);
    return status;
}

int is_gsea_archive_file(const char *file_path) {
    if (file_path == NULL) {
        return 0;
    }

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        return 0;
    }

    unsigned char header[ARCHIVE_HEADER_SIZE];
    ssize_t read_bytes = read(fd, header, sizeof(header));
    close(fd);

    if (read_bytes != (ssize_t)sizeof(header)) {
        return 0;
    }

    return (memcmp(header, ARCHIVE_MAGIC, ARCHIVE_HEADER_SIZE) == 0);
}

static int process_directory_with_pipeline(const program_config_t *config,
                                           const char *output_path) {
    char temp_path[MAX_PATH_LENGTH];
    if (create_temp_archive_file(temp_path, sizeof(temp_path)) != 0) {
        return -1;
    }

    if (create_directory_archive_file(config->input_path, temp_path) != 0) {
        unlink(temp_path);
        return -1;
    }

    int result = execute_file_pipeline(config, temp_path, output_path);
    unlink(temp_path);
    return result;
}

static int process_archive_to_directory(const program_config_t *config,
                                        const char *output_path) {
    char temp_path[MAX_PATH_LENGTH];
    if (create_temp_archive_file(temp_path, sizeof(temp_path)) != 0) {
        return -1;
    }

    int result = execute_file_pipeline(config, config->input_path, temp_path);
    if (result != 0) {
        unlink(temp_path);
        return result;
    }

    result = extract_directory_archive_file(temp_path, output_path);
    unlink(temp_path);
    return result;
}

int compress_directory_only(const program_config_t *config, const char *output_path) {
    printf("Comprimiendo directorio: %s\n", config->input_path);
    return process_directory_with_pipeline(config, output_path);
}

int compress_and_encrypt_directory(const program_config_t *config, const char *output_path) {
    printf("Comprimiendo y encriptando directorio: %s\n", config->input_path);
    return process_directory_with_pipeline(config, output_path);
}

int encrypt_directory_only(const program_config_t *config, const char *output_path) {
    printf("Encriptando directorio: %s\n", config->input_path);
    return process_directory_with_pipeline(config, output_path);
}

int decompress_directory_only(const program_config_t *config, const char *output_path) {
    printf("Descomprimiendo archive: %s\n", config->input_path);
    return process_archive_to_directory(config, output_path);
}

int decrypt_directory_only(const program_config_t *config, const char *output_path) {
    printf("Desencriptando archive: %s\n", config->input_path);
    return process_archive_to_directory(config, output_path);
}

int decrypt_and_decompress_directory(const program_config_t *config, const char *output_path) {
    printf("Desencriptando y descomprimiendo archive: %s\n", config->input_path);
    return process_archive_to_directory(config, output_path);
}