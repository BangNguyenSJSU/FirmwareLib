# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -IL1_drivers -IL2_services -IL3_app
LDFLAGS = -lm -lgpiod

# Directories
L1_DIR = L1_drivers
L2_DIR = L2_services
L3_DIR = L3_app
BUILD_DIR = build
BIN_DIR = bin

# Installation
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
INSTALL = install
INSTALL_PROGRAM = $(INSTALL) -m 755

# Target executable
TARGET = $(BIN_DIR)/pi_monitor

# Source files
L1_SOURCES = $(wildcard $(L1_DIR)/*.c)
L2_SOURCES = $(wildcard $(L2_DIR)/*.c)
L3_SOURCES = $(wildcard $(L3_DIR)/*.c)

# Object files
L1_OBJECTS = $(patsubst $(L1_DIR)/%.c,$(BUILD_DIR)/%.o,$(L1_SOURCES))
L2_OBJECTS = $(patsubst $(L2_DIR)/%.c,$(BUILD_DIR)/%.o,$(L2_SOURCES))
L3_OBJECTS = $(patsubst $(L3_DIR)/%.c,$(BUILD_DIR)/%.o,$(L3_SOURCES))

OBJECTS = $(L1_OBJECTS) $(L2_OBJECTS) $(L3_OBJECTS)

# Default target
all: $(TARGET)

# Link the target executable
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

# Compile L1_drivers sources
$(BUILD_DIR)/%.o: $(L1_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile L2_services sources
$(BUILD_DIR)/%.o: $(L2_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile L3_app sources
$(BUILD_DIR)/%.o: $(L3_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create directories if they don't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Install to system
install: $(TARGET)
	@echo "Installing to $(BINDIR)..."
	$(INSTALL) -d $(BINDIR)
	$(INSTALL_PROGRAM) $(TARGET) $(BINDIR)/pi_monitor
	@echo "Installation complete: $(BINDIR)/pi_monitor"
	@echo "Run with: sudo pi_monitor"

# Uninstall from system
uninstall:
	@echo "Removing $(BINDIR)/pi_monitor..."
	rm -f $(BINDIR)/pi_monitor
	@echo "Uninstall complete"

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "Clean complete"

# Rebuild from scratch
rebuild: clean all

# Run the application
run: $(TARGET)
	sudo ./$(TARGET)

.PHONY: all clean rebuild run install uninstall
