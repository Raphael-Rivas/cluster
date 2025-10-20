CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -pthread
SOURCES = main.c node.c config.c rpc.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = cluster

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all clean fclean re

clean:
	rm -f $(OBJECTS)

fclean: clean
	rm -f $(TARGET)

re: fclean all