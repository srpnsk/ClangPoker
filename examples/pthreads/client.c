#include "json_utils.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFFER_SIZE 2048
#define SERVER_PORT 8080

enum { INPUT_NONE, INPUT_USERNAME, INPUT_PLAYERS } input_mode = INPUT_NONE;

char server_ip[INET_ADDRSTRLEN];

static void out(const char *s) { write(STDOUT_FILENO, s, strlen(s)); }

static void prompt(void) { out("> "); }

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
  struct pollfd fds[2];
  char need_prompt = 0;
  fds[0].fd = sock;
  fds[0].events = POLLIN;

  fds[1].fd = STDIN_FILENO;
  fds[1].events = POLLIN;
  char user_input[512];
  char outbuf[BUFFER_SIZE];
  Message msg = {0};
  Message response = {0};

  while (1) {
    int ret = poll(fds, 2, -1);
    if (ret < 0) {
      perror("poll");
      break;
    }

    if (fds[1].revents & POLLIN) {
      ssize_t n = read(STDIN_FILENO, user_input, sizeof(user_input) - 1);
      if (n <= 0) {
        out("stdin closed\n");
        break;
      }

      user_input[n] = '\0';
      clean_input(user_input);

      if (strcmp(user_input, "exit") == 0) {
        out("Exiting...\n");
        break;
      }

      switch (input_mode) {
      case INPUT_USERNAME:
        if (strlen(user_input) == 0)
          strcpy(user_input, "Guest");
        else if (strlen(user_input) > 31)
          user_input[31] = '\0';

        msg.type = MSG_HELLO;
        strncpy(msg.username, user_input, sizeof(msg.username) - 1);
        send_json_message(sock, MSG_HELLO, &msg);

        input_mode = INPUT_NONE;
        break;

      case INPUT_PLAYERS: {
        int players = validate_and_parse_players(user_input);

        msg.type = MSG_JOIN_ROOM;
        msg.players = players;
        send_json_message(sock, MSG_JOIN_ROOM, &msg);

        input_mode = INPUT_NONE;
        break;
      }

      default:
        msg.type = MSG_CHAT;
        strncpy(msg.text, user_input, sizeof(msg.text) - 1);
        send_json_message(sock, MSG_CHAT, &msg);
        break;
      }
    }

    if (fds[0].revents & POLLIN) {
      if (!receive_json_message(sock, &response)) {
        out("\nConnection to the server is lost\n");
        break;
      }

      switch (response.type) {
      case MSG_SYSTEM:
      case MSG_ROOM_FORWARD:
        if (response.text[0]) {
          snprintf(outbuf, BUFFER_SIZE, "\n%s\n", response.text);
          out(outbuf);
        }
        break;

      case MSG_ROOM_READY:
      case MSG_CHAT:
        if (response.text[0]) {
          snprintf(outbuf, BUFFER_SIZE, "\n%s\n", response.text);
          out(outbuf);
          need_prompt = 1;
        }
        break;

      case MSG_SYSTEM_HELLO:
        snprintf(outbuf, BUFFER_SIZE, "\nServer: %s\n", response.text);
        out(outbuf);
        need_prompt = 1;
        input_mode = INPUT_USERNAME;
        break;

      case MSG_SYSTEM_INVITE:
        snprintf(outbuf, BUFFER_SIZE, "\nServer: %s\n", response.text);
        out(outbuf);
        need_prompt = 1;
        input_mode = INPUT_PLAYERS;
        break;

      case MSG_ERROR:
        snprintf(outbuf, BUFFER_SIZE, "\nERROR: %s\n", response.text);
        out(outbuf);
        need_prompt = 1;
        break;

      default:
        out("\nUnknown message type\n");
        need_prompt = 1;
        break;
      }
    }
    if (need_prompt) {
      prompt();
      need_prompt = 0;
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
