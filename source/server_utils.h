#ifndef SERVER_UTILS_H
#define SERVER_UTILS_H

#include "server.h"
#include "uno_rules.h"

extern int client_count;
extern Room *rooms;
extern pthread_mutex_t rooms_mutex;
extern ClientInfo clients[MAX_CONNECTIONS];

// Утилиты для работы с JSON
cJSON *create_card_json(Card card);
void save_game_state(Room *room);

// Работа с комнатами
Room *create_room(void);
Room *find_room_by_id(int room_id);
Room *find_player_room(int player_fd);
Room *find_room_by_desired_players(int desired_players);
void *timeout_monitor(void *arg);
void broadcast_to_room(Room *room, const char *message, int exclude_fd);
void next_player_room(Room *room);

// Обработка команд
char *handle_client_command(int client_fd, const char *command);
char *handle_room_command(Room *room, int client_fd, const char *command);

// Работа с клиентами
int set_keepalive(int sockfd);
void close_client(int client_fd, int epoll_fd);
void cleanup_rooms(void);
void handle_signal(int sig);

#endif
