CC = gcc
CFLAGS = -Wall -g -Iinclude
LDFLAGS = -pthread

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
INC_DIR = include

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))
TARGET = $(BIN_DIR)/cluster

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

.PHONY: all clean fclean re

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -rf $(BIN_DIR)

re: fclean all