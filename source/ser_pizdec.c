#include "server.h"
int client_count = 0;
Room *rooms = NULL;
pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;
ClientInfo clients[MAX_CONNECTIONS];

void create_uno_deck(Deck *deck) {
  int index = 0;

  for (Color c = COL_RED; c <= COL_BLUE; c++) {
    deck->cards[index++] = (Card){c, VAL_0};

    for (Value v = VAL_1; v <= VAL_9; v++) {
      deck->cards[index++] = (Card){c, v};
      deck->cards[index++] = (Card){c, v};
    }

    for (int i = 0; i < 2; i++) {
      deck->cards[index++] = (Card){c, VAL_SKIP};
      deck->cards[index++] = (Card){c, VAL_REVERSE};
      deck->cards[index++] = (Card){c, VAL_DRAW_TWO};
    }
  }

  for (int i = 0; i < 4; i++)
    deck->cards[index++] = (Card){COL_WILD, VAL_WILD};

  for (int i = 0; i < 4; i++)
    deck->cards[index++] = (Card){COL_WILD, VAL_WILD_DRAW_FOUR};

  deck->top = 0;
}

void shuffle_deck(Deck *deck) {
  for (int i = UNO_DECK_SIZE - 1; i > 0; i--) {
    int j = rand() % (i + 1);
    Card temp = deck->cards[i];
    deck->cards[i] = deck->cards[j];
    deck->cards[j] = temp;
  }
}

Card draw_card(Deck *deck) {
  if (deck->top < UNO_DECK_SIZE) {
    return deck->cards[deck->top++];
  }
  return (Card){COL_WILD, VAL_WILD};
}

const char *color_to_string(Color color) {
  switch (color) {
  case COL_RED:
    return "red";
  case COL_YELLOW:
    return "yellow";
  case COL_GREEN:
    return "green";
  case COL_BLUE:
    return "blue";
  case COL_WILD:
    return "wild";
  default:
    return "unknown";
  }
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
    return "skip";
  case VAL_REVERSE:
    return "reverse";
  case VAL_DRAW_TWO:
    return "draw_two";
  case VAL_WILD:
    return "wild";
  case VAL_WILD_DRAW_FOUR:
    return "wild_draw_four";
  default:
    return "unknown";
  }
}

cJSON *create_card_json(Card card) {
  cJSON *card_obj = cJSON_CreateObject();
  cJSON_AddStringToObject(card_obj, "value", value_to_string(card.value));
  cJSON_AddStringToObject(card_obj, "color", color_to_string(card.color));
  return card_obj;
}

Room *create_room(void) {
  pthread_mutex_lock(&rooms_mutex);

  int active_count = 0;
  Room *current = rooms;
  while (current) {
    if (current->is_active)
      active_count++;
    current = current->next;
  }

  if (active_count >= MAX_ROOMS) {
    pthread_mutex_unlock(&rooms_mutex);
    return NULL;
  }

  Room *new_room = malloc(sizeof(Room));
  if (!new_room) {
    pthread_mutex_unlock(&rooms_mutex);
    return NULL;
  }

  memset(new_room, 0, sizeof(Room));

  static int next_room_id = 0;
  new_room->id = next_room_id++;

  new_room->game.state = WAITING_FOR_PLAYERS;
  new_room->game.direction_clockwise = true;
  new_room->game.players_count = 0;
  new_room->game.draw_accumulator = 0;
  new_room->game.player_to_draw = -1;

  create_uno_deck(&new_room->game.draw_pile);
  shuffle_deck(&new_room->game.draw_pile);
  new_room->game.discard_pile.top = 0;

  new_room->is_active = true;
  new_room->connected_players = 0;
  new_room->last_activity = time(NULL);
  pthread_mutex_init(&new_room->lock, NULL);

  snprintf(new_room->save_file, sizeof(new_room->save_file),
           "room_%d_save.json", new_room->id);

  new_room->next = rooms;
  rooms = new_room;

  pthread_mutex_unlock(&rooms_mutex);
  printf("Created room %d\n", new_room->id);
  return new_room;
}

