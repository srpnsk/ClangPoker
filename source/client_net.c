#include "client_net.h"

char *server_ip = "127.0.0.1"; // значение по умолчанию
int sockfd = -1;
char game_status[100] = "Disconnected";
bool my_turn = false;
int hand_size = 0;
int players_count = 0;
int current_room = -1;

bool connect_to_server(void) {
  struct sockaddr_in server_addr;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("socket() failed");
    return false;
  }

  int opt = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct timeval tv;
  tv.tv_sec = 5;
  tv.tv_usec = 0;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);

  if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
    perror("inet_pton() failed");
    close(sockfd);
    sockfd = -1;
    return false;
  }

  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("connect() failed");
    close(sockfd);
    sockfd = -1;
    return false;
  }

  strcpy(game_status, "Connected to server");
  return true;
}

void disconnect_from_server(void) {
  if (sockfd >= 0) {
    close(sockfd);
    sockfd = -1;
  }
  strcpy(game_status, "Disconnected");
  current_room = -1;
  hand_size = 0;
  players_count = 0;
  my_turn = false;
}

bool send_to_server(const char *message) {
  if (sockfd < 0) {
    return false;
  }

  char buffer[INPUT_MAX + 2];
  snprintf(buffer, sizeof(buffer), "%s\n", message);

  int bytes_sent = send(sockfd, buffer, strlen(buffer), 0);
  return bytes_sent > 0;
}

bool receive_from_server(char *buffer, int buffer_size) {
  if (sockfd < 0) {
    return false;
  }

  struct pollfd pfd;
  pfd.fd = sockfd;
  pfd.events = POLLIN;

  int result = poll(&pfd, 1, 0);

  if (result > 0 && (pfd.revents & POLLIN)) {
    int bytes_received = recv(sockfd, buffer, buffer_size - 1, 0);
    if (bytes_received > 0) {
      buffer[bytes_received] = '\0';
      return true;
    } else if (bytes_received == 0) {
      disconnect_from_server();
      return false;
    }
  }

  return false;
}
