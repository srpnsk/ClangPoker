#include "protocol.h"
#include <stdbool.h>
#include <string.h>

const char *color_to_string(Color color) {
  switch (color) {
  case COL_RED:
    return "RED";
  case COL_YELLOW:
    return "YELLOW";
  case COL_GREEN:
    return "GREEN";
  case COL_BLUE:
    return "BLUE";
  case COL_WILD:
    return "WILD";
  default:
    return "UNKNOWN";
  }
}

Color string_to_color(const char *str) {
  if (!str)
    return COL_WILD;

  if (strcmp(str, "RED") == 0)
    return COL_RED;
  if (strcmp(str, "YELLOW") == 0)
    return COL_YELLOW;
  if (strcmp(str, "GREEN") == 0)
    return COL_GREEN;
  if (strcmp(str, "BLUE") == 0)
    return COL_BLUE;
  if (strcmp(str, "WILD") == 0)
    return COL_WILD;

  return COL_WILD;
}

const char *value_to_string(Value value) {
  switch (value) {
  case VAL_0:
    return "0";
  case VAL_1:
    return "1";
  case VAL_2:
    return "2";
  case VAL_3:
    return "3";
  case VAL_4:
    return "4";
  case VAL_5:
    return "5";
  case VAL_6:
    return "6";
  case VAL_7:
    return "7";
  case VAL_8:
    return "8";
  case VAL_9:
    return "9";
  case VAL_SKIP:
    return "SKIP";
  case VAL_REVERSE:
    return "REVERSE";
  case VAL_DRAW_TWO:
    return "DRAW_TWO";
  case VAL_WILD:
    return "WILD";
  case VAL_WILD_DRAW_FOUR:
    return "WILD_DRAW_FOUR";
  default:
    return "UNKNOWN";
  }
}

Value string_to_value(const char *str) {
  if (!str)
    return VAL_0;

  if (strcmp(str, "0") == 0)
    return VAL_0;
  if (strcmp(str, "1") == 0)
    return VAL_1;
  if (strcmp(str, "2") == 0)
    return VAL_2;
  if (strcmp(str, "3") == 0)
    return VAL_3;
  if (strcmp(str, "4") == 0)
    return VAL_4;
  if (strcmp(str, "5") == 0)
    return VAL_5;
  if (strcmp(str, "6") == 0)
    return VAL_6;
  if (strcmp(str, "7") == 0)
    return VAL_7;
  if (strcmp(str, "8") == 0)
    return VAL_8;
  if (strcmp(str, "9") == 0)
    return VAL_9;
  if (strcmp(str, "SKIP") == 0)
    return VAL_SKIP;
  if (strcmp(str, "REVERSE") == 0)
    return VAL_REVERSE;
  if (strcmp(str, "DRAW_TWO") == 0)
    return VAL_DRAW_TWO;
  if (strcmp(str, "WILD") == 0)
    return VAL_WILD;
  if (strcmp(str, "WILD_DRAW_FOUR") == 0)
    return VAL_WILD_DRAW_FOUR;

  return VAL_0;
}

const char *message_type_to_string(MessageType type) {
  switch (type) {
  case MSG_CLI_HELLO:
    return "CLI_HELLO";
  case MSG_CLI_JOIN:
    return "CLI_JOIN";
  case MSG_CLI_CHAT:
    return "CLI_CHAT";
  case MSG_CLI_GAME:
    return "CLI_GAME";
  case MSG_SER_WELCOME:
    return "SER_WELCOME";
  case MSG_SER_CHAT:
    return "SER_CHAT";
  case MSG_SER_EVENT:
    return "SER_EVENT";
  case MSG_SER_STATE:
    return "SER_STATE";
  case MSG_SER_ERROR:
    return "SER_ERROR";
  default:
    return "INVALID";
  }
}