Room *find_room_by_id(int room_id) {
  pthread_mutex_lock(&rooms_mutex);
  Room *current = rooms;

  while (current) {
    if (current->id == room_id && current->is_active) {
      pthread_mutex_unlock(&rooms_mutex);
      return current;
    }
    current = current->next;
  }

  pthread_mutex_unlock(&rooms_mutex);
  return NULL;
}

Room *find_player_room(int player_fd) {
  pthread_mutex_lock(&rooms_mutex);
  Room *current = rooms;

  while (current) {
    if (current->is_active) {
      for (int i = 0; i < current->game.players_count; i++) {
        if (current->game.players[i].fd == player_fd) {
          pthread_mutex_unlock(&rooms_mutex);
          return current;
        }
      }
    }
    current = current->next;
  }

  pthread_mutex_unlock(&rooms_mutex);
  return NULL;
}

void save_game_state(Room *room) {
  if (!room)
    return;

  pthread_mutex_lock(&room->lock);

  cJSON *root = cJSON_CreateObject();

  cJSON_AddNumberToObject(root, "game_state", room->game.state);
  cJSON_AddNumberToObject(root, "current_player", room->game.current_player);
  cJSON_AddBoolToObject(root, "direction_clockwise",
                        room->game.direction_clockwise);
  cJSON_AddNumberToObject(root, "draw_accumulator",
                          room->game.draw_accumulator);
  cJSON_AddNumberToObject(root, "player_to_draw", room->game.player_to_draw);
  cJSON_AddStringToObject(root, "current_color",
                          color_to_string(room->game.current_color));

  if (room->game.discard_pile.top > 0) {
    cJSON_AddItemToObject(root, "top_card",
                          create_card_json(room->game.top_card));
  }

  cJSON *draw_pile = cJSON_CreateArray();
  for (int i = room->game.draw_pile.top; i < UNO_DECK_SIZE; i++) {
    cJSON_AddItemToArray(draw_pile,
                         create_card_json(room->game.draw_pile.cards[i]));
  }
  cJSON_AddItemToObject(root, "draw_pile", draw_pile);

  cJSON *discard_pile = cJSON_CreateArray();
  for (int i = 0; i < room->game.discard_pile.top; i++) {
    cJSON_AddItemToArray(discard_pile,
                         create_card_json(room->game.discard_pile.cards[i]));
  }
  cJSON_AddItemToObject(root, "discard_pile", discard_pile);

  cJSON *players = cJSON_CreateArray();
  for (int i = 0; i < room->game.players_count; i++) {
    cJSON *player = cJSON_CreateObject();
    cJSON_AddStringToObject(player, "name", room->game.players[i].name);
    cJSON_AddNumberToObject(player, "score", room->game.players[i].score);
    cJSON_AddNumberToObject(player, "hand_size",
                            room->game.players[i].hand_size);
    cJSON_AddBoolToObject(player, "connected", room->game.players[i].connected);

    cJSON *hand = cJSON_CreateArray();
    for (int j = 0; j < room->game.players[i].hand_size; j++) {
      cJSON_AddItemToArray(hand,
                           create_card_json(room->game.players[i].hand[j]));
    }
    cJSON_AddItemToObject(player, "hand", hand);

    cJSON_AddItemToArray(players, player);
  }
  cJSON_AddItemToObject(root, "players", players);

  char *json_str = cJSON_Print(root);
  FILE *fp = fopen(room->save_file, "w");
  if (fp) {
    fwrite(json_str, 1, strlen(json_str), fp);
    fclose(fp);
    printf("Game state saved to %s\n", room->save_file);
  }

  free(json_str);
  cJSON_Delete(root);

  pthread_mutex_unlock(&room->lock);
}

