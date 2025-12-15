#include "client_utils.h"

void handle_sigwinch(int sig) {
  (void)sig;
  resized = 1;
}

void init_signals(void) {
  struct sigaction sa;
  sa.sa_handler = handle_sigwinch;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGWINCH, &sa, NULL);
}

bool check_min_size(void) {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  return (cols >= MIN_WIDTH && rows >= MIN_HEIGHT);
}

void show_size_warning(void) {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  clear();
  attron(A_BOLD | A_REVERSE);
  mvprintw(rows / 2 - 2, cols / 2 - 18, "TERMINAL TOO SMALL!");
  attroff(A_BOLD | A_REVERSE);

  mvprintw(rows / 2, cols / 2 - 12, "Current: %d x %d", cols, rows);
  mvprintw(rows / 2 + 1, cols / 2 - 12, "Required: %d x %d", MIN_WIDTH,
           MIN_HEIGHT);
  mvprintw(rows / 2 + 3, cols / 2 - 24, "Resize terminal or press 'q' to quit");

  refresh();
}

void wait_for_proper_size(void) {
  int ch;
  show_size_warning();
  timeout(100);

  while (1) {
    if (resized) {
      resized = 0;
      endwin();
      refresh();
      clear();

      if (check_min_size()) {
        timeout(-1);
        return;
      }
      show_size_warning();
    }

    ch = getch();
    if (ch == 'q' || ch == 'Q') {
      should_quit = 1;
      return;
    } else if (ch == ERR) {
      continue;
    }
  }
}

void add_message(const char *text, bool from_server, bool is_json) {
  if (message_count < 100) {
    strncpy(message_history[message_count].text, text, BUFFER_SIZE - 1);
    message_history[message_count].text[BUFFER_SIZE - 1] = '\0';
    message_history[message_count].from_server = from_server;
    message_history[message_count].is_json = is_json;
    message_history[message_count].timestamp = time(NULL);
    message_count++;
  } else {
    for (int i = 1; i < 100; i++) {
      message_history[i - 1] = message_history[i];
    }
    strncpy(message_history[99].text, text, BUFFER_SIZE - 1);
    message_history[99].text[BUFFER_SIZE - 1] = '\0';
    message_history[99].from_server = from_server;
    message_history[99].is_json = is_json;
    message_history[99].timestamp = time(NULL);
  }
}

void add_game_action(const char *player, const char *action, Card card) {
  if (game_history_count < 100) {
    strncpy(game_history[game_history_count].player_name, player, 49);
    strncpy(game_history[game_history_count].action, action, 99);
    game_history[game_history_count].card = card;
    game_history[game_history_count].timestamp = time(NULL);
    game_history_count++;
  } else {
    for (int i = 1; i < 100; i++) {
      game_history[i - 1] = game_history[i];
    }
    strncpy(game_history[99].player_name, player, 49);
    strncpy(game_history[99].action, action, 99);
    game_history[99].card = card;
    game_history[99].timestamp = time(NULL);
  }
}

