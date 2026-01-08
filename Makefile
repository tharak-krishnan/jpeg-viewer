CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -O2 -I./include $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs) -lm

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/jpeg_viewer

# Debug build flags
DEBUG_FLAGS = -g -O0 -DDEBUG

.PHONY: all clean debug test

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

debug: CFLAGS += $(DEBUG_FLAGS)
debug: clean $(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Clean complete"

# Test with sample image (if available)
test: $(TARGET)
	@if [ -f test_images/sample.jpg ]; then \
		./$(TARGET) test_images/sample.jpg; \
	else \
		echo "No test image found at test_images/sample.jpg"; \
	fi