void *timeout_monitor(void *arg) {
  (void)arg;

  while (running) {
    sleep(5);

    pthread_mutex_lock(&rooms_mutex);
    Room *current = rooms;

    while (current) {
      if (current->is_active) {
        time_t now = time(NULL);

        if (current->game.state == IN_PROGRESS) {
          Player *p = &current->game.players[current->game.current_player];
          if (p->is_turn && (now - current->last_activity) > TURN_TIMEOUT) {
            printf("Player %s timed out in room %d\n", p->name, current->id);

            if (current->game.draw_pile.top < UNO_DECK_SIZE &&
                p->hand_size < 20) {
              pthread_mutex_lock(&current->lock);
              Card drawn_card = draw_card(&current->game.draw_pile);
              p->hand[p->hand_size++] = drawn_card;

              char msg[100];
              snprintf(msg, sizeof(msg), "Player %s timed out and drew a card",
                       p->name);
              broadcast_to_room(current, msg, -1);

              next_player_room(current);
              current->last_activity = now;
              pthread_mutex_unlock(&current->lock);
            }
          }
        }

        if (current->connected_players == 0 &&
            (now - current->last_activity) > 300) {
          printf("Removing empty room %d\n", current->id);
          current->is_active = false;
        }
      }
      current = current->next;
    }

    pthread_mutex_unlock(&rooms_mutex);
  }

  return NULL;
}

void broadcast_to_room(Room *room, const char *message, int exclude_fd) {
  if (!room)
    return;

  cJSON *response = cJSON_CreateObject();
  cJSON_AddStringToObject(response, "type", "broadcast");
  cJSON_AddStringToObject(response, "message", message);

  char *json_str = cJSON_PrintUnformatted(response);

  for (int i = 0; i < room->game.players_count; i++) {
    if (room->game.players[i].connected &&
        room->game.players[i].fd != exclude_fd) {
      send(room->game.players[i].fd, json_str, strlen(json_str), 0);
    }
  }

  free(json_str);
  cJSON_Delete(response);
}

void next_player_room(Room *room) {
  if (!room)
    return;

  room->game.players[room->game.current_player].is_turn = false;

  if (room->game.direction_clockwise) {
    room->game.current_player =
        (room->game.current_player + 1) % room->game.players_count;
  } else {
    room->game.current_player =
        (room->game.current_player - 1 + room->game.players_count) %
        room->game.players_count;
  }

  room->game.players[room->game.current_player].is_turn = true;
  room->last_activity = time(NULL);
}

bool can_play_card(Card player_card, Color current_color, Card top_card) {
  if (player_card.color == COL_WILD)
    return true;
  if (player_card.color == current_color)
    return true;
  if (player_card.value == top_card.value)
    return true;
  return false;
}

