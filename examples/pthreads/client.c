#include "json_utils.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>

#define BUFFER_SIZE 2048
#define SERVER_PORT 8080

char server_ip[INET_ADDRSTRLEN];

void clean_input(char *input) {
  size_t len = strlen(input);
  if (len > 0 && (input[len - 1] == '\n' || input[len - 1] == '\r')) {
    input[len - 1] = '\0';
  }
  if (len > 1 && (input[len - 2] == '\r')) {
    input[len - 2] = '\0';
  }
}

int validate_and_parse_players(const char *input) {
  for (int i = 0; input[i] != '\0'; i++) {
    if (!isdigit((unsigned char)input[i])) {
      return 2;
    }
  }

  int players = atoi(input);

  if (players < 2)
    return 2;
  if (players > 6)
    return 6;

  return players;
}

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

  printf("Connected to server %s:%d\n", server_ip, SERVER_PORT);
  return sock;
}

void interactive_mode(int sock) {
  fd_set readfds;
  char user_input[512];
  Message msg = {0};
  Message response = {0};

  printf("\n=== WELCOME TO THE PLAYER CLIENT ===\n");
  printf("All messages are shared using the JSON format\n");

  if (receive_json_message(sock, &response)) {
    if (response.type == MSG_SYSTEM && response.text[0]) {
      printf("Server: %s\n", response.text);
    }
  }

  printf("Enter name here: ");
  fgets(user_input, sizeof(user_input), stdin);
  clean_input(user_input);

  if (strlen(user_input) == 0) {
    strcpy(user_input, "Guest");
  } else if (strlen(user_input) > 31) {
    user_input[31] = '\0';
  }

  msg.type = MSG_HELLO;
  strncpy(msg.username, user_input, sizeof(msg.username) - 1);
  send_json_message(sock, MSG_HELLO, &msg);
  printf("Name sent: %s\n", user_input);

  if (receive_json_message(sock, &response)) {
    if (response.type == MSG_SYSTEM && response.text[0]) {
      printf("Server: %s\n", response.text);
    }
  }

  printf("Enter wanted number of players in one room: ");
  fgets(user_input, sizeof(user_input), stdin);
  clean_input(user_input);

  int players = validate_and_parse_players(user_input);
  printf("Value used: %d players\n", players);

  msg.type = MSG_JOIN_ROOM;
  msg.players = players;
  send_json_message(sock, MSG_JOIN_ROOM, &msg);
  printf("Room for %d amount of players requested\n", players);

  if (receive_json_message(sock, &response)) {
    if (response.type == MSG_SYSTEM && response.text[0]) {
      printf("Server: %s\n", response.text);
    }
  }

  printf("\n=== maybe you are in a room, unless there are already more than 10 rooms existing"
         "===\n");
  printf("type something to chat with other roommates\n");
  printf("to exit type 'exit' or click Ctrl+C\n\n");

  while (1) {
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    FD_SET(STDIN_FILENO, &readfds);

    int max_fd = sock > STDIN_FILENO ? sock : STDIN_FILENO;
 
    if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
      perror("select");
      break;
    }

    if (FD_ISSET(STDIN_FILENO, &readfds)) {
      fgets(user_input, sizeof(user_input), stdin);
      clean_input(user_input);

      if (strcmp(user_input, "exit") == 0) {
        printf("Exiting...\n");
        break;
      }

      msg.type = MSG_CHAT;
      strncpy(msg.text, user_input, sizeof(msg.text) - 1);
      send_json_message(sock, MSG_CHAT, &msg);
    }

    if (FD_ISSET(sock, &readfds)) {
      if (receive_json_message(sock, &response)) {
        switch (response.type) {
        case MSG_ROOM_READY:
        case MSG_ROOM_FORWARD:
        case MSG_CHAT:
        case MSG_SYSTEM:
          if (response.text[0]) {
            printf("%s\n", response.text);
          }
          break;
        case MSG_ERROR:
          printf("ERROR: %s\n", response.text);
          break;
        default:
          printf("Unknown type of message\n");
          break;
        }
      } else {
        printf("Connection to the server is lost\n");
        break;
      }
    }
  }
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

  interactive_mode(sock);

  close(sock);
  printf("Client terminated\n");
  return 0;
}
