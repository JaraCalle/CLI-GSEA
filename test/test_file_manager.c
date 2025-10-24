#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "../include/file_manager.h"

/**
 * @brief Prueba lectura y escritura de archivo de texto
 */
void test_text_file() {
    printf("1. Prueba archivo de texto:\n");
    
    const char* test_file = "test/output/test_text.txt";
    const char* text_data = "Hola, este es un archivo de prueba!\nLínea 2 del archivo.\n";
    size_t text_size = strlen(text_data);
    
    // Asegurar que existe el directorio de salida
    create_directory("test/output");
    
    if (write_file(test_file, (const unsigned char*)text_data, text_size) == 0) {
        printf("   ✓ Archivo escrito correctamente\n");
        
        unsigned char* read_buffer = NULL;
        size_t read_size = 0;
        
        if (read_file(test_file, &read_buffer, &read_size) == 0) {
            printf("   ✓ Archivo leído correctamente (%zu bytes)\n", read_size);
            
            if (read_size == text_size && memcmp(text_data, read_buffer, text_size) == 0) {
                printf("   ✓ Contenido verificado correctamente\n");
            } else {
                printf("   ✗ Error: Contenido no coincide\n");
            }
            
            free(read_buffer);
        } else {
            printf("   ✗ Error al leer archivo\n");
        }
    } else {
        printf("   ✗ Error al escribir archivo\n");
    }
    printf("\n");
}

/**
 * @brief Prueba archivo binario
 */
void test_binary_file() {
    printf("2. Prueba archivo binario:\n");
    
    const char* test_binary = "test/output/test_binary.dat";
    unsigned char binary_data[256];
    
    for (int i = 0; i < 256; i++) {
        binary_data[i] = (unsigned char)i;
    }
    
    if (write_file(test_binary, binary_data, sizeof(binary_data)) == 0) {
        printf("   ✓ Archivo binario escrito correctamente\n");
        
        unsigned char* read_binary = NULL;
        size_t read_binary_size = 0;
        
        if (read_file(test_binary, &read_binary, &read_binary_size) == 0) {
            printf("   ✓ Archivo binario leído correctamente (%zu bytes)\n", read_binary_size);
            
            if (read_binary_size == sizeof(binary_data) && 
                memcmp(binary_data, read_binary, sizeof(binary_data)) == 0) {
                printf("   ✓ Contenido binario verificado correctamente\n");
            } else {
                printf("   ✗ Error: Contenido binario no coincide\n");
            }
            
            free(read_binary);
        } else {
            printf("   ✗ Error al leer archivo binario\n");
        }
    } else {
        printf("   ✗ Error al escribir archivo binario\n");
    }
    printf("\n");
}

/**
 * @brief Prueba archivo grande (>1MB)
 */
void test_large_file() {
    printf("3. Prueba archivo grande (>1MB):\n");
    
    const char* test_large = "test/output/test_large.dat";
    size_t large_size = 2 * 1024 * 1024; // 2MB
    unsigned char* large_data = (unsigned char*)malloc(large_size);
    
    if (large_data == NULL) {
        printf("   ✗ Error: No se pudo asignar memoria para archivo grande\n");
        return;
    }
    
    // Llenar con datos de prueba
    for (size_t i = 0; i < large_size; i++) {
        large_data[i] = (unsigned char)(i % 256);
    }
    
    clock_t start = clock();
    if (write_file(test_large, large_data, large_size) == 0) {
        clock_t write_time = clock() - start;
        printf("   ✓ Archivo grande escrito correctamente (%.2f segundos)\n", 
               (double)write_time / CLOCKS_PER_SEC);
        
        unsigned char* read_large = NULL;
        size_t read_large_size = 0;
        
        start = clock();
        if (read_file(test_large, &read_large, &read_large_size) == 0) {
            clock_t read_time = clock() - start;
            printf("   ✓ Archivo grande leído correctamente (%.2f segundos)\n", 
                   (double)read_time / CLOCKS_PER_SEC);
            
            if (read_large_size == large_size) {
                printf("   ✓ Tamaño verificado correctamente (%zu bytes)\n", large_size);
                
                // Verificar algunos puntos del contenido
                int content_ok = 1;
                for (size_t i = 0; i < large_size; i += 100000) {
                    if (read_large[i] != (unsigned char)(i % 256)) {
                        content_ok = 0;
                        break;
                    }
                }
                
                if (content_ok) {
                    printf("   ✓ Contenido verificado correctamente\n");
                } else {
                    printf("   ✗ Error: Contenido no coincide\n");
                }
            } else {
                printf("   ✗ Error: Tamaño no coincide (%zu vs %zu)\n", read_large_size, large_size);
            }
            
            free(read_large);
        } else {
            printf("   ✗ Error al leer archivo grande\n");
        }
    } else {
        printf("   ✗ Error al escribir archivo grande\n");
    }
    
    free(large_data);
    printf("\n");
}

/**
 * @brief Prueba manejo de errores
 */
void test_error_handling() {
    printf("4. Prueba manejo de errores:\n");
    
    // Archivo inexistente
    unsigned char* buffer = NULL;
    size_t size = 0;
    if (read_file("archivo_inexistente.txt", &buffer, &size) == -1) {
        printf("   ✓ Correctamente detectado archivo inexistente\n");
    }
    
    // Directorio en lugar de archivo
    if (read_file("test", &buffer, &size) == -1) {
        printf("   ✓ Correctamente detectado directorio como archivo inválido\n");
    }
    
    // Parámetros NULL
    if (read_file(NULL, &buffer, &size) == -1) {
        printf("   ✓ Correctamente manejados parámetros NULL\n");
    }
    
    printf("\n");
}

/**
 * @brief Prueba funciones de directorio
 */
void test_directory_operations() {
    printf("5. Prueba operaciones de directorio:\n");
    
    const char* test_dir = "test/output/nested/dir/structure";
    
    if (create_directory(test_dir) == 0) {
        printf("   ✓ Directorios anidados creados correctamente\n");
        
        if (is_directory(test_dir)) {
            printf("   ✓ Correctamente identificado como directorio\n");
        } else {
            printf("   ✗ Error: No se identifica como directorio\n");
        }
    } else {
        printf("   ✗ Error al crear directorios anidados\n");
    }
    
    printf("\n");
}

/**
 * @brief Función principal de pruebas del file manager
 */
int main() {
    printf("=== GSEA - Pruebas del Gestor de Archivos POSIX ===\n\n");
    
    // Crear directorio de test si no existe
    create_directory("test/output");
    
    test_text_file();
    test_binary_file();
    test_large_file();
    test_error_handling();
    test_directory_operations();
    
    printf("=== Pruebas completadas ===\n");
    return 0;
}