char *handle_room_command(Room *room, int client_fd, const char *command) {
  if (!room)
    return NULL;

  int player_index = -1;
  for (int i = 0; i < room->game.players_count; i++) {
    if (room->game.players[i].fd == client_fd) {
      player_index = i;
      break;
    }
  }

  if (player_index == -1) {
    cJSON *error = cJSON_CreateObject();
    cJSON_AddStringToObject(error, "error", "Player not found in room");
    char *result = cJSON_Print(error);
    cJSON_Delete(error);
    return result;
  }

  cJSON *response = cJSON_CreateObject();

  if (strcmp(command, "/start") == 0) {
    if (room->game.state == WAITING_FOR_PLAYERS) {
      if (room->game.players_count < MIN_PLAYERS_PER_ROOM) {
        cJSON_AddStringToObject(response, "error",
                                "Need at least 2 players to start");
      } else {
        for (int i = 0; i < room->game.players_count; i++) {
          for (int j = 0; j < 7; j++) {
            room->game.players[i].hand[room->game.players[i].hand_size++] =
                draw_card(&room->game.draw_pile);
          }
        }

        do {
          room->game.top_card = draw_card(&room->game.draw_pile);
        } while (room->game.top_card.color == COL_WILD);

        room->game.discard_pile.cards[0] = room->game.top_card;
        room->game.discard_pile.top = 1;
        room->game.current_color = room->game.top_card.color;

        room->game.current_player = rand() % room->game.players_count;
        room->game.players[room->game.current_player].is_turn = true;
        room->game.state = IN_PROGRESS;

        cJSON_AddStringToObject(response, "result", "success");
        cJSON_AddStringToObject(response, "message", "Game started!");

        char msg[100];
        snprintf(msg, sizeof(msg), "Game started! First player: %s",
                 room->game.players[room->game.current_player].name);
        broadcast_to_room(room, msg, -1);
      }
    } else {
      cJSON_AddStringToObject(response, "error", "Game already in progress");
    }

  } else if (strncmp(command, "/play ", 6) == 0) {
    if (room->game.state != IN_PROGRESS) {
      cJSON_AddStringToObject(response, "error", "Game not in progress");
    } else if (player_index != room->game.current_player) {
      cJSON_AddStringToObject(response, "error", "Not your turn");
    } else {
      int card_index = atoi(command + 6);
      Player *p = &room->game.players[player_index];

      if (card_index < 0 || card_index >= p->hand_size) {
        cJSON_AddStringToObject(response, "error", "Invalid card index");
      } else {
        Card played_card = p->hand[card_index];

        if (!can_play_card(played_card, room->game.current_color,
                           room->game.top_card)) {
          cJSON_AddStringToObject(response, "error", "Card cannot be played");
        } else {
          for (int i = card_index; i < p->hand_size - 1; i++) {
            p->hand[i] = p->hand[i + 1];
          }
          p->hand_size--;

          room->game.top_card = played_card;
          room->game.discard_pile.cards[room->game.discard_pile.top++] =
              played_card;

          if (played_card.color != COL_WILD) {
            room->game.current_color = played_card.color;
          }

          if (played_card.value == VAL_SKIP) {
            next_player_room(room);
          } else if (played_card.value == VAL_REVERSE) {
            room->game.direction_clockwise = !room->game.direction_clockwise;
          } else if (played_card.value == VAL_DRAW_TWO) {
            room->game.draw_accumulator += 2;
            room->game.player_to_draw =
                (room->game.current_player +
                 (room->game.direction_clockwise ? 1 : -1) +
                 room->game.players_count) %
                room->game.players_count;
          } else if (played_card.value == VAL_WILD_DRAW_FOUR) {
            room->game.draw_accumulator += 4;
            room->game.player_to_draw =
                (room->game.current_player +
                 (room->game.direction_clockwise ? 1 : -1) +
                 room->game.players_count) %
                room->game.players_count;
          }

          if (p->hand_size == 0) {
            room->game.state = ROUND_ENDED;
            cJSON_AddBoolToObject(response, "round_won", true);

            char win_msg[100];
            snprintf(win_msg, sizeof(win_msg), "Player %s wins the round!",
                     p->name);
            broadcast_to_room(room, win_msg, -1);
          } else {
            next_player_room(room);
          }

          cJSON_AddStringToObject(response, "result", "success");
          cJSON_AddStringToObject(response, "message", "Card played");
          cJSON_AddItemToObject(response, "card",
                                create_card_json(played_card));

          room->last_activity = time(NULL);
        }
      }
    }

  } else if (strcmp(command, "/draw") == 0) {
    if (room->game.state != IN_PROGRESS) {
      cJSON_AddStringToObject(response, "error", "Game not in progress");
    } else if (player_index != room->game.current_player) {
      cJSON_AddStringToObject(response, "error", "Not your turn");
    } else {
      Player *p = &room->game.players[player_index];

      if (room->game.draw_pile.top < UNO_DECK_SIZE && p->hand_size < 20) {
        Card drawn_card = draw_card(&room->game.draw_pile);
        p->hand[p->hand_size++] = drawn_card;

        cJSON_AddStringToObject(response, "result", "success");
        cJSON_AddStringToObject(response, "message", "Card drawn");
        cJSON_AddItemToObject(response, "card", create_card_json(drawn_card));

        next_player_room(room);
        room->last_activity = time(NULL);
      } else {
        cJSON_AddStringToObject(response, "error", "Cannot draw card");
      }
    }

  } else if (strcmp(command, "/state") == 0) {
    cJSON_AddStringToObject(response, "type", "game_state");
    cJSON_AddNumberToObject(response, "game_state", room->game.state);
    cJSON_AddNumberToObject(response, "current_player",
                            room->game.current_player);
    cJSON_AddBoolToObject(response, "your_turn",
                          player_index == room->game.current_player);

    if (room->game.discard_pile.top > 0) {
      cJSON_AddItemToObject(response, "top_card",
                            create_card_json(room->game.top_card));
    }

    cJSON_AddStringToObject(response, "current_color",
                            color_to_string(room->game.current_color));
    cJSON_AddBoolToObject(response, "direction_clockwise",
                          room->game.direction_clockwise);

    if (room->game.draw_accumulator > 0) {
      cJSON_AddNumberToObject(response, "draw_accumulator",
                              room->game.draw_accumulator);
      cJSON_AddNumberToObject(response, "player_to_draw",
                              room->game.player_to_draw);
    }

    Player *p = &room->game.players[player_index];
    cJSON *hand_array = cJSON_CreateArray();
    for (int i = 0; i < p->hand_size; i++) {
      cJSON_AddItemToArray(hand_array, create_card_json(p->hand[i]));
    }
    cJSON_AddItemToObject(response, "hand", hand_array);
    cJSON_AddNumberToObject(response, "hand_size", p->hand_size);

    cJSON *players_array = cJSON_CreateArray();
    for (int i = 0; i < room->game.players_count; i++) {
      cJSON *player_obj = cJSON_CreateObject();
      cJSON_AddStringToObject(player_obj, "name", room->game.players[i].name);
      cJSON_AddNumberToObject(player_obj, "cards_count",
                              room->game.players[i].hand_size);
      cJSON_AddNumberToObject(player_obj, "score", room->game.players[i].score);
      cJSON_AddBoolToObject(player_obj, "is_current",
                            i == room->game.current_player);
      cJSON_AddBoolToObject(player_obj, "connected",
                            room->game.players[i].connected);
      cJSON_AddItemToArray(players_array, player_obj);
    }
    cJSON_AddItemToObject(response, "players", players_array);

  } else if (strcmp(command, "/hand") == 0) {
    Player *p = &room->game.players[player_index];
    cJSON_AddStringToObject(response, "type", "hand");

    cJSON *hand_array = cJSON_CreateArray();
    for (int i = 0; i < p->hand_size; i++) {
      cJSON *card_obj = cJSON_CreateObject();
      cJSON_AddNumberToObject(card_obj, "index", i);
      cJSON_AddItemToObject(card_obj, "card", create_card_json(p->hand[i]));
      cJSON_AddItemToArray(hand_array, card_obj);
    }
    cJSON_AddItemToObject(response, "hand", hand_array);
    cJSON_AddNumberToObject(response, "hand_size", p->hand_size);

  } else if (strncmp(command, "/color ", 7) == 0) {
    if (room->game.state == IN_PROGRESS &&
        player_index == room->game.current_player) {
      const char *color_str = command + 7;
      Color new_color = COL_WILD;

      if (strcmp(color_str, "red") == 0)
        new_color = COL_RED;
      else if (strcmp(color_str, "yellow") == 0)
        new_color = COL_YELLOW;
      else if (strcmp(color_str, "green") == 0)
        new_color = COL_GREEN;
      else if (strcmp(color_str, "blue") == 0)
        new_color = COL_BLUE;

      if (new_color != COL_WILD) {
        room->game.current_color = new_color;
        cJSON_AddStringToObject(response, "result", "success");
        cJSON_AddStringToObject(response, "message", "Color set");
        cJSON_AddStringToObject(response, "color", color_str);
      } else {
        cJSON_AddStringToObject(response, "error", "Invalid color");
      }
    } else {
      cJSON_AddStringToObject(response, "error", "Cannot set color now");
    }

  } else if (strcmp(command, "/save") == 0) {
    save_game_state(room);
    cJSON_AddStringToObject(response, "result", "success");
    cJSON_AddStringToObject(response, "message", "Game saved");

  } else {
    cJSON_AddStringToObject(response, "error", "Unknown command");
  }

  char *result = cJSON_Print(response);
  cJSON_Delete(response);
  return result;
}

