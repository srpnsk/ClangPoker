#ifndef CLIENT_NET_H
#define CLIENT_NET_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define INPUT_MAX 256

extern char *server_ip;
extern int sockfd;
extern char game_status[100];
extern bool my_turn;
extern int hand_size;
extern int players_count;
extern int current_room;

// Сетевое подключение
bool connect_to_server(void);
void disconnect_from_server(void);
bool send_to_server(const char *message);
bool receive_from_server(char *buffer, int buffer_size);

#endif