void parse_game_state(const char *json_string) {
  cJSON *root = cJSON_Parse(json_string);
  if (!root) {
    add_message("Failed to parse JSON", true, false);
    return;
  }

  cJSON *type = cJSON_GetObjectItem(root, "type");
  if (type) {
    const char *type_str = cJSON_GetStringValue(type);

    if (strcmp(type_str, "game_state") == 0) {
      cJSON *game_state = cJSON_GetObjectItem(root, "game_state");
      cJSON *current_player = cJSON_GetObjectItem(root, "current_player");
      cJSON *your_turn = cJSON_GetObjectItem(root, "your_turn");
      cJSON *top_card_obj = cJSON_GetObjectItem(root, "top_card");
      cJSON *current_color_obj = cJSON_GetObjectItem(root, "current_color");
      cJSON *direction = cJSON_GetObjectItem(root, "direction_clockwise");
      cJSON *hand = cJSON_GetObjectItem(root, "hand");
      cJSON *players_array = cJSON_GetObjectItem(root, "players");

      if (game_state) {
        int state = cJSON_GetNumberValue(game_state);
        switch (state) {
        case 0:
          strcpy(game_status, "Waiting for players");
          break;
        case 1:
          strcpy(game_status, "Game in progress");
          break;
        case 2:
          strcpy(game_status, "Round ended");
          break;
        case 3:
          strcpy(game_status, "Game ended");
          break;
        }
      }

      if (your_turn) {
        my_turn = cJSON_IsTrue(your_turn);
        if (my_turn)
          strcat(game_status, " - YOUR TURN!");
      }

      if (top_card_obj) {
        cJSON *color = cJSON_GetObjectItem(top_card_obj, "color");
        cJSON *value = cJSON_GetObjectItem(top_card_obj, "value");
        if (color && value) {
          top_card.color = parse_color(cJSON_GetStringValue(color));
          top_card.value = parse_value(cJSON_GetStringValue(value));
        }
      }

      if (current_color_obj) {
        current_color = parse_color(cJSON_GetStringValue(current_color_obj));
      }

      if (direction) {
        direction_clockwise = cJSON_IsTrue(direction);
      }

      if (hand && cJSON_IsArray(hand)) {
        hand_size = 0;
        cJSON *card_item;
        cJSON_ArrayForEach(card_item, hand) {
          if (hand_size < 20) {
            cJSON *color = cJSON_GetObjectItem(card_item, "color");
            cJSON *value = cJSON_GetObjectItem(card_item, "value");
            if (color && value) {
              player_hand[hand_size].color =
                  parse_color(cJSON_GetStringValue(color));
              player_hand[hand_size].value =
                  parse_value(cJSON_GetStringValue(value));
              hand_size++;
            }
          }
        }
      }

      if (players_array && cJSON_IsArray(players_array)) {
        players_count = 0;
        cJSON *player_item;
        cJSON_ArrayForEach(player_item, players_array) {
          if (players_count < 6) {
            cJSON *name = cJSON_GetObjectItem(player_item, "name");
            cJSON *cards = cJSON_GetObjectItem(player_item, "cards_count");
            cJSON *score = cJSON_GetObjectItem(player_item, "score");
            cJSON *is_current = cJSON_GetObjectItem(player_item, "is_current");
            cJSON *connected = cJSON_GetObjectItem(player_item, "connected");

            if (name && cards && score) {
              strncpy(players[players_count].name, cJSON_GetStringValue(name),
                      49);
              players[players_count].name[49] = '\0';
              players[players_count].cards_count = cJSON_GetNumberValue(cards);
              players[players_count].score = cJSON_GetNumberValue(score);
              players[players_count].is_current = cJSON_IsTrue(is_current);
              players[players_count].connected = cJSON_IsTrue(connected);
              players_count++;
            }
          }
        }
      }

    } else if (strcmp(type_str, "rooms_list") == 0) {
      cJSON *rooms_array = cJSON_GetObjectItem(root, "rooms");
      if (rooms_array && cJSON_IsArray(rooms_array)) {
        add_message("=== Available Rooms ===", true, false);

        cJSON *room_item;
        cJSON_ArrayForEach(room_item, rooms_array) {
          cJSON *id = cJSON_GetObjectItem(room_item, "id");
          cJSON *players = cJSON_GetObjectItem(room_item, "players");
          cJSON *max = cJSON_GetObjectItem(room_item, "max_players");
          cJSON *status = cJSON_GetObjectItem(room_item, "status");

          if (id && players && max && status) {
            char room_info[100];
            snprintf(
                room_info, sizeof(room_info), "Room %d: %d/%d players (%s)",
                (int)cJSON_GetNumberValue(id),
                (int)cJSON_GetNumberValue(players),
                (int)cJSON_GetNumberValue(max), cJSON_GetStringValue(status));
            add_message(room_info, true, false);
          }
        }
        add_message("Use /join <room_id> to join", true, false);
      }

    } else if (strcmp(type_str, "broadcast") == 0) {
      cJSON *message = cJSON_GetObjectItem(root, "message");
      if (message) {
        add_message(cJSON_GetStringValue(message), true, false);
      }

    } else if (strcmp(type_str, "help") == 0) {
      cJSON *commands = cJSON_GetObjectItem(root, "commands");
      if (commands && cJSON_IsArray(commands)) {
        add_message("=== Available Commands ===", true, false);
        cJSON *command_item;
        cJSON_ArrayForEach(command_item, commands) {
          add_message(cJSON_GetStringValue(command_item), true, false);
        }
      }
    }
  }

  cJSON *result = cJSON_GetObjectItem(root, "result");
  cJSON *error = cJSON_GetObjectItem(root, "error");
  cJSON *message = cJSON_GetObjectItem(root, "message");

  if (result && strcmp(cJSON_GetStringValue(result), "success") == 0 &&
      message) {
    add_message(cJSON_GetStringValue(message), true, false);

    if (strstr(cJSON_GetStringValue(message), "Joined room") != NULL) {
      cJSON *room_id = cJSON_GetObjectItem(root, "room_id");
      cJSON *player_name_json = cJSON_GetObjectItem(root, "player_name");
      if (room_id) {
        current_room = cJSON_GetNumberValue(room_id);
        if (player_name_json) {
          strncpy(player_name, cJSON_GetStringValue(player_name_json), 49);
          player_name[49] = '\0';
        }
      }
    }
  } else if (error) {
    add_message(cJSON_GetStringValue(error), true, false);
  } else if (message) {
    add_message(cJSON_GetStringValue(message), true, false);
  }

  cJSON_Delete(root);
}