char *handle_client_command(int client_fd, const char *command) {
  Room *room = find_player_room(client_fd);

  cJSON *response = cJSON_CreateObject();

  if (strcmp(command, "/rooms") == 0) {
    cJSON_AddStringToObject(response, "type", "rooms_list");
    cJSON *rooms_array = cJSON_CreateArray();

    pthread_mutex_lock(&rooms_mutex);
    Room *current = rooms;
    while (current) {
      if (current->is_active) {
        cJSON *room_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(room_obj, "id", current->id);
        cJSON_AddNumberToObject(room_obj, "players",
                                current->game.players_count);
        cJSON_AddNumberToObject(room_obj, "max_players", MAX_PLAYERS_PER_ROOM);
        cJSON_AddStringToObject(
            room_obj, "status",
            current->game.state == WAITING_FOR_PLAYERS ? "waiting"
            : current->game.state == IN_PROGRESS       ? "in_progress"
                                                       : "ended");
        cJSON_AddItemToArray(rooms_array, room_obj);
      }
      current = current->next;
    }
    pthread_mutex_unlock(&rooms_mutex);

    cJSON_AddItemToObject(response, "rooms", rooms_array);

  } else if (strncmp(command, "/join ", 6) == 0) {
    int room_id = atoi(command + 6);

    if (room) {
      cJSON_AddStringToObject(response, "error", "Already in a room");
    } else {
      Room *target_room = find_room_by_id(room_id);
      if (!target_room) {
        target_room = create_room();
        if (!target_room) {
          cJSON_AddStringToObject(response, "error", "Cannot create room");
          char *result = cJSON_Print(response);
          cJSON_Delete(response);
          return result;
        }
      }

      if (target_room->game.players_count >= MAX_PLAYERS_PER_ROOM) {
        cJSON_AddStringToObject(response, "error", "Room is full");
      } else {
        pthread_mutex_lock(&target_room->lock);

        struct sockaddr_in addr;
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
          if (clients[i].fd == client_fd) {
            addr = clients[i].addr;
            break;
          }
        }

        Player *p = &target_room->game.players[target_room->game.players_count];
        p->fd = client_fd;
        p->addr = addr;
        p->connected = true;
        p->in_game = true;
        p->hand_size = 0;
        p->score = 0;
        p->is_turn = false;
        p->last_action = time(NULL);
        snprintf(p->name, sizeof(p->name), "Player%d",
                 target_room->game.players_count + 1);

        target_room->game.players_count++;
        target_room->connected_players++;
        target_room->last_activity = time(NULL);

        cJSON_AddStringToObject(response, "result", "success");
        cJSON_AddStringToObject(response, "message", "Joined room");
        cJSON_AddNumberToObject(response, "room_id", target_room->id);
        cJSON_AddStringToObject(response, "player_name", p->name);

        char msg[100];
        snprintf(msg, sizeof(msg), "Player %s joined the room", p->name);
        broadcast_to_room(target_room, msg, client_fd);

        pthread_mutex_unlock(&target_room->lock);
      }
    }

  } else if (strcmp(command, "/leave") == 0) {
    if (room) {
      pthread_mutex_lock(&room->lock);

      for (int i = 0; i < room->game.players_count; i++) {
        if (room->game.players[i].fd == client_fd) {
          char player_name[50];
          strcpy(player_name, room->game.players[i].name);

          for (int j = i; j < room->game.players_count - 1; j++) {
            room->game.players[j] = room->game.players[j + 1];
          }
          room->game.players_count--;
          room->connected_players--;

          if (room->connected_players == 0) {
            room->is_active = false;
            save_game_state(room);
          } else if (room->game.state == IN_PROGRESS) {
            if (room->game.current_player >= room->game.players_count) {
              room->game.current_player = 0;
            }

            if (room->game.players_count == 1) {
              room->game.state = GAME_ENDED;
              char win_msg[100];
              snprintf(win_msg, sizeof(win_msg), "Player %s wins by default!",
                       room->game.players[0].name);
              broadcast_to_room(room, win_msg, -1);
            }
          }

          char msg[100];
          snprintf(msg, sizeof(msg), "Player %s left the room", player_name);
          broadcast_to_room(room, msg, client_fd);

          cJSON_AddStringToObject(response, "result", "success");
          cJSON_AddStringToObject(response, "message", "Left the room");
          break;
        }
      }

      pthread_mutex_unlock(&room->lock);
    } else {
      cJSON_AddStringToObject(response, "error", "Not in a room");
    }

  } else if (strcmp(command, "/help") == 0) {
    cJSON_AddStringToObject(response, "type", "help");

    cJSON *commands = cJSON_CreateArray();
    cJSON_AddItemToArray(commands,
                         cJSON_CreateString("/rooms - List available rooms"));
    cJSON_AddItemToArray(commands,
                         cJSON_CreateString("/join <room_id> - Join a room"));
    cJSON_AddItemToArray(commands,
                         cJSON_CreateString("/leave - Leave current room"));
    cJSON_AddItemToArray(commands,
                         cJSON_CreateString("/start - Start game in room"));
    cJSON_AddItemToArray(commands,
                         cJSON_CreateString("/state - Get game state"));
    cJSON_AddItemToArray(commands, cJSON_CreateString("/draw - Draw a card"));
    cJSON_AddItemToArray(commands,
                         cJSON_CreateString("/play <index> - Play a card"));
    cJSON_AddItemToArray(commands,
                         cJSON_CreateString("/hand - Show your hand"));
    cJSON_AddItemToArray(
        commands,
        cJSON_CreateString("/color <color> - Set color for wild card"));
    cJSON_AddItemToArray(commands,
                         cJSON_CreateString("/save - Save game state"));
    cJSON_AddItemToArray(commands,
                         cJSON_CreateString("/help - Show this help"));

    cJSON_AddItemToObject(response, "commands", commands);

  } else {
    if (room) {
      char *room_response = handle_room_command(room, client_fd, command);
      cJSON_Delete(response);
      return room_response;
    } else {
      cJSON_AddStringToObject(response, "error",
                              "Not in a room. Use /join to join a room first");
    }
  }

  char *result = cJSON_Print(response);
  cJSON_Delete(response);
  return result;
}

