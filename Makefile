# Bitcoin Analysis Toolkit - C Edition
# Makefile for building with raylib

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99

# raylib settings - raylib安装程序提供的路径
RAYLIB_PATH = C:/raylib/w64devkit/x86_64-w64-mingw32
RAYLIB_LIB = -lraylib -lopengl32 -lgdi32 -lwinmm
LDFLAGS = -lm

# Source files
SRCDIR = src
OBJDIR = obj
BINDIR = .

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
TARGET = $(BINDIR)/btc_analysis.exe

# Include paths
INCLUDES = -I$(SRCDIR) -I$(RAYLIB_PATH)/include

# Library paths
LIBDIRS = -L$(RAYLIB_PATH)/lib

# Default target
all: dirs $(TARGET)

# Create directories
dirs:
	mkdir -p $(OBJDIR)

# Link
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LIBDIRS) $(RAYLIB_LIB) $(LDFLAGS)

# Compile
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean
clean:
	rm -rf $(OBJDIR) $(TARGET)

# Run
run: all
	$(TARGET)

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: clean all

# Help
help:
	@echo Bitcoin Analysis Toolkit - Build System
	@echo.
	@echo Targets:
	@echo   all      - Build the project (default)
	@echo   clean    - Remove build files
	@echo   run      - Build and run
	@echo   debug    - Build with debug symbols
	@echo   help     - Show this help

.PHONY: all dirs clean run debug help
