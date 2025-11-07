#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "../include/compression.h"

// Estructuras para Huffman
typedef struct HuffmanNode {
    unsigned char byte;
    unsigned frequency;
    struct HuffmanNode *left;
    struct HuffmanNode *right;
} HuffmanNode;

typedef struct {
    unsigned char byte;
    unsigned code;
    unsigned code_length;
} HuffmanCode;

typedef struct {
    HuffmanCode *codes;
    size_t count;
} HuffmanTable;

// Estructura para el header del archivo comprimido
typedef struct {
    unsigned char magic[4];  // "HUFF"
    size_t original_size;
    size_t table_size;
    size_t compressed_data_size;
} HuffmanHeader;

// Prototipos de funciones internas
HuffmanNode* create_node(unsigned char byte, unsigned frequency);
HuffmanNode* build_huffman_tree(unsigned frequencies[]);
void build_huffman_codes(HuffmanNode *root, unsigned code, unsigned length, HuffmanTable *table);
void free_huffman_tree(HuffmanNode *root);
int compare_nodes(const void *a, const void *b);
unsigned char get_bit(unsigned code, unsigned position);
void write_bit(unsigned char *buffer, size_t *bit_position, unsigned char bit);
unsigned char read_bit(const unsigned char *buffer, size_t bit_position);
size_t serialize_huffman_table(const HuffmanTable *table, unsigned char **data);
HuffmanTable* deserialize_huffman_table(const unsigned char *data, size_t size);
compression_result_t compress_small_file(const unsigned char *input, size_t input_size);
size_t serialize_huffman_table_optimized(const HuffmanTable *table, unsigned char **data);

// Constantes
#define HUFFMAN_MAGIC "HUFF"
#define MAX_BYTES 256
#define MAX_TREE_NODES 511  // 2*256 - 1

// Función para crear un nodo del árbol
HuffmanNode* create_node(unsigned char byte, unsigned frequency) {
    HuffmanNode *node = (HuffmanNode*)malloc(sizeof(HuffmanNode));
    if (node) {
        node->byte = byte;
        node->frequency = frequency;
        node->left = node->right = NULL;
    }
    return node;
}

// Función de comparación para qsort
int compare_nodes(const void *a, const void *b) {
    HuffmanNode *node_a = *(HuffmanNode**)a;
    HuffmanNode *node_b = *(HuffmanNode**)b;
    return (node_a->frequency > node_b->frequency) - (node_a->frequency < node_b->frequency);
}

// Construir el árbol de Huffman
HuffmanNode* build_huffman_tree(unsigned frequencies[]) {
    HuffmanNode *nodes[MAX_BYTES];
    int node_count = 0;
    
    // Crear nodos para bytes con frecuencia > 0
    for (int i = 0; i < MAX_BYTES; i++) {
        if (frequencies[i] > 0) {
            nodes[node_count++] = create_node((unsigned char)i, frequencies[i]);
        }
    }
    
    if (node_count == 0) return NULL;
    
    // Construir el árbol
    while (node_count > 1) {
        // Ordenar nodos por frecuencia (ascendente)
        for (int i = 0; i < node_count - 1; i++) {
            for (int j = i + 1; j < node_count; j++) {
                if (nodes[i]->frequency > nodes[j]->frequency) {
                    HuffmanNode *temp = nodes[i];
                    nodes[i] = nodes[j];
                    nodes[j] = temp;
                }
            }
        }
        
        // Combinar los dos nodos con menor frecuencia
        HuffmanNode *new_node = create_node(0, nodes[0]->frequency + nodes[1]->frequency);
        new_node->left = nodes[0];
        new_node->right = nodes[1];
        
        // Reorganizar el array
        nodes[0] = new_node;
        for (int i = 1; i < node_count - 1; i++) {
            nodes[i] = nodes[i + 1];
        }
        node_count--;
    }
    
    return nodes[0];
}

// Construir tabla de códigos Huffman
void build_huffman_codes(HuffmanNode *root, unsigned code, unsigned length, HuffmanTable *table) {
    if (!root) return;
    
    if (!root->left && !root->right) {
        // Nodo hoja, agregar a la tabla
        table->codes[table->count].byte = root->byte;
        table->codes[table->count].code = code;
        table->codes[table->count].code_length = length;
                
        table->count++;
        return;
    }
    
    // Recorrer izquierda (bit 0)
    if (length < 32) {
        build_huffman_codes(root->left, code << 1, length + 1, table);
    }
    
    // Recorrer derecha (bit 1)
    if (length < 32) {
        build_huffman_codes(root->right, (code << 1) | 1, length + 1, table);
    }
}

// Liberar el árbol de Huffman
void free_huffman_tree(HuffmanNode *root) {
    if (!root) return;
    free_huffman_tree(root->left);
    free_huffman_tree(root->right);
    free(root);
}

