# Compilador y flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -Iinclude -D_POSIX_C_SOURCE=200809L
TARGET = gsea

# Directorios
SRCDIR = src
TESTDIR = test
INCDIR = include
BUILDDIR = build
TESTBUILDDIR = $(BUILDDIR)/test

# Archivos fuente principales (excluyendo main.c para pruebas)
CORE_SOURCES = $(filter-out $(SRCDIR)/main.c, $(wildcard $(SRCDIR)/*.c))
CORE_OBJECTS = $(CORE_SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

# Objetos principales (incluyendo main.c)
MAIN_OBJECTS = $(BUILDDIR)/main.o $(CORE_OBJECTS)

# Archivos de prueba
TEST_FILE_MANAGER = $(TESTDIR)/test_file_manager.c
TEST_PARSER = $(TESTDIR)/test_parser.c
TEST_COMPRESSION = $(TESTDIR)/test_compression.c
TEST_ENCRYPTION = $(TESTDIR)/test_encryption.c
TEST_INTEGRATED = $(TESTDIR)/test_integrated.c
TEST_DIR_UTILS = $(TESTDIR)/test_dir_utils.c


# Target principal
$(TARGET): $(MAIN_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

# Compilar objetos principales
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Programa de pruebas integradas
$(TESTBUILDDIR)/test_integrated: $(TEST_INTEGRATED) $(CORE_OBJECTS)
	@mkdir -p $(TESTBUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@

# Programa de pruebas del file manager
$(TESTBUILDDIR)/test_file_manager: $(TEST_FILE_MANAGER) $(CORE_OBJECTS)
	@mkdir -p $(TESTBUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@

# Programa de pruebas del parser
$(TESTBUILDDIR)/test_parser: $(TEST_PARSER) $(CORE_OBJECTS)
	@mkdir -p $(TESTBUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@

# Programa de pruebas de compresión
$(TESTBUILDDIR)/test_compression: $(TEST_COMPRESSION) $(CORE_OBJECTS)
	@mkdir -p $(TESTBUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@

# Programa de pruebas de encriptación
$(TESTBUILDDIR)/test_encryption: $(TEST_ENCRYPTION) $(CORE_OBJECTS)
	@mkdir -p $(TESTBUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@

# Programa de pruebas de utilidades de directorio
$(TESTBUILDDIR)/test_dir_utils: $(TEST_DIR_UTILS) $(CORE_OBJECTS)
	@mkdir -p $(TESTBUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@

# Targets principales
all: $(TARGET)

# Targets de prueba
test-file-manager: $(TESTBUILDDIR)/test_file_manager
	@echo "=== Ejecutando pruebas del file manager ==="
	@./$(TESTBUILDDIR)/test_file_manager

test-parser: $(TESTBUILDDIR)/test_parser
	@echo "=== Ejecutando pruebas del parser ==="
	@./$(TESTBUILDDIR)/test_parser

test-integrated: $(TESTBUILDDIR)/test_integrated
	@echo "=== Ejecutando pruebas de integración ==="
	@./$(TESTBUILDDIR)/test_integrated

test-compression: $(TESTBUILDDIR)/test_compression
	@echo "=== Ejecutando pruebas de compresión ==="
	@./$(TESTBUILDDIR)/test_compression

test-encryption: $(TESTBUILDDIR)/test_encryption
	@echo "=== Ejecutando pruebas de encriptación ==="
	@./$(TESTBUILDDIR)/test_encryption

test-dir-utils: $(TESTBUILDDIR)/test_dir_utils
	@echo "=== Ejecutando pruebas de utilidades de directorio ==="
	@./$(TESTBUILDDIR)/test_dir_utils

test-all: test-file-manager test-parser test-compression test-encryption test-dir-utils test-integrated
	@echo "=== Todas las pruebas completadas ==="

# Limpiar
clean:
	rm -rf $(BUILDDIR) $(TARGET) test/output/*

# Verificar fugas de memoria
valgrind-test: $(TESTBUILDDIR)/test_file_manager
	@echo "=== Verificando fugas de memoria ==="
	@valgrind --leak-check=full --track-origins=yes --error-exitcode=1 ./$(TESTBUILDDIR)/test_file_manager

# Scripts de prueba
test-scripts:
	@chmod +x $(TESTDIR)/test_parser.sh
	@chmod +x $(TESTDIR)/test_files.sh
	@chmod +x $(TESTDIR)/test_dir_utils.sh

.PHONY: all clean test-all test-file-manager test-parser valgrind-test test-scripts test-compression test-encryption test-dir-utils test-integrated