void handle_signal(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
    printf("\nShutting down server...\n");
    running = 0;
  }
}

int set_keepalive(int sockfd) {
  int opt = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) == -1) {
    return -1;
  }

  int idle = 60;
  int interval = 10;
  int count = 3;

  setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
  setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
  setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));

  return 0;
}

void close_client(int client_fd, int epoll_fd) {
  Room *room = find_player_room(client_fd);
  if (room) {
    pthread_mutex_lock(&room->lock);

    for (int i = 0; i < room->game.players_count; i++) {
      if (room->game.players[i].fd == client_fd) {
        room->game.players[i].connected = false;
        room->connected_players--;

        char msg[100];
        snprintf(msg, sizeof(msg), "Player %s disconnected",
                 room->game.players[i].name);
        broadcast_to_room(room, msg, client_fd);

        if (room->connected_players == 1 && room->game.state == IN_PROGRESS) {
          for (int j = 0; j < room->game.players_count; j++) {
            if (room->game.players[j].connected) {
              room->game.state = GAME_ENDED;
              char win_msg[100];
              snprintf(win_msg, sizeof(win_msg), "Player %s wins by default!",
                       room->game.players[j].name);
              broadcast_to_room(room, win_msg, -1);
              save_game_state(room);
              break;
            }
          }
        }
        break;
      }
    }

    pthread_mutex_unlock(&room->lock);
  }

  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    if (clients[i].fd == client_fd) {
      clients[i].fd = -1;
      break;
    }
  }

  epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
  close(client_fd);
  client_count--;

  printf("Client disconnected. Total: %d\n", client_count);
}

