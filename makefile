CC = gcc
CFLAGS = -Wall
LDFLAGS = 
SERVER_SOURCES = src/server.c src/msg.c src/utils.c
CLIENT_SOURCES = src/client.c src/msg.c
SERVER_TARGET = server.out
CLIENT_TARGET = client.out

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(SERVER_TARGET): $(SERVER_SOURCES)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(CLIENT_TARGET): $(CLIENT_SOURCES)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lncurses

clean:
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET)

.PHONY: all clean
