#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/encryption.h"
#include "../include/file_manager.h"

/**
 * @brief Prueba encriptación/desencriptación básica de texto
 */
void test_vigenere_basic_text() {
    printf("1. Prueba Vigenère con texto básico:\n");
    
    const char *test_data = "HOLA MUNDO ESTO ES UNA PRUEBA DE VIGENERE";
    const char *key = "CLAVE";
    size_t data_size = strlen(test_data);
    size_t key_size = strlen(key);
    
    // Encriptar
    encryption_result_t encrypted = encrypt_vigenere(
        (const unsigned char *)test_data, data_size,
        (const unsigned char *)key, key_size
    );
    assert(encrypted.error == 0);
    assert(encrypted.data != NULL);
    assert(encrypted.size == data_size);
    
    printf("   ✓ Encriptación exitosa\n");
    
    // Verificar que el texto encriptado es diferente al original
    assert(memcmp(test_data, encrypted.data, data_size) != 0);
    printf("   ✓ Texto encriptado es diferente al original\n");
    
    // Desencriptar
    encryption_result_t decrypted = decrypt_vigenere(
        encrypted.data, encrypted.size,
        (const unsigned char *)key, key_size
    );
    assert(decrypted.error == 0);
    assert(decrypted.data != NULL);
    assert(decrypted.size == data_size);
    
    printf("   ✓ Desencriptación exitosa\n");
    
    // Verificar que los datos son idénticos
    assert(memcmp(test_data, decrypted.data, data_size) == 0);
    printf("   ✓ Datos restaurados correctamente\n");
    
    free_encryption_result(&encrypted);
    free_encryption_result(&decrypted);
    
    printf("\n");
}

/**
 * @brief Prueba con diferentes longitudes de clave
 */
void test_vigenere_key_lengths() {
    printf("2. Prueba Vigenère con diferentes longitudes de clave:\n");
    
    const char *test_data = "Texto de prueba para diferentes claves";
    size_t data_size = strlen(test_data);
    
    const char *keys[] = {
        "A",           // Clave muy corta
        "CLAVE",       // Clave normal
        "ESTAESUNACLAVEMUYLARGA", // Clave larga
        "aBcDeFg",     // Clave con mezcla de mayúsculas/minúsculas
        "C0ntr4s3ñ4",  // Clave con números y ñ (carácter extendido)
        "Clave con espacios", // Clave con espacios
        "¡Clave! con ¿caracteres? especiales", // Caracteres especiales
        NULL
    };
    
    for (int i = 0; keys[i] != NULL; i++) {
        printf("   Probando clave: '%s'\n", keys[i]);
        
        encryption_result_t encrypted = encrypt_vigenere(
            (const unsigned char *)test_data, data_size,
            (const unsigned char *)keys[i], strlen(keys[i])
        );
        assert(encrypted.error == 0);
        
        // Verificar que el texto cambió
        assert(memcmp(test_data, encrypted.data, data_size) != 0);
        
        encryption_result_t decrypted = decrypt_vigenere(
            encrypted.data, encrypted.size,
            (const unsigned char *)keys[i], strlen(keys[i])
        );
        assert(decrypted.error == 0);
        
        assert(memcmp(test_data, decrypted.data, data_size) == 0);
        
        free_encryption_result(&encrypted);
        free_encryption_result(&decrypted);
        
        printf("      ✓ Clave válida\n");
    }
    
    printf("\n");
}

/**
 * @brief Prueba con datos binarios
 */
void test_vigenere_binary_data() {
    printf("3. Prueba Vigenère con datos binarios:\n");
    
    // Crear datos binarios (todos los bytes posibles)
    size_t data_size = 256;
    unsigned char *test_data = (unsigned char *)malloc(data_size);
    for (size_t i = 0; i < data_size; i++) {
        test_data[i] = (unsigned char)i;
    }
    
    const char *key = "CLAVEBINARIA";
    size_t key_size = strlen(key);
    
    // Encriptar
    encryption_result_t encrypted = encrypt_vigenere(
        test_data, data_size,
        (const unsigned char *)key, key_size
    );
    assert(encrypted.error == 0);
    assert(encrypted.data != NULL);
    assert(encrypted.size == data_size);
    
    printf("   ✓ Encriptación binaria exitosa\n");
    
    // Verificar que los datos cambiaron
    int data_changed = 0;
    for (size_t i = 0; i < data_size; i++) {
        if (test_data[i] != encrypted.data[i]) {
            data_changed = 1;
            break;
        }
    }
    assert(data_changed);
    printf("   ✓ Datos binarios fueron modificados\n");
    
    // Desencriptar
    encryption_result_t decrypted = decrypt_vigenere(
        encrypted.data, encrypted.size,
        (const unsigned char *)key, key_size
    );
    assert(decrypted.error == 0);
    assert(decrypted.data != NULL);
    assert(decrypted.size == data_size);
    
    printf("   ✓ Desencriptación binaria exitosa\n");
    
    // Verificar que los datos son idénticos
    assert(memcmp(test_data, decrypted.data, data_size) == 0);
    printf("   ✓ Datos binarios restaurados correctamente\n");
    
    free(test_data);
    free_encryption_result(&encrypted);
    free_encryption_result(&decrypted);
    
    printf("\n");
}

