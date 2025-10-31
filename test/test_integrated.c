#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include "../include/file_manager.h"

/**
 * @brief Prueba flujo completo: compresión + encriptación
 */
void test_full_compress_encrypt_flow() {
    printf("1. Prueba flujo completo: COMPRIMIR → ENCRIPTAR\n");
    
    const char *input_file = "test/output/full_input.txt";
    const char *compressed_encrypted = "test/output/full_compressed_encrypted.gsea";
    const char *output_file = "test/output/full_output.txt";
    
    // Crear archivo de entrada con datos repetitivos (buenos para RLE)
    const char *test_data = "AAAAABBBBBCCCCCDDDDDEEEEEFFFFFGGGGGHHHHHIIIIIJJJJJ";
    size_t data_size = strlen(test_data);
    
    assert(write_file(input_file, (const unsigned char *)test_data, data_size) == 0);
    printf("   ✓ Archivo de entrada creado\n");
    
    // Simular comando: ./gsea -ce --comp-alg rle --enc-alg vigenere -i input -o output -k clave
    char command[512];
    snprintf(command, sizeof(command),
             "./gsea -ce --comp-alg rle --enc-alg vigenere -i %s -o %s -k 'MiClaveSecreta123'",
             input_file, compressed_encrypted);
    
    printf("   Ejecutando: %s\n", command);
    int result = system(command);
    
    if (result == 0) {
        printf("   ✓ Compresión + encriptación exitosa\n");
        
        // Verificar que el archivo resultante existe y es diferente
        struct stat st;
        assert(stat(compressed_encrypted, &st) == 0);
        printf("   ✓ Archivo comprimido/encriptado creado (%ld bytes)\n", st.st_size);
        
        // Ahora desencriptar y descomprimir
        snprintf(command, sizeof(command),
                 "./gsea -du --comp-alg rle --enc-alg vigenere -i %s -o %s -k 'MiClaveSecreta123'",
                 compressed_encrypted, output_file);
        
        printf("   Ejecutando: %s\n", command);
        result = system(command);
        
        if (result == 0) {
            printf("   ✓ Desencriptación + descompresión exitosa\n");
            
            // Verificar que el archivo final existe
            assert(stat(output_file, &st) == 0);
            printf("   ✓ Archivo final creado (%ld bytes)\n", st.st_size);
            
            // Leer y comparar con el original
            unsigned char *final_data = NULL;
            size_t final_size = 0;
            assert(read_file(output_file, &final_data, &final_size) == 0);
            
            assert(final_size == data_size);
            assert(memcmp(test_data, final_data, data_size) == 0);
            printf("   ✓ Archivo final es idéntico al original\n");
            
            free(final_data);
        } else {
            printf("   ✗ Error en desencriptación + descompresión\n");
        }
    } else {
        printf("   ✗ Error en compresión + encriptación\n");
    }
    
    printf("\n");
}

/**
 * @brief Prueba flujo completo solo con compresión
 */
void test_compress_only_flow() {
    printf("2. Prueba flujo solo compresión:\n");
    
    const char *input_file = "test/output/compress_input.txt";
    const char *compressed_file = "test/output/compressed.rle";
    const char *output_file = "test/output/compress_output.txt";
    
    // Crear archivo de entrada
    const char *test_data = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    size_t data_size = strlen(test_data);
    
    assert(write_file(input_file, (const unsigned char *)test_data, data_size) == 0);
    
    // Comprimir
    char command[512];
    snprintf(command, sizeof(command),
             "./gsea -c --comp-alg rle -i %s -o %s",
             input_file, compressed_file);
    
    printf("   Ejecutando: %s\n", command);
    int result = system(command);
    
    if (result == 0) {
        printf("   ✓ Compresión exitosa\n");
        
        // Descomprimir
        snprintf(command, sizeof(command),
                 "./gsea -d --comp-alg rle -i %s -o %s",
                 compressed_file, output_file);
        
        printf("   Ejecutando: %s\n", command);
        result = system(command);
        
        if (result == 0) {
            printf("   ✓ Descompresión exitosa\n");
            
            // Verificar igualdad
            unsigned char *final_data = NULL;
            size_t final_size = 0;
            assert(read_file(output_file, &final_data, &final_size) == 0);
            
            assert(final_size == data_size);
            assert(memcmp(test_data, final_data, data_size) == 0);
            printf("   ✓ Archivo restaurado correctamente\n");
            
            free(final_data);
        }
    }
    
    printf("\n");
}

/**
 * @brief Prueba flujo completo solo con encriptación
 */
