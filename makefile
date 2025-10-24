# Compilador y flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -Iinclude
TARGET = gsea

# Directorios
SRCDIR = src
INCDIR = include
BUILDDIR = build

# Archivos fuente
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

# Target principal
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

# Compilar objetos
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Limpiar
clean:
	rm -rf $(BUILDDIR) $(TARGET)

# Ejecutar pruebas manuales
test-parser: $(TARGET)
	@echo "=== Ejecutando pruebas del parser ==="
	@echo "1. Prueba ayuda:"
	@./$(TARGET) -h 2>&1 | head -5
	@echo
	@echo "2. Prueba operación válida:"
	@./$(TARGET) -ce --comp-alg rle --enc-alg vigenere -i test.txt -o out.dat -k clave123
	@echo
	@echo "3. Prueba error opción desconocida:"
	@./$(TARGET) -x 2>&1 | head -3
	@echo
	@echo "4. Prueba error falta argumento:"
	@./$(TARGET) -i 2>&1 | head -3

.PHONY: clean test-parser