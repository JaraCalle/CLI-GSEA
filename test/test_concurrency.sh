#!/bin/bash

echo "=== Pruebas de la Concurrencia ==="
echo

make clean
make test-concurrency

echo ""
echo "=== Pruebas completadas ==="