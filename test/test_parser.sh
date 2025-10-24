#!/bin/bash

echo "=== Pruebas del Parser de Argumentos ==="
echo

# Compilar el programa
make clean
make

# Crear archivo de prueba temporal
mkdir -p test/input
echo "Este es un archivo de prueba" > test/input/test1.txt

echo "1. Prueba: Uso correcto con compresión y encriptación"
./gsea -ce --comp-alg rle --enc-alg vigenere -i test/input/test1.txt -o test/output/result.dat -k mi_clave_secreta
echo "Exit code: $?"
echo

echo "2. Prueba: Solo descompresión"
./gsea -d --comp-alg huffman -i archivo.comprimido -o archivo.txt
echo "Exit code: $?"
echo

echo "3. Prueba: Solo compresión"
./gsea -c --comp-alg rle -i test/input/test1.txt -o test/output/comprimido.rle
echo "Exit code: $?"
echo

echo "4. Prueba: Solo encriptación"
./gsea -e --enc-alg vigenere -i test/input/test1.txt -o test/output/encriptado.dat -k clave123
echo "Exit code: $?"
echo

echo "5. Prueba: Descomprimir y desencriptar"
./gsea -du --comp-alg huffman --enc-alg vigenere -i archivo.enc -o resultado.txt -k mi_clave
echo "Exit code: $?"
echo

echo "6. Prueba: Error - opción desconocida"
./gsea -x -i input -o output
echo "Exit code: $?"
echo

echo "7. Prueba: Error - falta argumento para -i"
./gsea -c -i
echo "Exit code: $?"
echo

echo "8. Prueba: Error - operaciones contradictorias"
./gsea -cd -i input -o output
echo "Exit code: $?"
echo

echo "9. Prueba: Error - falta ruta de entrada"
./gsea -c -o output
echo "Exit code: $?"
echo

echo "10. Prueba: Error - algoritmo desconocido"
./gsea -c --comp-alg desconocido -i input -o output
echo "Exit code: $?"
echo

echo "11. Prueba: Ayuda"
./gsea -h
echo "Exit code: $?"
echo

echo "12. Prueba: Error - falta clave para encriptación"
./gsea -e --enc-alg vigenere -i input -o output
echo "Exit code: $?"
echo

echo "13. Prueba: Error - operación inválida en combinación"
./gsea -cx -i input -o output
echo "Exit code: $?"
echo

echo "=== Pruebas completadas ==="