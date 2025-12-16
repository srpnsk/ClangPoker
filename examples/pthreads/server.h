#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include "utils.h"

// Глобальные переменные сервера
extern RoomProcess rooms[MAX_ROOMS];
extern Client clients[MAX_CLIENTS];
extern int client_count;

// Функции обработки состояний
void handle_unknown(int client_fd);
int handle_lobby(int client_fd);
void transfer_to_room(int client_fd, int room_id);
int create_room_process(int room_id);
RoomProcess *find_room_process(int room_id);

// Вспомогательные функции
int find_free_client_slot();
void remove_client(int client_fd);

int run_server();

#endif // SERVER_H
