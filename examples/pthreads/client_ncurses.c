#include "client_ui.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SERVER_PORT 8080

char server_ip[INET_ADDRSTRLEN];

int connect_to_server() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("socket");
    return -1;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);

  if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
    perror("inet_pton");
    close(sock);
    return -1;
  }

  if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("connect");
    close(sock);
    return -1;
  }

  return sock;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <server_ip>\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  snprintf(server_ip, INET_ADDRSTRLEN, "%s", argv[1]);

  printf("=== GAME CLIENT ===\n");
  printf("Connecting to server...\n");

  int sock = connect_to_server();
  if (sock < 0) {
    return 1;
  }

  printf("Connected to server %s:%d\n", server_ip, SERVER_PORT);

  // Инициализация UI после подключения
  if (!client_ui_init(sock)) {
    close(sock);
    return 1;
  }

  // Запуск основного цикла UI
  client_ui_run();

  // Очистка ресурсов
  client_ui_cleanup();
  close(sock);

  printf("Client terminated\n");
  return 0;
}