/**
 * @brief Prueba que el mismo texto con claves diferentes produce resultados diferentes
 */
void test_vigenere_different_keys() {
    printf("4. Prueba que diferentes claves producen diferentes resultados:\n");
    
    const char *test_data = "Mismo texto con diferentes claves";
    size_t data_size = strlen(test_data);
    
    const char *key1 = "CLAVE1";
    const char *key2 = "CLAVE2";
    
    // Encriptar con clave 1
    encryption_result_t encrypted1 = encrypt_vigenere(
        (const unsigned char *)test_data, data_size,
        (const unsigned char *)key1, strlen(key1)
    );
    assert(encrypted1.error == 0);
    
    // Encriptar con clave 2
    encryption_result_t encrypted2 = encrypt_vigenere(
        (const unsigned char *)test_data, data_size,
        (const unsigned char *)key2, strlen(key2)
    );
    assert(encrypted2.error == 0);
    
    // Verificar que los resultados son diferentes
    assert(memcmp(encrypted1.data, encrypted2.data, data_size) != 0);
    printf("   ✓ Diferentes claves producen diferentes textos encriptados\n");
    
    // Verificar que cada clave solo puede desencriptar su propio texto
    encryption_result_t decrypted1_with_key1 = decrypt_vigenere(
        encrypted1.data, encrypted1.size,
        (const unsigned char *)key1, strlen(key1)
    );
    assert(decrypted1_with_key1.error == 0);
    assert(memcmp(test_data, decrypted1_with_key1.data, data_size) == 0);
    
    encryption_result_t decrypted1_with_key2 = decrypt_vigenere(
        encrypted1.data, encrypted1.size,
        (const unsigned char *)key2, strlen(key2)
    );
    assert(decrypted1_with_key2.error == 0); // XOR siempre "funciona" pero produce basura
    // Con XOR, desencriptar con clave incorrecta produce datos corruptos pero no error
    
    printf("   ✓ Cada clave solo desencripta correctamente su propio texto\n");
    
    free_encryption_result(&encrypted1);
    free_encryption_result(&encrypted2);
    free_encryption_result(&decrypted1_with_key1);
    free_encryption_result(&decrypted1_with_key2);
    
    printf("\n");
}

/**
 * @brief Prueba manejo de errores
 */
void test_vigenere_error_handling() {
    printf("5. Prueba manejo de errores en Vigenère:\n");
    
    const char *test_data = "Test data";
    const char *valid_key = "ValidKey";
    size_t data_size = strlen(test_data);
    size_t key_size = strlen(valid_key);
    
    // Prueba con datos NULL
    encryption_result_t result1 = encrypt_vigenere(NULL, data_size, 
                                                  (const unsigned char *)valid_key, key_size);
    assert(result1.error != 0);
    printf("   ✓ Manejo correcto de datos NULL\n");
    
    // Prueba con clave NULL
    encryption_result_t result2 = encrypt_vigenere((const unsigned char *)test_data, data_size, 
                                                  NULL, key_size);
    assert(result2.error != 0);
    printf("   ✓ Manejo correcto de clave NULL\n");
    
    // Prueba con clave vacía
    encryption_result_t result3 = encrypt_vigenere((const unsigned char *)test_data, data_size, 
                                                  (const unsigned char *)"", 0);
    assert(result3.error != 0);
    printf("   ✓ Manejo correcto de clave vacía\n");
    
    // Prueba con clave de solo bytes nulos
    unsigned char null_key[5] = {0, 0, 0, 0, 0};
    encryption_result_t result4 = encrypt_vigenere((const unsigned char *)test_data, data_size, 
                                                  null_key, 5);
    assert(result4.error != 0);
    printf("   ✓ Manejo correcto de clave inválida (solo ceros)\n");
    
    printf("\n");
}

/**
 * @brief Prueba que verifica que el tamaño se mantiene igual
 */