bool handle_command(const char *command) {
  if (strcmp(command, "/connect") == 0) {
    if (connect_to_server()) {
      add_message("Connected to UNO server", true, false);
    } else {
      add_message("Failed to connect to server", true, false);
    }
    return true;

  } else if (strcmp(command, "/disconnect") == 0) {
    disconnect_from_server();
    add_message("Disconnected from server", true, false);
    return true;

  } else if (strcmp(command, "/clear") == 0) {
    message_count = 0;
    game_history_count = 0;
    return true;

  } else if (strcmp(command, "/quit") == 0) {
    should_quit = 1;
    return true;
  }

  return false;
}

void print_card_fancy(int x, int y, Card card, int index, bool highlight) {
  const char *value_symbols[] = {" 0 ", " 1 ", " 2 ", " 3 ", " 4 ",
                                 " 5 ", " 6 ", " 7 ", " 8 ", " 9 ",
                                 "SKP", "RVS", "+2 ", "WLD", "+4 "};

  char color_char;
  int color_pair;

  switch (card.color) {
  case COL_RED:
    color_char = 'R';
    color_pair = COLOR_PAIR(1) | A_BOLD;
    break;
  case COL_YELLOW:
    color_char = 'Y';
    color_pair = COLOR_PAIR(2) | A_BOLD;
    break;
  case COL_GREEN:
    color_char = 'G';
    color_pair = COLOR_PAIR(3) | A_BOLD;
    break;
  case COL_BLUE:
    color_char = 'B';
    color_pair = COLOR_PAIR(4) | A_BOLD;
    break;
  case COL_WILD:
    color_char = 'W';
    color_pair = COLOR_PAIR(5) | A_BOLD;
    break;
  default:
    color_char = '?';
    color_pair = A_NORMAL;
    break;
  }

  if (highlight) {
    color_pair |= A_REVERSE;
  }

  attron(color_pair);
  mvprintw(y, x, "╔═══════╗");
  mvprintw(y + 1, x, "║%c  %2d ║", color_char, index);
  mvprintw(y + 2, x, "║  %s  ║", value_symbols[card.value]);
  mvprintw(y + 3, x, "║      %c║", color_char);
  mvprintw(y + 4, x, "╚═══════╝");
  attroff(color_pair);
}

void draw_player_hand(int start_y, int start_x, int width) {
  if (hand_size == 0) {
    mvprintw(start_y, start_x, "No cards in hand");
    return;
  }

  attron(A_BOLD | COLOR_PAIR(6));
  mvprintw(start_y, start_x, "Your Hand (%d cards):", hand_size);
  attroff(A_BOLD | COLOR_PAIR(6));

  int cards_per_row = (width - 2) / 9;
  if (cards_per_row < 1)
    cards_per_row = 1;

  int row = 0;
  int col = 0;

  for (int i = 0; i < hand_size; i++) {
    int x = start_x + col * 9;
    int y = start_y + 2 + row * 6;

    print_card_fancy(x, y, player_hand[i], i, false);

    col++;
    if (col >= cards_per_row) {
      col = 0;
      row++;
    }
  }
}

void draw_message_history(int start_row, int end_row, int width) {
  int rows_available = end_row - start_row - 1;
  int start_index =
      (message_count > rows_available) ? message_count - rows_available : 0;

  int y = start_row;
  for (int i = start_index; i < message_count && y < end_row; i++) {
    char time_str[20];
    struct tm *tm_info = localtime(&message_history[i].timestamp);
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

    if (message_history[i].from_server) {
      if (message_history[i].is_json) {
        attron(COLOR_PAIR(3) | A_BOLD);
      } else {
        attron(COLOR_PAIR(3));
      }
      mvprintw(y, 2, "[%s] ", time_str);
    } else {
      attron(COLOR_PAIR(4));
      mvprintw(y, 2, "[%s] ", time_str);
    }

    char display[width - 20];
    strncpy(display, message_history[i].text, sizeof(display) - 1);
    display[sizeof(display) - 1] = '\0';

    if (strlen(message_history[i].text) > sizeof(display) - 1) {
      strcat(display, "...");
    }

    printw("%s", display);

    attroff(COLOR_PAIR(3) | COLOR_PAIR(4) | A_BOLD);
    y++;
  }
}

