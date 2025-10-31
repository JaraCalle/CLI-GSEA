#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <time.h>
#include "../include/file_manager.h"
#include "../include/dir_utils.h"

/**
 * @brief Crea archivos de prueba para testing de concurrencia
 */
void create_test_files(const char *dir_path, int num_files) {
    printf("Creando %d archivos de prueba en '%s'...\n", num_files, dir_path);
    
    create_directory(dir_path);
    
    for (int i = 0; i < num_files; i++) {
        char file_path[256];
        snprintf(file_path, sizeof(file_path), "%s/test_file_%d.txt", dir_path, i);
        
        // Crear contenido con algún patrón para compresión
        char content[1024];
        snprintf(content, sizeof(content), 
                 "Archivo de prueba %d. Contenido repetitivo: %s\n", 
                 i, "AAAAABBBBBCCCCCDDDDDEEEEE");
        
        assert(write_file(file_path, (const unsigned char *)content, strlen(content)) == 0);
    }
    printf("✓ Archivos de prueba creados\n");
}

/**
 * @brief Prueba básica de lectura de directorios
 */
void test_directory_reading() {
    printf("1. Prueba lectura de directorios:\n");
    
    const char *test_dir = "test/output/concurrency_test";
    create_test_files(test_dir, 5);
    
    FileList file_list = {0};
    read_directory_recursive(test_dir, &file_list);
    
    assert(file_list.count == 5);
    printf("   ✓ Correctamente leídos %d archivos\n", file_list.count);
    
    // Liberar memoria
    for (int i = 0; i < file_list.count; i++) {
        free(file_list.paths[i]);
    }
    free(file_list.paths);
    
    printf("\n");
}

/**
 * @brief Prueba comparativa de rendimiento secuencial vs concurrente
 */
void test_performance_comparison() {
    printf("2. Prueba comparativa de rendimiento:\n");
    
    const char *input_dir = "test/output/performance_input";
    const char *output_dir_seq = "test/output/performance_output_seq";
    const char *output_dir_conc = "test/output/performance_output_conc";
    
    // Crear archivos de prueba
    int num_files = 10;
    create_test_files(input_dir, num_files);
    
    printf("   Procesando %d archivos...\n", num_files);
    
    // TODO: Aquí iría la comparación real de tiempo
    // Por ahora solo mostramos la estructura
    printf("   ✓ Estructura de prueba preparada\n");
    printf("   Nota: La comparación de rendimiento requiere implementación completa\n");
    
    printf("\n");
}

/**
 * @brief Prueba manejo de errores en modo concurrente
 */
void test_concurrent_error_handling() {
    printf("3. Prueba manejo de errores en concurrencia:\n");
    
    const char *test_dir = "test/output/error_test";
    create_directory(test_dir);
    
    // Crear algunos archivos válidos y algunos problemáticos
    char *valid_files[] = {
        "test/output/error_test/valid1.txt",
        "test/output/error_test/valid2.txt",
        NULL
    };
    
    char *invalid_files[] = {
        "test/output/error_test/nonexistent1.txt", // No existe
        "test/output/error_test/nonexistent2.txt", // No existe  
        NULL
    };
    
    // Crear archivos válidos
    for (int i = 0; valid_files[i] != NULL; i++) {
        const char *content = "Contenido válido para prueba";
        assert(write_file(valid_files[i], (const unsigned char *)content, strlen(content)) == 0);
    }
    
    printf("   ✓ Escenario de prueba de errores preparado\n");
    printf("   Nota: Las pruebas completas requieren ejecución del programa principal\n");
    
    printf("\n");
}

/**
 * @brief Prueba con diferentes números de archivos
 */
void test_different_file_counts() {
    printf("4. Prueba con diferentes cantidades de archivos:\n");
    
    int test_counts[] = {1, 5, 10, 20};
    int num_tests = sizeof(test_counts) / sizeof(test_counts[0]);
    
    for (int i = 0; i < num_tests; i++) {
        char dir_path[256];
        snprintf(dir_path, sizeof(dir_path), "test/output/count_test_%d", test_counts[i]);
        
        create_test_files(dir_path, test_counts[i]);
        
        FileList file_list = {0};
        read_directory_recursive(dir_path, &file_list);
        
        assert(file_list.count == test_counts[i]);
        printf("   ✓ Correcto con %d archivos\n", test_counts[i]);
        
        // Liberar memoria
        for (int j = 0; j < file_list.count; j++) {
            free(file_list.paths[j]);
        }
        free(file_list.paths);
    }
    
    printf("\n");
}

int main() {
    printf("=== GSEA - Pruebas de Concurrencia ===\n\n");
    
    // Crear directorios de prueba
    create_directory("test/output");
    
    test_directory_reading();
    test_performance_comparison();
    test_concurrent_error_handling();
    test_different_file_counts();
    
    printf("=== Pruebas de concurrencia completadas ===\n");
    printf("Nota: Las pruebas de rendimiento real requieren ejecutar el programa completo\n");
    
    return 0;
}