#include "json_utils.h"

void send_json_message(int socket, MessageType type, const Message *msg) {
  char *json = build_json_message(type, msg);
  if (!json)
    return;

  size_t len = strlen(json);
  char *json_with_newline = malloc(len + 2);
  strcpy(json_with_newline, json);
  json_with_newline[len] = '\n';
  json_with_newline[len + 1] = '\0';

  send(socket, json_with_newline, len + 1, 0);
  free(json);
  free(json_with_newline);
}

bool receive_json_message(int socket, Message *msg) {
  static char buffer[BUFFER_SIZE];
  static int buffer_pos = 0;

  while (1) {
    for (int i = 0; i < buffer_pos; i++) {
      if (buffer[i] == '\n') {
        buffer[i] = '\0';
        bool result = parse_json_message(buffer, msg);

        memmove(buffer, buffer + i + 1, buffer_pos - i - 1);
        buffer_pos -= i + 1;
        return result;
      }
    }

    if (buffer_pos >= BUFFER_SIZE - 1) {
      buffer_pos = 0;
    }

    ssize_t bytes =
        recv(socket, buffer + buffer_pos, BUFFER_SIZE - buffer_pos - 1, 0);
    if (bytes <= 0) {
      if (bytes == 0)
        return false;
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return false;
      return false;
    }

    buffer_pos += bytes;
    buffer[buffer_pos] = '\0';
  }
}
