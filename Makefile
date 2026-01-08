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
	@echo "  make run<N>     - Lance le noeud N-1 (ex: make run1 -> noeud 0)"
	@echo "  make deploy     - Push vers etape3 et development"
	@echo ""
	@echo "Utilisation:"
	@echo "  1. Compiler: make build"
	@echo "  2. Pour lancer un noeud: make run<N>"
	@echo "     Exemple: make run1 (noeud 0), make run2 (noeud 1), etc."
	@echo "  3. Deploiement: make deploy"
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

# Extraction du numéro depuis le nom de la cible (ex: run1 -> 1)
# On soustrait 1 pour avoir l'ID du noeud (ex: run1 -> noeud 0)
# Si le noeud n'existe pas dans nodes.conf, on l'ajoute automatiquement
run%: $(TARGET_NODE)
	@NODE_ID=$$(($(patsubst run%,%,$@) - 1)); \
	if ! grep -q "^$$NODE_ID " src/nodes.conf; then \
		echo "Node $$NODE_ID non trouve. Ajout automatique..."; \
		MAX_PORT=$$(awk '{print $$3}' src/nodes.conf | sort -n | tail -1); \
		if [ -z "$$MAX_PORT" ]; then MAX_PORT=8999; fi; \
		NEW_PORT=$$(($$MAX_PORT + 1)); \
		echo "" >> src/nodes.conf; \
		echo "$$NODE_ID 127.0.0.1 $$NEW_PORT" >> src/nodes.conf; \
		echo "Ajout de: $$NODE_ID 127.0.0.1 $$NEW_PORT"; \
	fi; \
	echo "Lancement du noeud $$NODE_ID..."; \
	$(TARGET_NODE) $$NODE_ID

deploy:
	@echo "Pushing to etape3..."
	git push origin HEAD:etape3
	@echo "Pushing to development..."
	git push origin HEAD:development

.PHONY: help build clean fclean re run% deploy

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -rf $(BIN_DIR)

re: fclean build