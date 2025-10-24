#!/bin/bash

echo "=== Pruebas del Gestor de Archivos ==="
echo

make clean
make test-file-manager

echo ""
echo "=== Pruebas completadas ==="