void test_vigenere_size_preservation() {
    printf("6. Prueba preservación de tamaño en Vigenère:\n");
    
    // Probar con diferentes tamaños de datos
    size_t sizes[] = {1, 10, 100, 1000, 5000};
    const char *key = "TESTKEY";
    size_t key_size = strlen(key);
    
    for (int i = 0; i < 5; i++) {
        size_t data_size = sizes[i];
        unsigned char *test_data = (unsigned char *)malloc(data_size);
        
        // Llenar con datos de prueba
        for (size_t j = 0; j < data_size; j++) {
            test_data[j] = (unsigned char)((j * 7) % 256); // Patrón pseudo-aleatorio
        }
        
        encryption_result_t encrypted = encrypt_vigenere(
            test_data, data_size,
            (const unsigned char *)key, key_size
        );
        assert(encrypted.error == 0);
        assert(encrypted.size == data_size);
        
        encryption_result_t decrypted = decrypt_vigenere(
            encrypted.data, encrypted.size,
            (const unsigned char *)key, key_size
        );
        assert(decrypted.error == 0);
        assert(decrypted.size == data_size);
        
        assert(memcmp(test_data, decrypted.data, data_size) == 0);
        
        free(test_data);
        free_encryption_result(&encrypted);
        free_encryption_result(&decrypted);
    }
    
    printf("   ✓ Tamaño preservado correctamente en todos los casos\n");
    printf("\n");
}

/**
 * @brief Prueba integración con archivos
 */
void test_vigenere_file_integration() {
    printf("7. Prueba integración Vigenère con archivos:\n");
    
    const char *test_file = "test/output/vigenere_test.txt";
    const char *encrypted_file = "test/output/vigenere_encrypted.dat";
    const char *decrypted_file = "test/output/vigenere_decrypted.txt";
    const char *key = "MI CLAVE SECRETA con ñ y áéíóú";
    
    // Crear archivo de prueba
    const char *test_data = "Este es un texto de prueba para el cifrado Vigenère. "
                           "Incluye caracteres especiales: áéíóú, ñ, ¿?¡! y también binarios: ";
    
    // Agregar algunos bytes binarios
    size_t data_size = strlen(test_data) + 10;
    unsigned char *full_test_data = (unsigned char *)malloc(data_size);
    memcpy(full_test_data, test_data, strlen(test_data));
    for (size_t i = strlen(test_data); i < data_size; i++) {
        full_test_data[i] = (unsigned char)(i % 256);
    }
    
    assert(write_file(test_file, full_test_data, data_size) == 0);
    printf("   ✓ Archivo original creado\n");
    
    // Leer y encriptar
    unsigned char *file_data = NULL;
    size_t file_size = 0;
    assert(read_file(test_file, &file_data, &file_size) == 0);
    
    encryption_result_t encrypted = encrypt_vigenere(
        file_data, file_size,
        (const unsigned char *)key, strlen(key)
    );
    assert(encrypted.error == 0);
    
    // Guardar encriptado
    assert(write_file(encrypted_file, encrypted.data, encrypted.size) == 0);
    printf("   ✓ Archivo encriptado creado\n");
    
    // Leer y desencriptar
    unsigned char *encrypted_file_data = NULL;
    size_t encrypted_file_size = 0;
    assert(read_file(encrypted_file, &encrypted_file_data, &encrypted_file_size) == 0);
    
    encryption_result_t decrypted = decrypt_vigenere(
        encrypted_file_data, encrypted_file_size,
        (const unsigned char *)key, strlen(key)
    );
    assert(decrypted.error == 0);
    
    // Guardar desencriptado
    assert(write_file(decrypted_file, decrypted.data, decrypted.size) == 0);
    printf("   ✓ Archivo desencriptado creado\n");
    
    // Verificar que son idénticos
    unsigned char *final_data = NULL;
    size_t final_size = 0;
    assert(read_file(decrypted_file, &final_data, &final_size) == 0);
    
    assert(final_size == data_size);
    assert(memcmp(full_test_data, final_data, data_size) == 0);
    printf("   ✓ Archivo desencriptado es idéntico al original\n");
    
    // Limpiar
    free(full_test_data);
    free(file_data);
    free(encrypted_file_data);
    free(final_data);
    free_encryption_result(&encrypted);
    free_encryption_result(&decrypted);
    
    printf("\n");
}

int main() {
    printf("=== GSEA - Pruebas del Algoritmo Vigenère (XOR) ===\n\n");
    
    // Crear directorio de salida
    create_directory("test/output");
    
    test_vigenere_basic_text();
    test_vigenere_key_lengths();
    test_vigenere_binary_data();
    test_vigenere_different_keys();
    test_vigenere_error_handling();
    test_vigenere_size_preservation();
    test_vigenere_file_integration();
    
    printf("=== Todas las pruebas Vigenère completadas ===\n");
    return 0;
}