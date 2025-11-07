#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include "../include/archive.h"
#include "../include/file_manager.h"

/**
 * @brief Crea una estructura de directorios de prueba
 */
void create_test_directory_structure() {
    printf("Creando estructura de directorios de prueba...\n");
    
    // Directorio base
    create_directory("test/output/test_dir");
    
    // Subdirectorios y archivos
    create_directory("test/output/test_dir/subdir1");
    create_directory("test/output/test_dir/subdir2");
    
    // Archivos en diferentes niveles
    const char *file1_content = "Contenido del archivo 1";
    const char *file2_content = "Contenido del archivo 2 en subdir1";
    const char *file3_content = "Contenido del archivo 3 en subdir2";
    
    assert(write_file("test/output/test_dir/file1.txt", 
                     (const unsigned char*)file1_content, strlen(file1_content)) == 0);
    assert(write_file("test/output/test_dir/subdir1/file2.txt", 
                     (const unsigned char*)file2_content, strlen(file2_content)) == 0);
    assert(write_file("test/output/test_dir/subdir2/file3.txt", 
                     (const unsigned char*)file3_content, strlen(file3_content)) == 0);
    
    printf("✓ Estructura de directorios creada\n");
}

/**
 * @brief Prueba creación y serialización de archive
 */
void test_archive_creation() {
    printf("1. Prueba creación de archive:\n");
    
    archive_t *archive = create_archive_from_dir("test/output/test_dir");
    assert(archive != NULL);
    assert(archive->file_count == 3);
    
    printf("   ✓ Archive creado con %ld archivos\n", (long)archive->file_count);
    
    // Verificar que los archivos están en el archive
    int found_file1 = 0, found_file2 = 0, found_file3 = 0;
    for (size_t i = 0; i < archive->file_count; i++) {
        if (strstr(archive->files[i].path, "file1.txt")) found_file1 = 1;
        if (strstr(archive->files[i].path, "subdir1/file2.txt")) found_file2 = 1;
        if (strstr(archive->files[i].path, "subdir2/file3.txt")) found_file3 = 1;
    }
    assert(found_file1 && found_file2 && found_file3);
    printf("   ✓ Todos los archivos incluidos en el archive\n");
    
    // Serializar
    unsigned char *serialized = NULL;
    size_t serialized_size = 0;
    assert(serialize_archive(archive, &serialized, &serialized_size) == 0);
    assert(serialized != NULL);
    printf("   ✓ Archive serializado (%ld bytes)\n", (long)serialized_size);
    
    // Deserializar
    archive_t *archive2 = deserialize_archive(serialized, serialized_size);
    assert(archive2 != NULL);
    assert(archive2->file_count == archive->file_count);
    printf("   ✓ Archive deserializado correctamente\n");
    
    free(serialized);
    free_archive(archive);
    free_archive(archive2);
    
    printf("\n");
}

/**
 * @brief Prueba extracción de archive
 */
void test_archive_extraction() {
    printf("2. Prueba extracción de archive:\n");
    
    // Crear archive
    archive_t *archive = create_archive_from_dir("test/output/test_dir");
    assert(archive != NULL);
    
    // Extraer a nuevo directorio
    assert(extract_archive(archive, "test/output/extracted_dir") == 0);
    printf("   ✓ Archive extraído\n");
    
    // Verificar que los archivos existen
    struct stat st;
    assert(stat("test/output/extracted_dir/file1.txt", &st) == 0);
    assert(stat("test/output/extracted_dir/subdir1/file2.txt", &st) == 0);
    assert(stat("test/output/extracted_dir/subdir2/file3.txt", &st) == 0);
    printf("   ✓ Estructura de directorios recreada\n");
    
    // Verificar contenido
    unsigned char *content1 = NULL, *content2 = NULL, *content3 = NULL;
    size_t size1 = 0, size2 = 0, size3 = 0;
    
    assert(read_file("test/output/extracted_dir/file1.txt", &content1, &size1) == 0);
    assert(read_file("test/output/extracted_dir/subdir1/file2.txt", &content2, &size2) == 0);
    assert(read_file("test/output/extracted_dir/subdir2/file3.txt", &content3, &size3) == 0);
    
    assert(memcmp(content1, "Contenido del archivo 1", size1) == 0);
    assert(memcmp(content2, "Contenido del archivo 2 en subdir1", size2) == 0);
    assert(memcmp(content3, "Contenido del archivo 3 en subdir2", size3) == 0);
    printf("   ✓ Contenido de archivos preservado\n");
    
    free(content1);
    free(content2);
    free(content3);
    free_archive(archive);
    
    printf("\n");
}

/**
 * @brief Prueba integración completa con compresión y encriptación
 */
void test_full_archive_workflow() {
    printf("3. Prueba flujo completo archive + compresión + encriptación:\n");
    
    // Simular configuración
    program_config_t config = {0};
    strcpy(config.input_path, "test/output/test_dir");
    strcpy(config.output_path, "test/output/archive_compressed_encrypted.gsea");
    strcpy(config.key, "testkey123");
    config.operations = OP_COMPRESS | OP_ENCRYPT;
    config.comp_alg = COMP_ALG_RLE;
    config.enc_alg = ENC_ALG_VIGENERE;
    config.valid = 1;
    
    // Comprimir y encriptar directorio
    assert(compress_and_encrypt_directory(&config) == 0);
    printf("   ✓ Directorio comprimido y encriptado\n");
    
    // Cambiar configuración para descompresión
    strcpy(config.input_path, "test/output/archive_compressed_encrypted.gsea");
    strcpy(config.output_path, "test/output/restored_dir");
    config.operations = OP_DECRYPT | OP_DECOMPRESS;
    
    // Desencriptar y descomprimir
    assert(decrypt_and_decompress_directory(&config) == 0);
    printf("   ✓ Archive desencriptado y descomprimido\n");
    
    // Verificar que la estructura se restauró
    struct stat st;
    assert(stat("test/output/restored_dir/file1.txt", &st) == 0);
    assert(stat("test/output/restored_dir/subdir1/file2.txt", &st) == 0);
    assert(stat("test/output/restored_dir/subdir2/file3.txt", &st) == 0);
    printf("   ✓ Estructura de directorios completamente restaurada\n");
    
    printf("\n");
}

int main() {
    printf("=== GSEA - Pruebas del Sistema de Archives ===\n\n");
    
    // Crear directorios de prueba
    create_directory("test/output");
    
    // Crear estructura de prueba
    create_test_directory_structure();
    
    // Ejecutar pruebas
    test_archive_creation();
    test_archive_extraction();
    test_full_archive_workflow();
    
    printf("=== Todas las pruebas de archives completadas ===\n");
    return 0;
}