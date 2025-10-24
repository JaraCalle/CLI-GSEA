#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/args_parser.h"
#include "../include/file_manager.h"

/**
 * @brief Simula la ejecución de operaciones (sin main)
 */
int simulate_operations(const program_config_t *config) {
    printf("Simulando operaciones...\n");
    
    if (!config->valid) {
        fprintf(stderr, "Error: Configuración inválida\n");
        return -1;
    }
    
    // Leer archivo de entrada
    unsigned char *input_data = NULL;
    size_t input_size = 0;
    
    printf("Leyendo archivo de entrada: %s\n", config->input_path);
    if (read_file(config->input_path, &input_data, &input_size) != 0) {
        fprintf(stderr, "Error: No se pudo leer el archivo de entrada\n");
        return -1;
    }
    
    printf("Archivo leído correctamente (%zu bytes)\n", input_size);
    
    // Por ahora solo copiamos los datos (simulación)
    unsigned char *output_data = input_data;
    size_t output_size = input_size;
    
    // Escribir archivo de salida
    printf("Escribiendo archivo de salida: %s\n", config->output_path);
    if (write_file(config->output_path, output_data, output_size) != 0) {
        fprintf(stderr, "Error: No se pudo escribir el archivo de salida\n");
        free(input_data);
        return -1;
    }
    
    printf("Archivo escrito correctamente (%zu bytes)\n", output_size);
    
    // Liberar memoria
    free(input_data);
    
    printf("Operaciones simuladas exitosamente\n");
    return 0;
}

/**
 * @brief Prueba integración básica: parser + file manager
 */
void test_integration_basic() {
    printf("=== Prueba de Integración Básica ===\n\n");
    
    // Crear archivo de prueba
    const char* test_input = "test/output/integration_test.txt";
    const char* test_output = "test/output/integration_result.txt";
    const char* test_data = "Datos de prueba para integración";
    
    if (write_file(test_input, (const unsigned char*)test_data, strlen(test_data)) != 0) {
        printf("✗ Error creando archivo de prueba\n");
        return;
    }
    
    // Simular argumentos de línea de comandos COMPLETOS
    char *argv[] = {
        "./gsea",
        "-c",
        "--comp-alg", "rle", 
        "-i", "test/output/integration_test.txt",
        "-o", "test/output/integration_result.txt",
        NULL
    };
    int argc = 8;
    
    program_config_t config;
    if (parse_arguments(argc, argv, &config) == 0) {
        printf("✓ Parser configurado correctamente\n");
        
        // Verificar configuración
        if (config.operations & OP_COMPRESS) {
            printf("✓ Operación de compresión detectada\n");
        }
        
        if (strcmp(config.input_path, test_input) == 0) {
            printf("✓ Ruta de entrada configurada correctamente\n");
        }
        
        if (strcmp(config.output_path, test_output) == 0) {
            printf("✓ Ruta de salida configurada correctamente\n");
        }
        
        // Simular procesamiento
        if (simulate_operations(&config) == 0) {
            printf("✓ Integración básica completada exitosamente\n");
        } else {
            printf("✗ Error en la simulación de operaciones\n");
        }
    } else {
        printf("✗ Error en el parser\n");
    }
    
    printf("\n");
}

/**
 * @brief Prueba manejo de errores integrado
 */
void test_integration_errors() {
    printf("=== Prueba de Manejo de Errores Integrado ===\n\n");
    
    // Test 1: Archivo de entrada inexistente
    char *argv1[] = {
        "./gsea",
        "-c", "--comp-alg", "rle",
        "-i", "archivo_que_no_existe.txt",
        "-o", "salida.txt",
        NULL
    };
    
    program_config_t config;
    if (parse_arguments(7, argv1, &config) == -1) {
        printf("✓ Correctamente rechazado archivo inexistente\n");
    } else {
        printf("✗ Debería rechazar archivo inexistente\n");
    }
    
    // Test 2: Sin operaciones
    char *argv2[] = {
        "./gsea",
        "-i", "input.txt",
        "-o", "output.txt",
        NULL
    };
    
    if (parse_arguments(5, argv2, &config) == -1) {
        printf("✓ Correctamente rechazado sin operaciones\n");
    } else {
        printf("✗ Debería rechazar sin operaciones\n");
    }
    
    printf("\n");
}

int main() {
    printf("=== GSEA - Pruebas de Integración ===\n\n");
    
    // Crear directorio de salida
    create_directory("test/output");
    
    test_integration_basic();
    test_integration_errors();
    
    printf("=== Pruebas de integración completadas ===\n");
    return 0;
}