// Obtener un bit específico de un código
unsigned char get_bit(unsigned code, unsigned position) {
    return (code >> position) & 1;
}

// Escribir un bit en el buffer
void write_bit(unsigned char *buffer, size_t *bit_position, unsigned char bit) {
    size_t byte_pos = *bit_position / 8;
    size_t bit_pos = *bit_position % 8;
    
    if (bit) {
        buffer[byte_pos] |= (1 << (7 - bit_pos));
    } else {
        buffer[byte_pos] &= ~(1 << (7 - bit_pos));
    }
    
    (*bit_position)++;
}

// Leer un bit del buffer
unsigned char read_bit(const unsigned char *buffer, size_t bit_position) {
    size_t byte_pos = bit_position / 8;
    size_t bit_pos = bit_position % 8;
    return (buffer[byte_pos] >> (7 - bit_pos)) & 1;
}

// Serializar la tabla Huffman (versión original)
size_t serialize_huffman_table(const HuffmanTable *table, unsigned char **data) {
    // Formato: [count:2 bytes][(byte, code_length, code)...]
    size_t size = 2; // count (2 bytes)
    
    for (size_t i = 0; i < table->count; i++) {
        size += 1 + 1 + (table->codes[i].code_length + 7) / 8; // byte + code_length + code bytes
    }
    
    *data = (unsigned char*)malloc(size);
    if (!*data) return 0;
    
    unsigned char *ptr = *data;
    
    // Escribir count (2 bytes)
    *ptr++ = (table->count >> 8) & 0xFF;
    *ptr++ = table->count & 0xFF;
    
    // Escribir cada código
    for (size_t i = 0; i < table->count; i++) {
        HuffmanCode *code = &table->codes[i];
        
        // Byte
        *ptr++ = code->byte;
        
        // Longitud del código (1 byte)
        *ptr++ = code->code_length;
        
        // Código (almacenado en bytes suficientes)
        unsigned bytes_needed = (code->code_length + 7) / 8;
        for (unsigned j = 0; j < bytes_needed; j++) {
            unsigned shift = (bytes_needed - 1 - j) * 8;
            *ptr++ = (code->code >> shift) & 0xFF;
        }
    }
    
    return size;
}

// Serialización optimizada de tabla Huffman
size_t serialize_huffman_table_optimized(const HuffmanTable *table, unsigned char **data) {
    // Formato más compacto: [count:1 byte][(byte, code_length:4 bits, code...)...]
    size_t size = 1; // count (1 byte, máximo 256)
    
    for (size_t i = 0; i < table->count; i++) {
        size += 1 + 1; // byte + code_length (en 4 bits) + code bytes
        size += (table->codes[i].code_length + 7) / 8;
    }
    
    *data = (unsigned char*)malloc(size);
    if (!*data) return 0;
    
    unsigned char *ptr = *data;
    
    // Escribir count (1 byte)
    *ptr++ = (unsigned char)table->count;
    
    // Escribir cada código
    for (size_t i = 0; i < table->count; i++) {
        HuffmanCode *code = &table->codes[i];
        
        // Byte
        *ptr++ = code->byte;
        
        // Longitud del código en 4 bits (máximo 15)
        unsigned char code_length_byte = (code->code_length & 0x0F);
        *ptr++ = code_length_byte;
        
        // Código
        unsigned bytes_needed = (code->code_length + 7) / 8;
        for (unsigned j = 0; j < bytes_needed; j++) {
            unsigned shift = (bytes_needed - 1 - j) * 8;
            *ptr++ = (code->code >> shift) & 0xFF;
        }
    }
    
    return size;
}

// Deserializar la tabla Huffman
HuffmanTable* deserialize_huffman_table(const unsigned char *data, size_t size) {
    if (size < 2) return NULL;
    
    HuffmanTable *table = (HuffmanTable*)malloc(sizeof(HuffmanTable));
    if (!table) return NULL;
    
    const unsigned char *ptr = data;
    
    // Leer count (2 bytes)
    size_t count = ((size_t)ptr[0] << 8) | ptr[1];
    ptr += 2;
    
    
    table->count = 0;
    table->codes = (HuffmanCode*)malloc(sizeof(HuffmanCode) * count);
    if (!table->codes) {
        free(table);
        return NULL;
    }
    
    // Leer cada código
    for (size_t i = 0; i < count && ptr < data + size; i++) {
        HuffmanCode *code = &table->codes[table->count];
        
        if (ptr + 2 > data + size) {
            break;
        }
        
        // Byte
        code->byte = *ptr++;
        
        // Longitud del código
        code->code_length = *ptr++;
        
        // Calcular bytes necesarios para el código
        unsigned bytes_needed = (code->code_length + 7) / 8;
        
        if (ptr + bytes_needed > data + size) {
            break;
        }
        
        code->code = 0;
        for (unsigned j = 0; j < bytes_needed; j++) {
            code->code = (code->code << 8) | *ptr++;
        }
                
        table->count++;
    }
    
    return table;
}