void cleanup_rooms(void) {
  pthread_mutex_lock(&rooms_mutex);

  Room *prev = NULL;
  Room *current = rooms;

  while (current) {
    if (!current->is_active) {
      save_game_state(current);
      pthread_mutex_destroy(&current->lock);

      if (prev) {
        prev->next = current->next;
        free(current);
        current = prev->next;
      } else {
        rooms = current->next;
        free(current);
        current = rooms;
      }
    } else {
      prev = current;
      current = current->next;
    }
  }

  pthread_mutex_unlock(&rooms_mutex);
}

int main() {
  int server_fd = -1, epoll_fd = -1;
  struct sockaddr_in server_addr;
  struct epoll_event event, events[MAX_EVENTS];

  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    clients[i].fd = -1;
  }

  struct sigaction sa;
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  signal(SIGPIPE, SIG_IGN);

  printf("=== UNO Game Server (Multi-room) ===\n");
  printf("Port: %d\n", PORT);
  printf("Max rooms: %d\n", MAX_ROOMS);
  printf("Players per room: %d-%d\n", MIN_PLAYERS_PER_ROOM,
         MAX_PLAYERS_PER_ROOM);
  printf("Turn timeout: %d seconds\n", TURN_TIMEOUT);
  printf("====================================\n\n");

  srand(time(NULL));

  pthread_t timeout_thread;
  pthread_create(&timeout_thread, NULL, timeout_monitor, NULL);
  pthread_detach(timeout_thread);

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  int opt_reuse = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_reuse,
             sizeof(opt_reuse));
  set_keepalive(server_fd);

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    perror("Bind failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 128) == -1) {
    perror("Listen failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %d\n", PORT);

  if ((epoll_fd = epoll_create1(0)) == -1) {
    perror("epoll_create1 failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  event.events = EPOLLIN;
  event.data.fd = server_fd;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);

  while (running) {
    int event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);

    if (event_count == -1) {
      if (errno == EINTR)
        continue;
      perror("epoll_wait failed");
      break;
    }

    for (int i = 0; i < event_count; i++) {
      if (events[i].events & EPOLLIN) {
        int client_fd = events[i].data.fd;

        if (client_fd == server_fd) {
          struct sockaddr_in client_addr;
          socklen_t client_len = sizeof(client_addr);

          client_fd =
              accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
          if (client_fd == -1) {
            perror("accept failed");
            continue;
          }

          if (client_count >= MAX_CONNECTIONS) {
            printf("Too many connections, rejecting client\n");
            close(client_fd);
            continue;
          }

          int saved = 0;
          for (int j = 0; j < MAX_CONNECTIONS; j++) {
            if (clients[j].fd == -1) {
              clients[j].fd = client_fd;
              clients[j].addr = client_addr;
              saved = 1;
              break;
            }
          }

          if (!saved) {
            printf("No space in clients array, rejecting\n");
            close(client_fd);
            continue;
          }

          set_keepalive(client_fd);

          int flags = fcntl(client_fd, F_GETFL, 0);
          fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

          struct epoll_event client_event;
          client_event.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP;
          client_event.data.fd = client_fd;

          if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) ==
              -1) {
            perror("epoll_ctl failed");
            close(client_fd);
            continue;
          }

          client_count++;

          char client_ip[INET_ADDRSTRLEN];
          inet_ntop(AF_INET, &client_addr.sin_addr, client_ip,
                    sizeof(client_ip));
          printf("New client: FD %d, IP: %s:%d, Total: %d\n", client_fd,
                 client_ip, ntohs(client_addr.sin_port), client_count);

          char *welcome = handle_client_command(client_fd, "/help");
          send(client_fd, welcome, strlen(welcome), 0);
          free(welcome);

        } else {
          char buffer[BUFFER_SIZE];
          ssize_t bytes_received;

          bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);

          if (bytes_received > 0) {
            buffer[bytes_received] = '\0';

            size_t len = strlen(buffer);
            if (len > 0 && buffer[len - 1] == '\n') {
              buffer[len - 1] = '\0';
            }

            printf("Client %d: %s\n", client_fd, buffer);

            char *json_response = handle_client_command(client_fd, buffer);
            if (json_response) {
              send(client_fd, json_response, strlen(json_response), 0);
              free(json_response);
            }

          } else if (bytes_received == 0) {
            printf("Client disconnected: FD %d\n", client_fd);
            close_client(client_fd, epoll_fd);
          } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
              perror("recv failed");
              close_client(client_fd, epoll_fd);
            }
          }
        }
      }

      if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
        int client_fd = events[i].data.fd;
        if (client_fd != server_fd) {
          printf("Client disconnected (event): FD %d\n", client_fd);
          close_client(client_fd, epoll_fd);
        }
      }
    }

    static time_t last_cleanup = 0;
    time_t now = time(NULL);
    if (now - last_cleanup > 60) {
      cleanup_rooms();
      last_cleanup = now;
    }
  }

  printf("\nShutting down server...\n");

  pthread_mutex_lock(&rooms_mutex);
  Room *current = rooms;
  while (current) {
    if (current->is_active) {
      save_game_state(current);
    }
    current = current->next;
  }
  pthread_mutex_unlock(&rooms_mutex);

  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    if (clients[i].fd > 0) {
      close(clients[i].fd);
    }
  }

  if (epoll_fd > 0)
    close(epoll_fd);
  if (server_fd > 0)
    close(server_fd);

  cleanup_rooms();

  printf("Server stopped. Total clients served: %d\n", client_count);
  return 0;
}
