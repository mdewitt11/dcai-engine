# Compiler
CC = gcc

# Directories
SRC_DIR = src
OBJ_DIR = obj
GO_BIN_DIR = gobin

# Flags (Notice the new -I$(GO_BIN_DIR) so C can find the Go header!)
CFLAGS = -Wall -Wextra -Iinclude -I$(GO_BIN_DIR) -g -pthread -lm

# ==========================================
# DYNAMIC FILE DISCOVERY
# ==========================================
# 1. Find ALL .c files anywhere inside src/
C_SRCS = $(shell find $(SRC_DIR) -name '*.c')

# 2. Generate matching object file paths inside obj/
C_OBJS = $(C_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# 3. Find ALL .go files anywhere inside src/
GO_SRCS = $(shell find $(SRC_DIR) -name '*.go')

# Executable name
EXEC = dcai

# Default target
all: $(EXEC)

# ==========================================
# 1. GO NETWORK LAYER
# ==========================================
# This rule creates the gobin/ folder and drops the archive and header inside it.
$(GO_BIN_DIR)/libnetwork.a: $(GO_SRCS)
	@echo "=> Building Go Network Layer..."
	@mkdir -p $(GO_BIN_DIR)
	go build -buildmode=c-archive -o $(GO_BIN_DIR)/libnetwork.a $(GO_SRCS)

# ==========================================
# 2. C AI CORE & LINKING
# ==========================================
# Links the final executable. Depends on the Go archive and ALL C objects.
$(EXEC): $(GO_BIN_DIR)/libnetwork.a $(C_OBJS)
	@echo "=> Linking Polyglot Engine..."
	$(CC) $(CFLAGS) $(C_OBJS) $(GO_BIN_DIR)/libnetwork.a -o $@

# Compile rule for C files. 
# It depends on the Go archive first, ensuring the .h file exists before C tries to read it!
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(GO_BIN_DIR)/libnetwork.a
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# ==========================================
# UTILITIES
# ==========================================
clean:
	@echo "=> Cleaning build files..."
	rm -rf $(OBJ_DIR) $(GO_BIN_DIR) $(EXEC)

.PHONY: all clean