void test_encrypt_only_flow() {
    printf("3. Prueba flujo solo encriptación:\n");
    
    const char *input_file = "test/output/encrypt_input.txt";
    const char *encrypted_file = "test/output/encrypted.dat";
    const char *output_file = "test/output/encrypt_output.txt";
    
    // Crear archivo de entrada
    const char *test_data = "Texto secreto que debe ser encriptado";
    size_t data_size = strlen(test_data);
    
    assert(write_file(input_file, (const unsigned char *)test_data, data_size) == 0);
    
    // Encriptar
    char command[512];
    snprintf(command, sizeof(command),
             "./gsea -e --enc-alg vigenere -i %s -o %s -k 'ClaveSuperSecreta'",
             input_file, encrypted_file);
    
    printf("   Ejecutando: %s\n", command);
    int result = system(command);
    
    if (result == 0) {
        printf("   ✓ Encriptación exitosa\n");
        
        // Desencriptar
        snprintf(command, sizeof(command),
                 "./gsea -u --enc-alg vigenere -i %s -o %s -k 'ClaveSuperSecreta'",
                 encrypted_file, output_file);
        
        printf("   Ejecutando: %s\n", command);
        result = system(command);
        
        if (result == 0) {
            printf("   ✓ Desencriptación exitosa\n");
            
            // Verificar igualdad
            unsigned char *final_data = NULL;
            size_t final_size = 0;
            assert(read_file(output_file, &final_data, &final_size) == 0);
            
            assert(final_size == data_size);
            assert(memcmp(test_data, final_data, data_size) == 0);
            printf("   ✓ Archivo restaurado correctamente\n");
            
            free(final_data);
        }
    }
    
    printf("\n");
}

/**
 * @brief Prueba manejo de errores en flujo completo
 */
void test_error_handling_flow() {
    printf("4. Prueba manejo de errores en flujo completo:\n");
    
    // Prueba con archivo inexistente
    printf("   Probando archivo inexistente...\n");
    int result = system("./gsea -c --comp-alg rle -i archivo_inexistente.txt -o salida.rle 2>/dev/null");
    assert(result != 0);
    printf("   ✓ Correctamente rechazado archivo inexistente\n");
    
    // Prueba sin clave para encriptación
    printf("   Probando encriptación sin clave...\n");
    result = system("./gsea -e --enc-alg vigenere -i test/output/encrypt_input.txt -o salida.dat 2>/dev/null");
    assert(result != 0);
    printf("   ✓ Correctamente rechazada encriptación sin clave\n");
    
    // Prueba con operaciones contradictorias
    printf("   Probando operaciones contradictorias...\n");
    result = system("./gsea -cd --comp-alg rle -i test/output/encrypt_input.txt -o salida.rle 2>/dev/null");
    assert(result != 0);
    printf("   ✓ Correctamente rechazadas operaciones contradictorias\n");
    
    printf("\n");
}

/**
 * @brief Prueba con archivo binario grande
 */
void test_large_binary_file_flow() {
    printf("5. Prueba con archivo binario grande:\n");
    
    const char *input_file = "test/output/large_input.bin";
    const char *processed_file = "test/output/large_processed.gsea";
    const char *output_file = "test/output/large_output.bin";
    
    // Crear archivo binario de 1MB
    size_t file_size = 1024 * 1024; // 1MB
    unsigned char *test_data = (unsigned char *)malloc(file_size);
    for (size_t i = 0; i < file_size; i++) {
        test_data[i] = (unsigned char)((i * 7) % 256); // Patrón pseudo-aleatorio
    }
    
    assert(write_file(input_file, test_data, file_size) == 0);
    printf("   ✓ Archivo binario grande creado (%zu bytes)\n", file_size);
    
    // Procesar: comprimir + encriptar
    char command[512];
    snprintf(command, sizeof(command),
             "./gsea -ce --comp-alg rle --enc-alg vigenere -i %s -o %s -k 'ClaveGrande'",
             input_file, processed_file);
    
    printf("   Ejecutando compresión + encriptación...\n");
    int result = system(command);
    
    if (result == 0) {
        printf("   ✓ Procesamiento de archivo grande exitoso\n");
        
        // Recuperar archivo
        snprintf(command, sizeof(command),
                 "./gsea -du --comp-alg rle --enc-alg vigenere -i %s -o %s -k 'ClaveGrande'",
                 processed_file, output_file);
        
        printf("   Ejecutando desencriptación + descompresión...\n");
        result = system(command);
        
        if (result == 0) {
            printf("   ✓ Recuperación de archivo grande exitosa\n");
            
            // Verificar igualdad
            unsigned char *final_data = NULL;
            size_t final_size = 0;
            assert(read_file(output_file, &final_data, &final_size) == 0);
            
            assert(final_size == file_size);
            assert(memcmp(test_data, final_data, file_size) == 0);
            printf("   ✓ Archivo grande restaurado correctamente\n");
            
            free(final_data);
        }
    }
    
    free(test_data);
    printf("\n");
}

int main() {
    printf("=== GSEA - Pruebas de Integración Completa ===\n\n");
    
    // Crear directorio de salida
    create_directory("test/output");
    
    // Compilar el programa primero
    printf("Compilando GSEA...\n");
    int compile_result = system("make clean > /dev/null && make > /dev/null");
    if (compile_result != 0) {
        fprintf(stderr, "Error: No se pudo compilar GSEA\n");
        return 1;
    }
    printf("✓ GSEA compilado correctamente\n\n");
    
    test_full_compress_encrypt_flow();
    test_compress_only_flow();
    test_encrypt_only_flow();
    test_error_handling_flow();
    test_large_binary_file_flow();
    
    printf("=== Todas las pruebas de integración completadas ===\n");
    return 0;
}