MessageType string_to_message_type(const char *str) {
  if (!str)
    return MSG_INVALID;

  if (strcmp(str, "CLI_HELLO") == 0)
    return MSG_CLI_HELLO;
  if (strcmp(str, "CLI_JOIN") == 0)
    return MSG_CLI_JOIN;
  if (strcmp(str, "CLI_CHAT") == 0)
    return MSG_CLI_CHAT;
  if (strcmp(str, "CLI_GAME") == 0)
    return MSG_CLI_GAME;
  if (strcmp(str, "SER_WELCOME") == 0)
    return MSG_SER_WELCOME;
  if (strcmp(str, "SER_CHAT") == 0)
    return MSG_SER_CHAT;
  if (strcmp(str, "SER_EVENT") == 0)
    return MSG_SER_EVENT;
  if (strcmp(str, "SER_STATE") == 0)
    return MSG_SER_STATE;
  if (strcmp(str, "SER_ERROR") == 0)
    return MSG_SER_ERROR;

  return MSG_INVALID;
}

const char *client_action_to_string(ClientAction action) {
  switch (action) {
  case ACT_READY:
    return "READY";
  case ACT_PLAY_CARD:
    return "PLAY_CARD";
  case ACT_DRAW_CARD:
    return "DRAW_CARD";
  case ACT_LEAVE_GAME:
    return "LEAVE_GAME";
  default:
    return "NONE";
  }
}

ClientAction string_to_client_action(const char *str) {
  if (!str)
    return ACT_NONE;

  if (strcmp(str, "READY") == 0)
    return ACT_READY;
  if (strcmp(str, "PLAY_CARD") == 0)
    return ACT_PLAY_CARD;
  if (strcmp(str, "DRAW_CARD") == 0)
    return ACT_DRAW_CARD;
  if (strcmp(str, "LEAVE_GAME") == 0)
    return ACT_LEAVE_GAME;

  return ACT_NONE;
}

const char *server_event_to_string(ServerEvent event) {
  switch (event) {
  case EV_PLAYER_LEFT:
    return "PLAYER_LEFT";
  case EV_GAME_STARTED:
    return "GAME_STARTED";
  case EV_GAME_ENDED:
    return "GAME_ENDED";
  case EV_CARD_PLAYED:
    return "CARD_PLAYED";
  case EV_CARD_DRAWN:
    return "CARD_DRAWN";
  case EV_TURN_CHANGED:
    return "TURN_CHANGED";
  case EV_ERROR:
    return "ERROR";
  default:
    return "NONE";
  }
}

ServerEvent string_to_server_event(const char *str) {
  if (!str)
    return EV_NONE;

  if (strcmp(str, "PLAYER_LEFT") == 0)
    return EV_PLAYER_LEFT;
  if (strcmp(str, "GAME_STARTED") == 0)
    return EV_GAME_STARTED;
  if (strcmp(str, "GAME_ENDED") == 0)
    return EV_GAME_ENDED;
  if (strcmp(str, "CARD_PLAYED") == 0)
    return EV_CARD_PLAYED;
  if (strcmp(str, "CARD_DRAWN") == 0)
    return EV_CARD_DRAWN;
  if (strcmp(str, "TURN_CHANGED") == 0)
    return EV_TURN_CHANGED;
  if (strcmp(str, "ERROR") == 0)
    return EV_ERROR;

  return EV_NONE;
}

cJSON *card_to_json(const Card *card) {
  if (!card)
    return NULL;

  cJSON *json = cJSON_CreateObject();
  if (!json)
    return NULL;

  cJSON_AddStringToObject(json, "color", color_to_string(card->color));
  cJSON_AddStringToObject(json, "value", value_to_string(card->value));

  return json;
}

bool json_to_card(const cJSON *json, Card *card) {
  if (!json || !card)
    return false;

  cJSON *color_json = cJSON_GetObjectItem(json, "color");
  cJSON *value_json = cJSON_GetObjectItem(json, "value");

  if (!cJSON_IsString(color_json) || !cJSON_IsString(value_json)) {
    return false;
  }

  card->color = string_to_color(color_json->valuestring);
  card->value = string_to_value(value_json->valuestring);

  return true;
}

