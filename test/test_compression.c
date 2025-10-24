#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/compression.h"
#include "../include/file_manager.h"

/**
 * @brief Prueba compresión RLE con datos altamente repetitivos
 */
void test_rle_highly_repetitive() {
    printf("1. Prueba RLE con datos altamente repetitivos:\n");
    
    // Datos muy repetitivos: 100 'A', 100 'B', 100 'C'
    size_t data_size = 300;
    unsigned char *test_data = (unsigned char *)malloc(data_size);
    
    for (size_t i = 0; i < 100; i++) test_data[i] = 'A';
    for (size_t i = 100; i < 200; i++) test_data[i] = 'B';
    for (size_t i = 200; i < 300; i++) test_data[i] = 'C';
    
    // Comprimir
    compression_result_t compressed = compress_rle(test_data, data_size);
    assert(compressed.error == 0);
    assert(compressed.data != NULL);
    
    printf("   ✓ Compresión exitosa\n");
    printf("   Tamaño original: %zu, comprimido: %zu\n", data_size, compressed.size);
    
    // Verificar ratio de compresión (debería ser muy bueno)
    double ratio = compression_ratio(data_size, compressed.size);
    printf("   Ratio de compresión: %.2f\n", ratio);
    assert(ratio < 0.1); // Muy comprimible
    
    // Descomprimir
    compression_result_t decompressed = decompress_rle(compressed.data, compressed.size);
    assert(decompressed.error == 0);
    assert(decompressed.data != NULL);
    
    printf("   ✓ Descompresión exitosa\n");
    
    // Verificar que los datos son idénticos
    assert(decompressed.size == data_size);
    assert(memcmp(test_data, decompressed.data, data_size) == 0);
    printf("   ✓ Datos restaurados correctamente\n");
    
    // Limpiar
    free(test_data);
    free_compression_result(&compressed);
    free_compression_result(&decompressed);
    
    printf("\n");
}

/**
 * @brief Prueba RLE con datos no repetitivos
 */
void test_rle_non_repetitive() {
    printf("2. Prueba RLE con datos no repetitivos:\n");
    
    // Datos no repetitivos (secuencia incremental)
    size_t data_size = 256;
    unsigned char *test_data = (unsigned char *)malloc(data_size);
    for (size_t i = 0; i < data_size; i++) {
        test_data[i] = (unsigned char)i;
    }
    
    // Comprimir
    compression_result_t compressed = compress_rle(test_data, data_size);
    assert(compressed.error == 0);
    assert(compressed.data != NULL);
    
    printf("   ✓ Compresión exitosa\n");
    printf("   Tamaño original: %zu, comprimido: %zu\n", data_size, compressed.size);
    
    // En datos no repetitivos, RLE puede aumentar el tamaño
    double ratio = compression_ratio(data_size, compressed.size);
    printf("   Ratio de compresión: %.2f\n", ratio);
    
    // Descomprimir
    compression_result_t decompressed = decompress_rle(compressed.data, compressed.size);
    assert(decompressed.error == 0);
    assert(decompressed.data != NULL);
    
    printf("   ✓ Descompresión exitosa\n");
    
    // Verificar que los datos son idénticos
    assert(decompressed.size == data_size);
    assert(memcmp(test_data, decompressed.data, data_size) == 0);
    printf("   ✓ Datos restaurados correctamente\n");
    
    // Limpiar
    free(test_data);
    free_compression_result(&compressed);
    free_compression_result(&decompressed);
    
    printf("\n");
}

/**
 * @brief Prueba RLE con secuencias cortas y largas
 */
void test_rle_mixed_sequences() {
    printf("3. Prueba RLE con secuencias mixtas:\n");
    
    // Mezcla de secuencias cortas y largas
    unsigned char test_data[] = {
        'A', 'A', 'A',          // 3 A's
        'B',                    // 1 B
        'C', 'C', 'C', 'C', 'C', // 5 C's  
        'D', 'D',               // 2 D's
        'E', 'E', 'E', 'E', 'E', 'E', 'E', 'E', 'E', 'E' // 10 E's
    };
    size_t data_size = sizeof(test_data);
    
    // Comprimir
    compression_result_t compressed = compress_rle(test_data, data_size);
    assert(compressed.error == 0);
    assert(compressed.data != NULL);
    
    printf("   ✓ Compresión exitosa\n");
    printf("   Tamaño original: %zu, comprimido: %zu\n", data_size, compressed.size);
    
    // Verificar formato: debería tener 5 pares <count><byte>
    assert(compressed.size == 10); // 5 pares * 2 bytes
    
    // Descomprimir
    compression_result_t decompressed = decompress_rle(compressed.data, compressed.size);
    assert(decompressed.error == 0);
    assert(decompressed.data != NULL);
    
    printf("   ✓ Descompresión exitosa\n");
    
    // Verificar que los datos son idénticos
    assert(decompressed.size == data_size);
    assert(memcmp(test_data, decompressed.data, data_size) == 0);
    printf("   ✓ Datos restaurados correctamente\n");
    
    // Limpiar
    free_compression_result(&compressed);
    free_compression_result(&decompressed);
    
    printf("\n");
}

/**
 * @brief Prueba RLE con secuencias de longitud máxima
 */
