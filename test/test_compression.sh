#!/bin/bash

echo "=== Pruebas de la Compresión de archivos RLE ==="
echo

make clean
make test-compression

echo ""
echo "=== Pruebas completadas ==="