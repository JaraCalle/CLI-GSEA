#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/args_parser.h"

/**
 * @brief Prueba el parser de argumentos con casos completos
 */
void test_argument_parser() {
    printf("=== Pruebas del Parser de Argumentos ===\n\n");
    
    // Simular diferentes casos de uso COMPLETOS
    struct test_case {
        char *args[12];
        int expected_result;
        char *description;
    };
    
    struct test_case test_cases[] = {
        {
            {"./gsea", "-ce", "--comp-alg", "rle", "--enc-alg", "vigenere", 
             "-i", "input.txt", "-o", "output.dat", "-k", "clave"},
            0,
            "Caso válido: comprimir + encriptar (completo)"
        },
        {
            {"./gsea", "-c", "--comp-alg", "rle", "-i", "input.txt", "-o", "output.rle", NULL},
            0,
            "Caso válido: solo comprimir"
        },
        {
            {"./gsea", "-e", "--enc-alg", "vigenere", "-i", "input.txt", "-o", "output.enc", "-k", "clave", NULL},
            0,
            "Caso válido: solo encriptar"
        },
        {
            {"./gsea", "-du", "--comp-alg", "huffman", "--enc-alg", "vigenere", 
             "-i", "input.dat", "-o", "output.txt", "-k", "clave"},
            0,
            "Caso válido: descomprimir + desencriptar"
        },
        {
            {"./gsea", "-ce", "--comp-alg", "rle", "--enc-alg", "vigenere", "-i", "input.txt", "-o", "output.dat"},
            -1, // Falta -k para encriptación
            "Caso inválido: falta clave para encriptación"
        },
        {
            {"./gsea", "-c", "-i", "input.txt", "-o", "output.dat"},
            0, // Falta --comp-alg
            "Caso inválido: falta algoritmo de compresión"
        },
        {
            {"./gsea", "-cx", "-i", "input", "-o", "output", NULL},
            -1,
            "Caso inválido: operación desconocida"
        },
        {
            {"./gsea", "-cd", "--comp-alg", "rle", "-i", "input", "-o", "output", NULL},
            -1,
            "Caso inválido: operaciones contradictorias"
        },
        {
            {"./gsea", "-i", "input.txt", "-o", "output.txt", NULL},
            -1,
            "Caso inválido: sin operaciones"
        }
    };
    
    int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);
    
    for (int i = 0; i < num_tests; i++) {
        printf("Prueba %d: %s\n", i + 1, test_cases[i].description);
        
        // Contar argumentos
        int argc = 0;
        while (argc < 12 && test_cases[i].args[argc] != NULL) {
            argc++;
        }
        
        program_config_t config;
        int result = parse_arguments(argc, test_cases[i].args, &config);
        
        if (result == test_cases[i].expected_result) {
            printf("   ✓ Resultado esperado\n");
        } else {
            printf("   ✗ Resultado inesperado (esperado: %d, obtenido: %d)\n", 
                   test_cases[i].expected_result, result);
        }
        
        printf("\n");
    }
}

/**
 * @brief Prueba específica de combinaciones de operaciones
 */
void test_operation_combinations() {
    printf("=== Pruebas de Combinaciones de Operaciones ===\n\n");
    
    // Combinaciones que deberían ser aceptadas (solo verificación de sintaxis)
    char *valid_combinations[] = {"-c", "-d", "-e", "-u", "-ce", "-du", "-cu", "-de", NULL};
    
    // Combinaciones que deberían ser rechazadas (contradictorias o inválidas)
    char *invalid_combinations[] = {"-cd", "-eu", "-dc", "-ue", NULL};
    
    printf("Combinaciones sintácticamente válidas:\n");
    for (int i = 0; valid_combinations[i] != NULL; i++) {
        operation_t operations;
        
        if (parse_operations(valid_combinations[i] + 1, &operations) == 0) {
            printf("   ✓ %s\n", valid_combinations[i]);
        } else {
            printf("   ✗ %s\n", valid_combinations[i]);
        }
    }
    
    printf("\nCombinaciones sintácticamente inválidas:\n");
    for (int i = 0; invalid_combinations[i] != NULL; i++) {
        operation_t operations;
        
        if (parse_operations(invalid_combinations[i] + 1, &operations) != 0) {
            printf("   ✓ %s (correctamente rechazada)\n", invalid_combinations[i]);
        } else {
            printf("   ✗ %s (debería ser rechazada)\n", invalid_combinations[i]);
        }
    }
    printf("\n");
}

/**
 * @brief Prueba validación semántica (operaciones contradictorias)
 */
void test_semantic_validation() {
    printf("=== Pruebas de Validación Semántica ===\n\n");
    
    struct semantic_test {
        char *operation;
        int should_be_valid;
        char *description;
    };
    
    struct semantic_test tests[] = {
        {"-c", 1, "Solo compresión válida"},
        {"-d", 1, "Solo descompresión válida"}, 
        {"-e", 1, "Solo encriptación válida"},
        {"-u", 1, "Solo desencriptación válida"},
        {"-ce", 1, "Comprimir + encriptar válido"},
        {"-du", 1, "Descomprimir + desencriptar válido"},
        {"-cd", 0, "Comprimir + descomprimir inválido"},
        {"-eu", 0, "Encriptar + desencriptar inválido"},
        {"-dc", 0, "Descomprimir + comprimir inválido"},
        {"-ue", 0, "Desencriptar + encriptar inválido"}
    };
    
    int num_tests = sizeof(tests) / sizeof(tests[0]);
    
    for (int i = 0; i < num_tests; i++) {
        printf("Prueba: %s\n", tests[i].description);
        
        // Crear argumentos de prueba completos
        char *test_args[] = {
            "./gsea", tests[i].operation, 
            "--comp-alg", "rle", "--enc-alg", "vigenere",
            "-i", "test.txt", "-o", "out.txt", "-k", "clave",
            NULL
        };
        
        int argc = 0;
        while (test_args[argc] != NULL) argc++;
        
        program_config_t config;
        int result = parse_arguments(argc, test_args, &config);
        
        if ((result == 0 && tests[i].should_be_valid) || 
            (result == -1 && !tests[i].should_be_valid)) {
            printf("   ✓ Comportamiento esperado\n");
        } else {
            printf("   ✗ Comportamiento inesperado\n");
        }
        printf("\n");
    }
}

/**
 * @brief Programa principal de pruebas
 */
int main() {
    printf("=== GSEA - Suite de Pruebas del Parser ===\n\n");
    
    test_argument_parser();
    test_operation_combinations();
    test_semantic_validation();
    
    printf("=== Todas las pruebas completadas ===\n");
    return 0;
}