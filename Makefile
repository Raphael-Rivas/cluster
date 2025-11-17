# Compilateur et flags
CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude
LDFLAGS = -pthread

# Répertoires
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
INC_DIR = include

# Fichiers
SOURCES = $(wildcard $(SRC_DIR)/*.c)
HEADERS = $(wildcard $(INC_DIR)/*.h)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))
TARGET = $(BIN_DIR)/cluster

# Couleurs pour l'affichage
GREEN = \033[0;32m
YELLOW = \033[0;33m
RED = \033[0;31m
NC = \033[0m # No Color

# Aide
help:
	@echo "$(GREEN)===== Makefile - Système de Cluster =====$(NC)"
	@echo ""
	@echo "$(YELLOW)Cibles disponibles:$(NC)"
	@echo "  make compile  - Compile le projet"
	@echo "  make run0     - Lance le nœud 0"
	@echo "  make run1     - Lance le nœud 1"
	@echo "  make run2     - Lance le nœud 2"
	@echo "  make runall   - Lance tous les nœuds en parallèle"
	@echo "  make killall  - Arrête tous les nœuds"
	@echo "  make clean    - Supprime les fichiers objets"
	@echo "  make fclean   - Supprime tout (objets + exécutable)"
	@echo "  make re       - Recompile tout"
	@echo "  make info     - Affiche les informations du projet"
	@echo "  make help     - Affiche cette aide"
	@echo ""
	@echo "$(YELLOW)Exemple d'utilisation:$(NC)"
	@echo "  make compile  # Compile"
	@echo "  make run0     # Lance le nœud 0 dans un terminal"
	@echo "  make run1     # Lance le nœud 1 dans un autre terminal"
	@echo "  make run2     # Lance le nœud 2 dans un autre terminal"
	@echo ""

# Règle par défaut
compile: $(TARGET)
	@echo "$(GREEN)✓ Compilation terminée avec succès$(NC)"
	@echo "$(YELLOW)Utilisation: ./$(TARGET) <node_id>$(NC)"

# Compilation de l'exécutable
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	@echo "$(GREEN)Linking...$(NC)"
	$(CC) -o $@ $^ $(LDFLAGS)

# Compilation des fichiers objets avec dépendances aux headers
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS) | $(OBJ_DIR)
	@echo "$(GREEN)Compiling $<...$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

# Création des répertoires
$(BIN_DIR) $(OBJ_DIR):
	@mkdir -p $@

run0: $(TARGET)
	@echo "$(YELLOW)Démarrage du nœud 0...$(NC)"
	./$(TARGET) 0

run1: $(TARGET)
	@echo "$(YELLOW)Démarrage du nœud 1...$(NC)"
	./$(TARGET) 1

run2: $(TARGET)
	@echo "$(YELLOW)Démarrage du nœud 2...$(NC)"
	./$(TARGET) 2

# Lancement de tous les nœuds en parallèle (pour tests)
runall: $(TARGET)
	@echo "$(YELLOW)Démarrage de tous les nœuds en parallèle...$(NC)"
	@./$(TARGET) 0 & ./$(TARGET) 1 & ./$(TARGET) 2 &
	@echo "$(GREEN)Tous les nœuds sont démarrés. Utilisez 'make killall' pour arrêter.$(NC)"

# Arrêter tous les processus cluster
killall:
	@echo "$(YELLOW)Arrêt de tous les nœuds...$(NC)"
	@pkill -f "$(TARGET)" || true
	@echo "$(GREEN)Tous les nœuds ont été arrêtés$(NC)"

# Nettoyage des fichiers objets
clean:
	@echo "$(RED)Nettoyage des fichiers objets...$(NC)"
	rm -rf $(OBJ_DIR)
	@echo "$(GREEN)✓ Nettoyage terminé$(NC)"

# Nettoyage complet
fclean: clean
	@echo "$(RED)Nettoyage complet...$(NC)"
	rm -rf $(BIN_DIR)
	@echo "$(GREEN)✓ Nettoyage complet terminé$(NC)"

# Recompilation complète
re: fclean all

# Afficher les informations du projet
info:
	@echo "$(GREEN)===== Informations du Projet =====$(NC)"
	@echo "Nom: Système de Cluster Distribué"
	@echo "Exécutable: $(TARGET)"
	@echo "Sources: $(SOURCES)"
	@echo "Headers: $(HEADERS)"
	@echo "Compilateur: $(CC)"
	@echo "Flags: $(CFLAGS)"
	@echo ""

.PHONY: all clean fclean re run run0 run1 run2 runall killall info help