char *serialize_msg_cliser(const MsgCliSer *msg) {
  if (!msg)
    return NULL;

  cJSON *root = cJSON_CreateObject();
  if (!root)
    return NULL;

  cJSON_AddStringToObject(root, "type", message_type_to_string(msg->type));

  if (msg->username[0] != '\0') {
    cJSON_AddStringToObject(root, "username", msg->username);
  }

  switch (msg->type) {
  case MSG_CLI_JOIN: {
    cJSON_AddNumberToObject(root, "players", msg->join.players);
    break;
  }
  case MSG_CLI_CHAT: {
    cJSON_AddStringToObject(root, "text", msg->chat.text);
    break;
  }
  case MSG_CLI_GAME: {
    cJSON *game = cJSON_CreateObject();
    cJSON_AddStringToObject(game, "action",
                            client_action_to_string(msg->game.action));

    if (msg->game.action == ACT_PLAY_CARD) {
      cJSON_AddNumberToObject(game, "card_index", msg->game.card_index);
    }

    cJSON_AddItemToObject(root, "game", game);
    break;
  }
  default:
    break;
  }

  char *json_str = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);

  return json_str;
}

bool deserialize_msg_cliser(const char *json_str, MsgCliSer *out) {
  if (!json_str || !out)
    return false;

  memset(out, 0, sizeof(MsgCliSer));

  cJSON *root = cJSON_Parse(json_str);
  if (!root)
    return false;

  cJSON *type_json = cJSON_GetObjectItem(root, "type");
  if (!cJSON_IsString(type_json)) {
    cJSON_Delete(root);
    return false;
  }
  out->type = string_to_message_type(type_json->valuestring);

  cJSON *username_json = cJSON_GetObjectItem(root, "username");
  if (cJSON_IsString(username_json)) {
    strncpy(out->username, username_json->valuestring,
            sizeof(out->username) - 1);
  }

  switch (out->type) {
  case MSG_CLI_JOIN: {
    cJSON *players_json = cJSON_GetObjectItem(root, "players");
    if (cJSON_IsNumber(players_json)) {
      out->join.players = players_json->valueint;
    }
    break;
  }
  case MSG_CLI_CHAT: {
    cJSON *text_json = cJSON_GetObjectItem(root, "text");
    if (cJSON_IsString(text_json)) {
      strncpy(out->chat.text, text_json->valuestring,
              sizeof(out->chat.text) - 1);
    }
    break;
  }
  case MSG_CLI_GAME: {
    cJSON *game_json = cJSON_GetObjectItem(root, "game");
    if (cJSON_IsObject(game_json)) {
      cJSON *action_json = cJSON_GetObjectItem(game_json, "action");
      if (cJSON_IsString(action_json)) {
        out->game.action = string_to_client_action(action_json->valuestring);
      }

      cJSON *card_index_json = cJSON_GetObjectItem(game_json, "card_index");
      if (cJSON_IsNumber(card_index_json)) {
        out->game.card_index = card_index_json->valueint;
      }
    }
    break;
  }
  default:
    break;
  }

  cJSON_Delete(root);
  return true;
}

