# Pi Monitor Makefile - Layered Architecture

# Directories
SRC_DIR     = .
INC_DIR     = include
DRV_DIR     = drivers
SVC_DIR     = services
APP_DIR     = app
BUILD_DIR   = build
BIN_DIR     = bin

# Compiler
CC          = gcc
CFLAGS      = -Wall -Wextra -I$(INC_DIR) -O2
LDFLAGS     = -lgpiod -lm

# Debug flags
DEBUG_FLAGS = -g -DDEBUG

# Sources
DRV_SRCS    = $(DRV_DIR)/gpio_driver.c $(DRV_DIR)/spi_driver.c
SVC_SRCS    = $(SVC_DIR)/st7735s.c $(SVC_DIR)/fonts.c
APP_SRCS    = $(APP_DIR)/main.c $(APP_DIR)/system_info.c

ALL_SRCS    = $(DRV_SRCS) $(SVC_SRCS) $(APP_SRCS)

# Objects
DRV_OBJS    = $(patsubst $(DRV_DIR)/%.c,$(BUILD_DIR)/%.o,$(DRV_SRCS))
SVC_OBJS    = $(patsubst $(SVC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SVC_SRCS))
APP_OBJS    = $(patsubst $(APP_DIR)/%.c,$(BUILD_DIR)/%.o,$(APP_SRCS))

ALL_OBJS    = $(DRV_OBJS) $(SVC_OBJS) $(APP_OBJS)

# Target
TARGET      = $(BIN_DIR)/pi_monitor

# Default target
all: dirs $(TARGET)

# Debug build
debug: CFLAGS += $(DEBUG_FLAGS)
debug: all

# Create directories
dirs:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

# Link
$(TARGET): $(ALL_OBJS)
	$(CC) $(ALL_OBJS) -o $@ $(LDFLAGS)
	@echo "Built: $@"

# Compile drivers
$(BUILD_DIR)/gpio_driver.o: $(DRV_DIR)/gpio_driver.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/spi_driver.o: $(DRV_DIR)/spi_driver.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile services
$(BUILD_DIR)/st7735s.o: $(SVC_DIR)/st7735s.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/fonts.o: $(SVC_DIR)/fonts.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile app
$(BUILD_DIR)/main.o: $(APP_DIR)/main.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/system_info.o: $(APP_DIR)/system_info.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Run
run: all
	sudo $(TARGET)

# Install
install: all
	sudo cp $(TARGET) /usr/local/bin/

# Info
info:
	@echo "Sources:"
	@echo "  Drivers:  $(DRV_SRCS)"
	@echo "  Services: $(SVC_SRCS)"
	@echo "  App:      $(APP_SRCS)"
	@echo "Target: $(TARGET)"

.PHONY: all debug dirs clean run install info
