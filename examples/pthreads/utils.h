#ifndef UTILS_H
#define UTILS_H

#include "common.h"

// Передача файлового дескриптора между процессами
int send_fd(int socket, int fd_to_send);
int receive_fd(int socket);

// Создание серверного сокета
int create_server_socket(int port);

// Установка сокета в неблокирующий режим
int set_nonblocking(int fd);

// Создание пары сокетов для IPC
int create_socket_pair(int pair[2]);

#endif // UTILS_H