void test_rle_max_run_length() {
    printf("4. Prueba RLE con secuencias de longitud máxima:\n");
    
    // Crear secuencia de 255 bytes iguales (máximo permitido)
    size_t data_size = 255;
    unsigned char *test_data = (unsigned char *)malloc(data_size);
    memset(test_data, 'X', data_size);
    
    // Comprimir
    compression_result_t compressed = compress_rle(test_data, data_size);
    assert(compressed.error == 0);
    assert(compressed.data != NULL);
    
    printf("   ✓ Compresión exitosa\n");
    printf("   Tamaño original: %zu, comprimido: %zu\n", data_size, compressed.size);
    
    // Debería comprimirse a solo 2 bytes
    assert(compressed.size == 2);
    
    // Verificar el contenido comprimido
    assert(compressed.data[0] == 255); // count
    assert(compressed.data[1] == 'X'); // byte
    
    // Descomprimir
    compression_result_t decompressed = decompress_rle(compressed.data, compressed.size);
    assert(decompressed.error == 0);
    assert(decompressed.data != NULL);
    
    printf("   ✓ Descompresión exitosa\n");
    
    // Verificar que los datos son idénticos
    assert(decompressed.size == data_size);
    assert(memcmp(test_data, decompressed.data, data_size) == 0);
    printf("   ✓ Datos restaurados correctamente\n");
    
    // Limpiar
    free(test_data);
    free_compression_result(&compressed);
    free_compression_result(&decompressed);
    
    printf("\n");
}

/**
 * @brief Prueba manejo de errores en RLE
 */
void test_rle_error_handling() {
    printf("5. Prueba manejo de errores en RLE:\n");
    
    // Prueba con datos NULL
    compression_result_t result1 = compress_rle(NULL, 10);
    assert(result1.error != 0);
    printf("   ✓ Manejo correcto de datos NULL\n");
    
    // Prueba con tamaño 0
    unsigned char data[] = {1, 2, 3};
    compression_result_t result2 = compress_rle(data, 0);
    assert(result2.error != 0);
    printf("   ✓ Manejo correcto de tamaño 0\n");
    
    // Prueba descompresión con datos corruptos (tamaño impar)
    unsigned char corrupt_data[] = {1, 2, 3}; // Tamaño impar
    compression_result_t result3 = decompress_rle(corrupt_data, 3);
    assert(result3.error != 0);
    printf("   ✓ Manejo correcto de datos corruptos\n");
    
    printf("\n");
}

/**
 * @brief Prueba integración RLE con archivos reales
 */
void test_rle_file_integration() {
    printf("6. Prueba integración RLE con archivos:\n");
    
    const char *test_file = "test/output/rle_test.bin";
    const char *compressed_file = "test/output/rle_compressed.rle";
    const char *decompressed_file = "test/output/rle_decompressed.bin";
    
    // Crear archivo de prueba con datos repetitivos
    size_t file_size = 1000;
    unsigned char *original_data = (unsigned char *)malloc(file_size);
    for (size_t i = 0; i < file_size; i++) {
        original_data[i] = (unsigned char)((i / 100) + 'A'); // Bloques de 100 bytes iguales
    }
    
    // Escribir archivo original
    assert(write_file(test_file, original_data, file_size) == 0);
    printf("   ✓ Archivo original creado\n");
    
    // Comprimir
    compression_result_t compressed = compress_rle(original_data, file_size);
    assert(compressed.error == 0);
    
    // Escribir archivo comprimido
    assert(write_file(compressed_file, compressed.data, compressed.size) == 0);
    printf("   ✓ Archivo comprimido creado\n");
    
    // Leer archivo comprimido
    unsigned char *compressed_from_file = NULL;
    size_t compressed_size = 0;
    assert(read_file(compressed_file, &compressed_from_file, &compressed_size) == 0);
    
    // Descomprimir
    compression_result_t decompressed = decompress_rle(compressed_from_file, compressed_size);
    assert(decompressed.error == 0);
    
    // Escribir archivo descomprimido
    assert(write_file(decompressed_file, decompressed.data, decompressed.size) == 0);
    printf("   ✓ Archivo descomprimido creado\n");
    
    // Verificar que el archivo descomprimido es idéntico al original
    unsigned char *decompressed_from_file = NULL;
    size_t decompressed_size = 0;
    assert(read_file(decompressed_file, &decompressed_from_file, &decompressed_size) == 0);
    
    assert(decompressed_size == file_size);
    assert(memcmp(original_data, decompressed_from_file, file_size) == 0);
    printf("   ✓ Archivo descomprimido es idéntico al original\n");
    
    // Mostrar estadísticas
    double ratio = compression_ratio(file_size, compressed.size);
    printf("   Estadísticas: Original=%zu, Comprimido=%zu, Ratio=%.2f\n", 
           file_size, compressed.size, ratio);
    
    // Limpiar
    free(original_data);
    free(compressed_from_file);
    free(decompressed_from_file);
    free_compression_result(&compressed);
    free_compression_result(&decompressed);
    
    printf("\n");
}

int main() {
    printf("=== GSEA - Pruebas del Algoritmo RLE ===\n\n");
    
    // Crear directorio de salida
    create_directory("test/output");
    
    test_rle_highly_repetitive();
    test_rle_non_repetitive();
    test_rle_mixed_sequences();
    test_rle_max_run_length();
    test_rle_error_handling();
    test_rle_file_integration();
    
    printf("=== Todas las pruebas RLE completadas ===\n");
    return 0;
}