// Función para archivos pequeños (Huffman no es eficiente)
compression_result_t compress_small_file(const unsigned char *input, size_t input_size) {
    compression_result_t result = {NULL, 0, 0};
    
    // Para archivos pequeños, simplemente almacenar con un header especial
    size_t total_size = sizeof(HuffmanHeader) + input_size;
    unsigned char *data = (unsigned char*)malloc(total_size);
    if (!data) {
        result.error = -1;
        return result;
    }
    
    HuffmanHeader header;
    memcpy(header.magic, "SMAL", 4); // Magic diferente para archivos pequeños
    header.original_size = input_size;
    header.table_size = 0;
    header.compressed_data_size = input_size;
    
    memcpy(data, &header, sizeof(header));
    memcpy(data + sizeof(header), input, input_size);
    
    result.data = data;
    result.size = total_size;
    return result;
}

// Función principal de compresión Huffman
compression_result_t compress_huffman(const unsigned char *input, size_t input_size) {
    compression_result_t result = {NULL, 0, 0};
    
    if (!input || input_size == 0) {
        result.error = -1;
        return result;
    }
    
    // Si el archivo es muy pequeño, Huffman puede no ser eficiente
    if (input_size < 10) {
        // Para archivos muy pequeños, usar almacenamiento simple
        return compress_small_file(input, input_size);
    }
    
    // Paso 1: Calcular frecuencias
    unsigned frequencies[MAX_BYTES] = {0};
    for (size_t i = 0; i < input_size; i++) {
        frequencies[input[i]]++;
    }
    
    // Paso 2: Construir árbol de Huffman
    HuffmanNode *root = build_huffman_tree(frequencies);
    if (!root) {
        result.error = -2;
        return result;
    }
    
    // Paso 3: Construir tabla de códigos
    HuffmanTable table = {NULL, 0};
    table.codes = (HuffmanCode*)malloc(sizeof(HuffmanCode) * MAX_BYTES);
    if (!table.codes) {
        free_huffman_tree(root);
        result.error = -3;
        return result;
    }
    
    build_huffman_codes(root, 0, 0, &table);
    
    // Verificar que tenemos códigos para todos los bytes usados
    if (table.count == 0) {
        free(table.codes);
        free_huffman_tree(root);
        result.error = -4;
        return result;
    }
    
    
    // Paso 4: Serializar tabla Huffman (usar versión original más robusta)
    unsigned char *table_data = NULL;
    size_t table_size = serialize_huffman_table(&table, &table_data);
    if (!table_data) {
        free(table.codes);
        free_huffman_tree(root);
        result.error = -5;
        return result;
    }
    
    
    // Paso 5: Comprimir datos
    size_t max_compressed_bits = 0;
    for (size_t i = 0; i < input_size; i++) {
        unsigned char byte = input[i];
        for (size_t j = 0; j < table.count; j++) {
            if (table.codes[j].byte == byte) {
                max_compressed_bits += table.codes[j].code_length;
                break;
            }
        }
    }
    
    size_t max_compressed_bytes = (max_compressed_bits + 7) / 8;
    unsigned char *compressed_data = (unsigned char*)calloc(max_compressed_bytes + 1, 1);
    if (!compressed_data) {
        free(table_data);
        free(table.codes);
        free_huffman_tree(root);
        result.error = -6;
        return result;
    }
    
    size_t bit_position = 0;
    
    // Codificar datos de entrada
    for (size_t i = 0; i < input_size; i++) {
        unsigned char byte = input[i];
        HuffmanCode *code = NULL;
        
        for (size_t j = 0; j < table.count; j++) {
            if (table.codes[j].byte == byte) {
                code = &table.codes[j];
                break;
            }
        }
        
        if (!code) {
            free(compressed_data);
            free(table_data);
            free(table.codes);
            free_huffman_tree(root);
            result.error = -7;
            return result;
        }
        
        // Escribir cada bit del código
        for (unsigned bit = 0; bit < code->code_length; bit++) {
            unsigned shift_amount = code->code_length - 1 - bit;
            unsigned char current_bit = (code->code >> shift_amount) & 1;
            write_bit(compressed_data, &bit_position, current_bit);
        }
    }
    
    size_t compressed_data_size = (bit_position + 7) / 8;
    
    
    // Paso 6: Crear archivo comprimido final
    HuffmanHeader header;
    memcpy(header.magic, HUFFMAN_MAGIC, 4);
    header.original_size = input_size;
    header.table_size = table_size;
    header.compressed_data_size = compressed_data_size;
    
    size_t total_size = sizeof(header) + table_size + compressed_data_size;
    unsigned char *final_data = (unsigned char*)malloc(total_size);
    if (!final_data) {
        free(compressed_data);
        free(table_data);
        free(table.codes);
        free_huffman_tree(root);
        result.error = -8;
        return result;
    }
    
    // Ensamblar archivo comprimido
    unsigned char *ptr = final_data;
    memcpy(ptr, &header, sizeof(header));
    ptr += sizeof(header);
    memcpy(ptr, table_data, table_size);
    ptr += table_size;
    memcpy(ptr, compressed_data, compressed_data_size);
    
    // Limpiar
    free(compressed_data);
    free(table_data);
    free(table.codes);
    free_huffman_tree(root);
    
    result.data = final_data;
    result.size = total_size;
    result.error = 0;
    
    
    return result;
}

