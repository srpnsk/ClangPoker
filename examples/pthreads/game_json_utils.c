#include "json_utils.h"

static bool send_json(int socket, const char *json) {
  if (!json || socket < 0)
    return false;

  size_t len = strlen(json);
  char *json_with_newline = malloc(len + 2);
  if (!json_with_newline)
    return false;

  strcpy(json_with_newline, json);
  json_with_newline[len] = '\n';
  json_with_newline[len + 1] = '\0';

  ssize_t sent = send(socket, json_with_newline, len + 1, 0);
  free(json_with_newline);

  return sent > 0;
}

static bool receive_json(int socket, char *output_buffer, size_t buffer_size) {
  if (!output_buffer || buffer_size == 0 || socket < 0)
    return false;

  static char buffer[BUFFER_SIZE];
  static int buffer_pos = 0;

  while (1) {
    for (int i = 0; i < buffer_pos; i++) {
      if (buffer[i] == '\n') {
        size_t msg_len = i;
        if (msg_len >= buffer_size - 1) {
          msg_len = buffer_size - 2;
        }

        memcpy(output_buffer, buffer, msg_len);
        output_buffer[msg_len] = '\0';

        memmove(buffer, buffer + i + 1, buffer_pos - i - 1);
        buffer_pos -= i + 1;

        return true;
      }
    }

    if (buffer_pos >= BUFFER_SIZE - 1) {
      buffer_pos = 0;
      return false;
    }

    ssize_t bytes =
        recv(socket, buffer + buffer_pos, BUFFER_SIZE - buffer_pos - 1, 0);

    if (bytes <= 0) {
      if (bytes == 0) {
        return false;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return false;
      }
      return false;
    }

    buffer_pos += bytes;
    buffer[buffer_pos] = '\0';
  }
}

bool send_server_json(int socket, const MsgSer *msg) {
  if (!msg || socket < 0)
    return false;

  char *json = serialize_msg_ser(msg);
  if (!json)
    return false;

  bool result = send_json(socket, json);
  free(json);

  return result;
}

bool send_server_simple_message(int socket, MessageType type,
                                const char *message) {
  if (!message || socket < 0)
    return false;

  MsgSer msg;
  memset(&msg, 0, sizeof(msg));
  msg.type = type;
  strncpy(msg.system.message, message, sizeof(msg.system.message) - 1);

  return send_server_json(socket, &msg);
}

bool send_server_game_state(int socket, bool your_turn, const Card *top_card,
                            const Card *hand, int hand_count,
                            const char *current_player, int players_in_game) {
  if (!top_card || !hand || !current_player || socket < 0)
    return false;

  MsgSer msg;
  memset(&msg, 0, sizeof(msg));
  msg.type = MSG_SER_STATE;

  msg.state.your_turn = your_turn;
  msg.state.top_card = *top_card;
  msg.state.hand_count = (hand_count < 20) ? hand_count : 20;
  strncpy(msg.state.current_player, current_player,
          sizeof(msg.state.current_player) - 1);
  msg.state.players_in_game = players_in_game;

  for (int i = 0; i < msg.state.hand_count; i++) {
    msg.state.your_hand[i] = hand[i];
  }

  return send_server_json(socket, &msg);
}

bool send_server_event(int socket, ServerEvent event, const char *username,
                       const Card *card, int card_count, const char *message) {
  if (!username || socket < 0)
    return false;

  MsgSer msg;
  memset(&msg, 0, sizeof(msg));
  msg.type = MSG_SER_EVENT;
  msg.event.event = event;

  switch (event) {
  case EV_PLAYER_LEFT:
    strncpy(msg.event.player.username, username,
            sizeof(msg.event.player.username) - 1);
    break;

  case EV_CARD_PLAYED:
    if (card) {
      strncpy(msg.event.card_played.username, username,
              sizeof(msg.event.card_played.username) - 1);
      msg.event.card_played.card = *card;
    }
    break;

  case EV_CARD_DRAWN:
    strncpy(msg.event.card_drawn.username, username,
            sizeof(msg.event.card_drawn.username) - 1);
    msg.event.card_drawn.count = (card_count > 0) ? card_count : 1;
    break;

  case EV_TURN_CHANGED:
    strncpy(msg.event.turn.current_player, username,
            sizeof(msg.event.turn.current_player) - 1);
    break;

  case EV_ERROR:
    if (message) {
      strncpy(msg.event.error.message, message,
              sizeof(msg.event.error.message) - 1);
    }
    break;

  default:
    break;
  }

  return send_server_json(socket, &msg);
}

bool receive_client_json(int socket, MsgCliSer *out) {
  if (!out || socket < 0)
    return false;

  char json_buffer[BUFFER_SIZE];
  if (!receive_json(socket, json_buffer, sizeof(json_buffer))) {
    return false;
  }

  return deserialize_msg_cliser(json_buffer, out);
}

bool send_client_json(int socket, const MsgCliSer *msg) {
  if (!msg || socket < 0)
    return false;

  char *json = serialize_msg_cliser(msg);
  if (!json)
    return false;

  bool result = send_json(socket, json);
  free(json);

  return result;
}

bool send_client_simple_message(int socket, MessageType type,
                                const char *username) {
  if (!username || socket < 0)
    return false;

  MsgCliSer msg;
  memset(&msg, 0, sizeof(msg));
  msg.type = type;
  strncpy(msg.username, username, sizeof(msg.username) - 1);

  return send_client_json(socket, &msg);
}

bool send_client_chat_message(int socket, const char *username,
                              const char *text) {
  if (!username || !text || socket < 0)
    return false;

  MsgCliSer msg;
  memset(&msg, 0, sizeof(msg));
  msg.type = MSG_CLI_CHAT;
  strncpy(msg.username, username, sizeof(msg.username) - 1);
  strncpy(msg.chat.text, text, sizeof(msg.chat.text) - 1);

  return send_client_json(socket, &msg);
}

bool send_client_game_action(int socket, const char *username,
                             ClientAction action, int card_index) {
  if (!username || socket < 0)
    return false;

  MsgCliSer msg;
  memset(&msg, 0, sizeof(msg));
  msg.type = MSG_CLI_GAME;
  strncpy(msg.username, username, sizeof(msg.username) - 1);
  msg.game.action = action;

  if (action == ACT_PLAY_CARD) {
    msg.game.card_index = card_index;
  }

  return send_client_json(socket, &msg);
}

bool send_client_join_game(int socket, const char *username, int players) {
  if (!username || socket < 0)
    return false;

  MsgCliSer msg;
  memset(&msg, 0, sizeof(msg));
  msg.type = MSG_CLI_JOIN;
  strncpy(msg.username, username, sizeof(msg.username) - 1);
  msg.join.players = players;

  return send_client_json(socket, &msg);
}

bool receive_server_json(int socket, MsgSer *out) {
  if (!out || socket < 0)
    return false;

  char json_buffer[BUFFER_SIZE];
  if (!receive_json(socket, json_buffer, sizeof(json_buffer))) {
    return false;
  }

  return deserialize_msg_ser(json_buffer, out);
}
