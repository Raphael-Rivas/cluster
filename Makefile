CC = gcc
CFLAGS = -Wall -g -Iinclude
LDFLAGS = -pthread

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
INC_DIR = include

SOURCES_NODE = $(SRC_DIR)/main.c $(SRC_DIR)/node.c $(SRC_DIR)/rpc.c $(SRC_DIR)/config.c
OBJECTS_NODE = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES_NODE))

SOURCES_CLIENT = $(SRC_DIR)/client.c
OBJECTS_CLIENT = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES_CLIENT))

TARGET_NODE = $(BIN_DIR)/cluster
TARGET_CLIENT = $(BIN_DIR)/raftclient

.DEFAULT_GOAL := help

help:
	@echo "=========================================="
	@echo "  CLUSTER - Makefile Help"
	@echo "=========================================="
	@echo ""
	@echo "Compilation:"
	@echo "  make build      - Compile le projet (cluster + client)"
	@echo "  make re         - Recompile tout"
	@echo "  make clean      - Nettoie les fichiers objets"
	@echo "  make fclean     - Nettoie tout (objets + executable)"
	@echo ""
	@echo "Execution:"
	@echo "  make run1       - Lance le noeud 0"
	@echo "  make run2       - Lance le noeud 1"
	@echo "  make run3       - Lance le noeud 2"
	@echo ""
	@echo "Utilisation:"
	@echo "  1. Compiler: make build"
	@echo "  2. Ouvrir 3 terminaux"
	@echo "  3. Terminal 1: make run1"
	@echo "  4. Terminal 2: make run2"
	@echo "  5. Terminal 3: make run3"
	@echo ""

build: $(TARGET_NODE) $(TARGET_CLIENT)

$(TARGET_NODE): $(OBJECTS_NODE) | $(BIN_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TARGET_CLIENT): $(OBJECTS_CLIENT) | $(BIN_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

run1: $(TARGET_NODE)
	@echo "Lancement du noeud 0..."
	@$(TARGET_NODE) 0

run2: $(TARGET_NODE)
	@echo "Lancement du noeud 1..."
	@$(TARGET_NODE) 1

run3: $(TARGET_NODE)
	@echo "Lancement du noeud 2..."
	@$(TARGET_NODE) 2

.PHONY: help build clean fclean re run1 run2 run3

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -rf $(BIN_DIR)

re: fclean build