char *serialize_msg_ser(const MsgSer *msg) {
  if (!msg)
    return NULL;

  cJSON *root = cJSON_CreateObject();
  if (!root)
    return NULL;

  cJSON_AddStringToObject(root, "type", message_type_to_string(msg->type));

  switch (msg->type) {
  case MSG_SER_CHAT: {
    cJSON_AddStringToObject(root, "username", msg->chat.username);
    cJSON_AddStringToObject(root, "text", msg->chat.text);
    break;
  }
  case MSG_SER_EVENT: {
    cJSON *event = cJSON_CreateObject();
    cJSON_AddStringToObject(event, "event",
                            server_event_to_string(msg->event.event));

    switch (msg->event.event) {
    case EV_PLAYER_LEFT: {
      cJSON_AddStringToObject(event, "username", msg->event.player.username);
      break;
    }
    case EV_CARD_PLAYED: {
      cJSON *card_played = cJSON_CreateObject();
      cJSON_AddStringToObject(card_played, "username",
                              msg->event.card_played.username);

      cJSON *card_json = card_to_json(&msg->event.card_played.card);
      if (card_json) {
        cJSON_AddItemToObject(card_played, "card", card_json);
      }

      cJSON_AddItemToObject(event, "data", card_played);
      break;
    }
    case EV_CARD_DRAWN: {
      cJSON *card_drawn = cJSON_CreateObject();
      cJSON_AddStringToObject(card_drawn, "username",
                              msg->event.card_drawn.username);
      cJSON_AddNumberToObject(card_drawn, "count", msg->event.card_drawn.count);
      cJSON_AddItemToObject(event, "data", card_drawn);
      break;
    }
    case EV_TURN_CHANGED: {
      cJSON *turn = cJSON_CreateObject();
      cJSON_AddStringToObject(turn, "current_player",
                              msg->event.turn.current_player);
      cJSON_AddItemToObject(event, "data", turn);
      break;
    }
    case EV_ERROR: {
      cJSON *error = cJSON_CreateObject();
      cJSON_AddStringToObject(error, "message", msg->event.error.message);
      cJSON_AddItemToObject(event, "data", error);
      break;
    }
    default:
      break;
    }

    cJSON_AddItemToObject(root, "event", event);
    break;
  }
  case MSG_SER_STATE: {
    cJSON *state = cJSON_CreateObject();

    cJSON_AddBoolToObject(state, "your_turn", msg->state.your_turn);
    cJSON_AddStringToObject(state, "current_player", msg->state.current_player);
    cJSON_AddNumberToObject(state, "players_in_game",
                            msg->state.players_in_game);

    cJSON *top_card_json = card_to_json(&msg->state.top_card);
    if (top_card_json) {
      cJSON_AddItemToObject(state, "top_card", top_card_json);
    }

    cJSON *hand_array = cJSON_CreateArray();
    if (hand_array) {
      for (int i = 0; i < msg->state.hand_count && i < 20; i++) {
        cJSON *card_json = card_to_json(&msg->state.your_hand[i]);
        if (card_json) {
          cJSON_AddItemToArray(hand_array, card_json);
        }
      }
      cJSON_AddItemToObject(state, "your_hand", hand_array);
      cJSON_AddNumberToObject(state, "hand_count", msg->state.hand_count);
    }

    cJSON_AddItemToObject(root, "state", state);
    break;
  }
  case MSG_SER_ERROR:
  case MSG_SER_WELCOME: {
    cJSON_AddStringToObject(root, "message", msg->system.message);
    break;
  }
  default:
    break;
  }

  char *json_str = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);

  return json_str;
}

