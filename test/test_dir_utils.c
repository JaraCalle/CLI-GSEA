#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <dirent.h>

#include "../include/dir_utils.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int ensure_directory(const char *path) {
    if (mkdir(path, 0755) == 0) {
        return 0;
    }

    if (errno == EEXIST) {
        return 0;
    }

    perror("mkdir");
    return -1;
}

static int create_file_with_content(const char *path, const char *content) {
    FILE *file = fopen(path, "w");
    if (!file) {
        perror("fopen");
        return -1;
    }

    if (fputs(content, file) == EOF) {
        perror("fputs");
        fclose(file);
        return -1;
    }

    if (fclose(file) != 0) {
        perror("fclose");
        return -1;
    }

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

static bool contains_path(const FileList *list, const char *target) {
    if (!list || !target) {
        return false;
    }

    for (size_t i = 0; i < list->count; ++i) {
        if (strcmp(list->paths[i], target) == 0) {
            return true;
        }
    }

    return false;
}

static int remove_path(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }

    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        if (!dir) {
            perror("opendir");
            return -1;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char child_path[PATH_MAX];
            snprintf(child_path, sizeof(child_path), "%s/%s", path, entry->d_name);
            if (remove_path(child_path) != 0) {
                closedir(dir);
                return -1;
            }
        }

        closedir(dir);
        if (rmdir(path) != 0) {
            perror("rmdir");
            return -1;
        }
    } else {
        if (unlink(path) != 0) {
            perror("unlink");
            return -1;
        }
    }

    return 0;
}

static int test_recursive_listing(void) {
    printf("[1] Lectura recursiva de directorios con archivos anidados...\n");

    char base_template[] = "/tmp/cli_gsea_dir_utils_testXXXXXX";
    char *base_dir = mkdtemp(base_template);
    if (!base_dir) {
        perror("mkdtemp");
        return 1;
    }

    char subdir_one[PATH_MAX];
    char subdir_two[PATH_MAX];
    char nested_dir[PATH_MAX];
    char file_root[PATH_MAX];
    char file_sub_one[PATH_MAX];
    char file_nested[PATH_MAX];

    snprintf(subdir_one, sizeof(subdir_one), "%s/subdir_one", base_dir);
    snprintf(subdir_two, sizeof(subdir_two), "%s/subdir_two", base_dir);
    snprintf(nested_dir, sizeof(nested_dir), "%s/subdir_two/nested", base_dir);
    snprintf(file_root, sizeof(file_root), "%s/root.txt", base_dir);
    snprintf(file_sub_one, sizeof(file_sub_one), "%s/subdir_one/file_one.txt", base_dir);
    snprintf(file_nested, sizeof(file_nested), "%s/subdir_two/nested/deep.txt", base_dir);

    int result = 0;
    FileList list = {0};

    if (create_file_with_content(file_root, "root") != 0) {
        result = 1;
        goto cleanup;
    }

    if (ensure_directory(subdir_one) != 0 || ensure_directory(subdir_two) != 0 || ensure_directory(nested_dir) != 0) {
        result = 1;
        goto cleanup;
    }

    if (create_file_with_content(file_sub_one, "sub one") != 0 ||
        create_file_with_content(file_nested, "nested") != 0) {
        result = 1;
        goto cleanup;
    }

    read_directory_recursive(base_dir, &list);

    size_t expected_files = 3;
    bool count_ok = (list.count == expected_files);
    bool files_ok = contains_path(&list, file_root) &&
                    contains_path(&list, file_sub_one) &&
                    contains_path(&list, file_nested);

    if (count_ok && files_ok) {
        printf("   ✓ Se recopilaron los %zu archivos esperados\n", expected_files);
    } else {
        printf("   ✗ Se esperaban %zu archivos, se obtuvieron %zu\n", expected_files, list.count);
        if (!files_ok) {
            printf("   ✗ No se encontraron todas las rutas esperadas\n");
        }
        result = 1;
    }

cleanup:
    free_file_list(&list);
    if (remove_path(base_dir) != 0) {
        fprintf(stderr, "Warning: no se pudo limpiar el directorio de pruebas %s\n", base_dir);
    }

    return result;
}

static int test_empty_directory(void) {
    printf("[2] Directorio vacío...\n");

    char base_template[] = "/tmp/cli_gsea_dir_utils_emptyXXXXXX";
    char *base_dir = mkdtemp(base_template);
    if (!base_dir) {
        perror("mkdtemp");
        return 1;
    }

    FileList list = {0};
    read_directory_recursive(base_dir, &list);

    int result = 0;
    if (list.count == 0) {
        printf("   ✓ No se encontraron archivos, como se esperaba\n");
    } else {
        printf("   ✗ Se encontraron %zu archivos en un directorio vacío\n", list.count);
        result = 1;
    }

    free_file_list(&list);

    if (remove_path(base_dir) != 0) {
        fprintf(stderr, "Warning: no se pudo eliminar %s\n", base_dir);
    }

    return result;
}

static int test_nonexistent_directory(void) {
    printf("[3] Directorio inexistente...\n");

    const char *fake_path = "/path/that/should/not/exist/cli_gsea";
    FileList list = {0};
    read_directory_recursive(fake_path, &list);

    if (list.count == 0 && list.paths == NULL) {
        printf("   ✓ No se añadieron rutas al fallar la apertura del directorio\n");
        return 0;
    }

    printf("   ✗ Se añadieron rutas inesperadas a la lista\n");
    free_file_list(&list);
    return 1;
}

int main(void) {
    printf("=== Pruebas de utilidades de directorio ===\n\n");

    int failures = 0;
    failures += test_recursive_listing();
    failures += test_empty_directory();
    failures += test_nonexistent_directory();

    printf("\n=== Resumen: %s ===\n", (failures == 0) ? "todas las pruebas pasaron" : "fallas detectadas");
    return failures == 0 ? 0 : 1;
}