void draw_players(int start_y, int start_x, int width) {
  attron(A_BOLD | COLOR_PAIR(7));
  mvprintw(start_y, start_x, "Players:");
  attroff(A_BOLD | COLOR_PAIR(7));

  for (int i = 0; i < players_count && i < 6; i++) {
    int y = start_y + 1 + i;

    if (players[i].is_current) {
      attron(A_BOLD | COLOR_PAIR(2));
      mvprintw(y, start_x, "→ ");
      attroff(A_BOLD | COLOR_PAIR(2));
    } else {
      mvprintw(y, start_x, "  ");
    }

    if (!players[i].connected) {
      attron(COLOR_PAIR(1));
    }

    char player_info[width - 4];
    snprintf(player_info, sizeof(player_info), "%s: %d cards, %d points",
             players[i].name, players[i].cards_count, players[i].score);
    printw("%s", player_info);

    if (!players[i].connected) {
      attroff(COLOR_PAIR(1));
      printw(" (disconnected)");
    }
  }
}

void draw_game_state(int start_y, int start_x, int width) {
  attron(A_BOLD | COLOR_PAIR(6));
  mvprintw(start_y, start_x, "Game State:");
  attroff(A_BOLD | COLOR_PAIR(6));

  if (top_card.color != COL_WILD || top_card.value != VAL_WILD) {
    mvprintw(start_y + 1, start_x, "Top card: ");
    print_card_fancy(start_x + 10, start_y + 1, top_card, -1, false);
  }

  const char *color_names[] = {"Red", "Yellow", "Green", "Blue", "Wild"};
  mvprintw(start_y + 6, start_x, "Current color: %s",
           color_names[current_color]);

  mvprintw(start_y + 7, start_x, "Direction: %s",
           direction_clockwise ? "Clockwise" : "Counter-clockwise");

  if (my_turn) {
    attron(A_BOLD | COLOR_PAIR(2));
    mvprintw(start_y + 8, start_x, "Time left: %d seconds", time_left);
    attroff(A_BOLD | COLOR_PAIR(2));
  }
}

void draw_interface(int rows, int cols) {
  attron(A_BOLD | COLOR_PAIR(6));
  mvprintw(0, 2, "UNO Game Client");
  attroff(A_BOLD | COLOR_PAIR(6));

  mvprintw(1, 2, "Server: ");
  attron(COLOR_PAIR(4));
  printw("%s:%d", server_ip, PORT);
  attroff(COLOR_PAIR(4));

  printw(" | Status: ");
  if (sockfd >= 0) {
    attron(COLOR_PAIR(3) | A_BOLD);
    printw("CONNECTED");
    attroff(COLOR_PAIR(3) | A_BOLD);
  } else {
    attron(COLOR_PAIR(1) | A_BOLD);
    printw("DISCONNECTED");
    attroff(COLOR_PAIR(1) | A_BOLD);
  }

  printw(" | Room: ");
  if (current_room >= 0) {
    attron(COLOR_PAIR(2));
    printw("%d", current_room);
    attroff(COLOR_PAIR(2));
  } else {
    printw("None");
  }

  printw(" | Player: %s | %s", player_name, game_status);

  mvprintw(2, 2, "Commands: ");
  attron(A_BOLD);
  printw("/rooms, /join N, /leave, /start, /state, /draw, /play N, /hand, "
         "/color, /help");
  attroff(A_BOLD);

  attron(COLOR_PAIR(5));
  for (int i = 0; i < cols; i++) {
    mvaddch(3, i, ACS_HLINE);
  }
  attroff(COLOR_PAIR(5));

  int middle = cols / 2;

  attron(A_BOLD);
  mvprintw(4, 2, "Game Log:");
  attroff(A_BOLD);
  draw_message_history(5, rows / 2, middle - 4);

  draw_game_state(4, middle + 2, cols - middle - 4);
  draw_players(rows / 4, middle + 2, cols - middle - 4);

  for (int i = 0; i < cols; i++) {
    mvaddch(rows / 2, i, ACS_HLINE);
  }

  draw_player_hand(rows / 2 + 1, 2, cols - 4);

  for (int i = 0; i < cols; i++) {
    mvaddch(rows - 3, i, ACS_HLINE);
  }
}
