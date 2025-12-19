#include "client_ui.h"
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SERVER_PORT 8080

int connect_to_server(const char *host, int port) {
  struct addrinfo hints = {0}, *res, *rp;
  int sock = -1;
  char port_str[16];

  snprintf(port_str, sizeof(port_str), "%d", port);

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  int err = getaddrinfo(host, port_str, &hints, &res);
  if (err != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
    return -1;
  }

  for (rp = res; rp != NULL; rp = rp->ai_next) {
    sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sock == -1)
      continue;

    if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0)
      break;

    close(sock);
    sock = -1;
  }

  freeaddrinfo(res);

  if (sock == -1) {
    perror("connect");
    return -1;
  }

  return sock;
}

int main(int argc, char *argv[]) {
  char server_ip[INET_ADDRSTRLEN];
  if (argc < 2) {
    printf("Usage: %s <server_ip>\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  snprintf(server_ip, INET_ADDRSTRLEN, "%s", argv[1]);

  printf("=== GAME CLIENT ===\n");
  printf("Connecting to server...\n");

  int sock = connect_to_server(server_ip, SERVER_PORT);
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
