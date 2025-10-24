#!/bin/bash

echo "=== Pruebas de Integración ==="
echo

make clean
make test-integrated

echo ""
echo "=== Pruebas completadas ==="