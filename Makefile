# Compiler
CC = gcc

# Flags
CFLAGS = -Wall -Wextra -Iinclude -g -pthread -lm

# Directories
SRC_DIR = src
OBJ_DIR = obj
MAIN = $(SRC_DIR)/core/main.c

# Find all .c files in src (including subdirectories)
SRCS = $(shell find $(SRC_DIR) -name '*.c')

# Generate matching object file paths inside obj/
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Executable name
EXEC = main

# Default target
all: $(EXEC)

# Link
$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile rule
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(OBJ_DIR) $(EXEC)

.PHONY: all clean
