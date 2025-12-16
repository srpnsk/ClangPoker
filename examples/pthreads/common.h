#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

#define MAX_CLIENTS 1000
#define MAX_ROOMS 10
#define BUFFER_SIZE 1024
#define SERVER_PORT 8080

// Состояния клиента
typedef enum {
    CLIENT_UNKNOWN,
    CLIENT_IN_LOBBY,
    CLIENT_IN_ROOM
} ClientState;

// Структура клиента (используется в родителе)
typedef struct {
    int socket;
    ClientState state;
    int room_id;           // назначенная комната
    char username[32];     // имя клиента (если нужно)
} Client;

// Структура процесса комнаты
typedef struct {
    pid_t pid;             // PID дочернего процесса
    int control_fd;        // сокет для общения с родителем
    int client_count;
    int room_id;
} RoomProcess;

// Прототипы функций для room.c
void room_process(int room_id, int parent_fd);

#endif // COMMON_H
