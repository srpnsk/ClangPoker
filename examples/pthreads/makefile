CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = tcp_server.out
OBJS = main.o server.o room.o utils.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

main.o: main.c common.h server.h
	$(CC) $(CFLAGS) -c main.c

server.o: server.c common.h utils.h server.h
	$(CC) $(CFLAGS) -c server.c

room.o: room.c common.h utils.h
	$(CC) $(CFLAGS) -c room.c

utils.o: utils.c utils.h common.h
	$(CC) $(CFLAGS) -c utils.c

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
