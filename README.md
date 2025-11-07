# CLI-GSEA

CLI-GSEA (Gestión Segura y Eficiente de Archivos) es una herramienta de línea de comandos en C que permite comprimir y encriptar archivos de forma individual o en directorios completos usando procesamiento concurrente.

## Características principales:

* Compresión de archivos usando algoritmo RLE (Run-Length Encoding)
* Encriptación usando cifrado Vigenère
* Procesamiento concurrente de directorios completos con múltiples hilos
* Operaciones combinadas (comprimir + encriptar) o individuales

## Requisitos del Sistema

### Dependencias:

* GCC (compilador C con soporte para C99) makefile:2-3
* GNU Make
* Biblioteca POSIX threads (pthreads)
* Sistema operativo compatible con POSIX

## Compilación

### Compilar el proyecto:

```bash
make
```

### Uso Básico

Sintaxis general:

```bash
./gsea [OPERACIONES] [OPCIONES]
```

### Ejemplo de uso:

Comprimir un archivo:

```bash
./gsea -c --comp-alg rle -i input.txt -o output.rle
```

## Opciones de operaciones:

    -c: Comprimir
    -d: Descomprimir
    -e: Encriptar
    -u: Desencriptar
    -ce: Comprimir y encriptar
    -du: Desencriptar y descomprimir

## Ejecutar pruebas individuales:

```bash
make test-compression    # Pruebas de compresión  
make test-encryption     # Pruebas de encriptación  
make test-integrated     # Pruebas de integración  
make test-concurrency    # Pruebas de concurrencia
```

## Estructura del Proyecto

```
CLI-GSEA/  
├── src/              # Código fuente principal  
├── include/          # Archivos de cabecera  
├── test/             # Suite de pruebas  
├── makefile          # Sistema de compilación  
└── gsea              # Ejecutable (después de compilar)  
```