// Función principal de descompresión Huffman
compression_result_t decompress_huffman(const unsigned char *input, size_t input_size) {
    compression_result_t result = {NULL, 0, 0};
    
    
    if (!input || input_size < sizeof(HuffmanHeader)) {
        result.error = -1;
        return result;
    }
    
    // Paso 1: Leer y verificar header
    const HuffmanHeader *header = (const HuffmanHeader*)input;    
    // Verificar si es un archivo pequeño
    if (memcmp(header->magic, "SMAL", 4) == 0) {
        // Archivo pequeño, simplemente extraer datos
        if (sizeof(HuffmanHeader) + header->original_size > input_size) {
            result.error = -3;
            return result;
        }
        
        unsigned char *decompressed_data = (unsigned char*)malloc(header->original_size);
        if (!decompressed_data) {
            result.error = -5;
            return result;
        }
        
        memcpy(decompressed_data, input + sizeof(HuffmanHeader), header->original_size);
        result.data = decompressed_data;
        result.size = header->original_size;
        result.error = 0;
        return result;
    }
    
    // Verificar header Huffman normal
    if (memcmp(header->magic, HUFFMAN_MAGIC, 4) != 0) {
        result.error = -2;
        return result;
    }
    
    size_t expected_size = sizeof(HuffmanHeader) + header->table_size + header->compressed_data_size;
    if (expected_size > input_size) {
        result.error = -3;
        return result;
    }
    
    // Paso 2: Deserializar tabla Huffman
    const unsigned char *table_data = input + sizeof(HuffmanHeader);
    
    HuffmanTable *table = deserialize_huffman_table(table_data, header->table_size);
    if (!table) {
        result.error = -4;
        return result;
    }
    
    // Paso 3: Obtener datos comprimidos
    const unsigned char *compressed_data = table_data + header->table_size;
    
    // Paso 4: Descomprimir datos
    unsigned char *decompressed_data = (unsigned char*)malloc(header->original_size);
    if (!decompressed_data) {
        free(table->codes);
        free(table);
        result.error = -5;
        return result;
    }
    
    size_t decompressed_index = 0;
    size_t bit_position = 0;
    size_t max_bits = header->compressed_data_size * 8;
    
    while (decompressed_index < header->original_size && bit_position < max_bits) {
        unsigned current_code = 0;
        unsigned current_length = 0;
        int code_found = 0;
        
        // Leer bits hasta encontrar un código válido
        while (bit_position < max_bits && current_length < 32 && !code_found) {
            unsigned char bit = read_bit(compressed_data, bit_position);
            current_code = (current_code << 1) | bit;
            current_length++;
            bit_position++;
            
            // Buscar este código en la tabla
            for (size_t i = 0; i < table->count; i++) {
                if (table->codes[i].code == current_code && 
                    table->codes[i].code_length == current_length) {
                    decompressed_data[decompressed_index++] = table->codes[i].byte;
                    code_found = 1;
                    break;
                }
            }
        }
        
        if (!code_found && current_length >= 32) {
            free(decompressed_data);
            free(table->codes);
            free(table);
            result.error = -7;
            return result;
        }
    }
    
    // Verificar que descomprimimos todo
    if (decompressed_index != header->original_size) {
        free(decompressed_data);
        free(table->codes);
        free(table);
        result.error = -6;
        return result;
    }
    
    
    free(table->codes);
    free(table);
    
    result.data = decompressed_data;
    result.size = header->original_size;
    result.error = 0;
    
    return result;
}

// Wrapper functions para la interfaz de compression.h
compression_result_t compress_huffman_wrapper(const unsigned char *input, size_t input_size) {
    return compress_huffman(input, input_size);
}

compression_result_t decompress_huffman_wrapper(const unsigned char *input, size_t input_size) {
    return decompress_huffman(input, input_size);
}