bool deserialize_msg_ser(const char *json_str, MsgSer *out) {
  if (!json_str || !out)
    return false;

  memset(out, 0, sizeof(MsgSer));

  cJSON *root = cJSON_Parse(json_str);
  if (!root)
    return false;

  cJSON *type_json = cJSON_GetObjectItem(root, "type");
  if (!cJSON_IsString(type_json)) {
    cJSON_Delete(root);
    return false;
  }
  out->type = string_to_message_type(type_json->valuestring);

  switch (out->type) {
  case MSG_SER_CHAT: {
    cJSON *username_json = cJSON_GetObjectItem(root, "username");
    cJSON *text_json = cJSON_GetObjectItem(root, "text");

    if (cJSON_IsString(username_json)) {
      strncpy(out->chat.username, username_json->valuestring,
              sizeof(out->chat.username) - 1);
    }
    if (cJSON_IsString(text_json)) {
      strncpy(out->chat.text, text_json->valuestring,
              sizeof(out->chat.text) - 1);
    }
    break;
  }
  case MSG_SER_EVENT: {
    cJSON *event_json = cJSON_GetObjectItem(root, "event");
    if (cJSON_IsObject(event_json)) {
      cJSON *event_type_json = cJSON_GetObjectItem(event_json, "event");
      if (cJSON_IsString(event_type_json)) {
        out->event.event = string_to_server_event(event_type_json->valuestring);
      }

      switch (out->event.event) {
      case EV_PLAYER_LEFT: {
        cJSON *username_json = cJSON_GetObjectItem(event_json, "username");
        if (cJSON_IsString(username_json)) {
          strncpy(out->event.player.username, username_json->valuestring,
                  sizeof(out->event.player.username) - 1);
        }
        break;
      }
      case EV_CARD_PLAYED: {
        cJSON *data_json = cJSON_GetObjectItem(event_json, "data");
        if (cJSON_IsObject(data_json)) {
          cJSON *username_json = cJSON_GetObjectItem(data_json, "username");
          if (cJSON_IsString(username_json)) {
            strncpy(out->event.card_played.username, username_json->valuestring,
                    sizeof(out->event.card_played.username) - 1);
          }

          cJSON *card_json = cJSON_GetObjectItem(data_json, "card");
          if (cJSON_IsObject(card_json)) {
            json_to_card(card_json, &out->event.card_played.card);
          }
        }
        break;
      }
      case EV_CARD_DRAWN: {
        cJSON *data_json = cJSON_GetObjectItem(event_json, "data");
        if (cJSON_IsObject(data_json)) {
          cJSON *username_json = cJSON_GetObjectItem(data_json, "username");
          if (cJSON_IsString(username_json)) {
            strncpy(out->event.card_drawn.username, username_json->valuestring,
                    sizeof(out->event.card_drawn.username) - 1);
          }

          cJSON *count_json = cJSON_GetObjectItem(data_json, "count");
          if (cJSON_IsNumber(count_json)) {
            out->event.card_drawn.count = count_json->valueint;
          }
        }
        break;
      }
      case EV_TURN_CHANGED: {
        cJSON *data_json = cJSON_GetObjectItem(event_json, "data");
        if (cJSON_IsObject(data_json)) {
          cJSON *player_json = cJSON_GetObjectItem(data_json, "current_player");
          if (cJSON_IsString(player_json)) {
            strncpy(out->event.turn.current_player, player_json->valuestring,
                    sizeof(out->event.turn.current_player) - 1);
          }
        }
        break;
      }
      case EV_ERROR: {
        cJSON *data_json = cJSON_GetObjectItem(event_json, "data");
        if (cJSON_IsObject(data_json)) {
          cJSON *message_json = cJSON_GetObjectItem(data_json, "message");
          if (cJSON_IsString(message_json)) {
            strncpy(out->event.error.message, message_json->valuestring,
                    sizeof(out->event.error.message) - 1);
          }
        }
        break;
      }
      default:
        break;
      }
    }
    break;
  }
  case MSG_SER_STATE: {
    cJSON *state_json = cJSON_GetObjectItem(root, "state");
    if (cJSON_IsObject(state_json)) {
      cJSON *your_turn_json = cJSON_GetObjectItem(state_json, "your_turn");
      if (cJSON_IsBool(your_turn_json)) {
        out->state.your_turn = cJSON_IsTrue(your_turn_json);
      }

      cJSON *current_player_json =
          cJSON_GetObjectItem(state_json, "current_player");
      if (cJSON_IsString(current_player_json)) {
        strncpy(out->state.current_player, current_player_json->valuestring,
                sizeof(out->state.current_player) - 1);
      }

      cJSON *players_json = cJSON_GetObjectItem(state_json, "players_in_game");
      if (cJSON_IsNumber(players_json)) {
        out->state.players_in_game = players_json->valueint;
      }

      cJSON *top_card_json = cJSON_GetObjectItem(state_json, "top_card");
      if (cJSON_IsObject(top_card_json)) {
        json_to_card(top_card_json, &out->state.top_card);
      }

      cJSON *hand_array = cJSON_GetObjectItem(state_json, "your_hand");
      if (cJSON_IsArray(hand_array)) {
        int array_size = cJSON_GetArraySize(hand_array);
        out->state.hand_count = (array_size < 20) ? array_size : 20;

        for (int i = 0; i < out->state.hand_count; i++) {
          cJSON *card_json = cJSON_GetArrayItem(hand_array, i);
          if (cJSON_IsObject(card_json)) {
            json_to_card(card_json, &out->state.your_hand[i]);
          }
        }
      }

      cJSON *hand_count_json = cJSON_GetObjectItem(state_json, "hand_count");
      if (cJSON_IsNumber(hand_count_json)) {
        out->state.hand_count = hand_count_json->valueint;
      }
    }
    break;
  }
  case MSG_SER_ERROR:
  case MSG_SER_WELCOME: {
    cJSON *message_json = cJSON_GetObjectItem(root, "message");
    if (cJSON_IsString(message_json)) {
      strncpy(out->system.message, message_json->valuestring,
              sizeof(out->system.message) - 1);
    }
    break;
  }
  default:
    break;
  }

  cJSON_Delete(root);
  return true;
}
