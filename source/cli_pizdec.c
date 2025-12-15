#include "client_utils.h"
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("UNO Game Client\n");
    printf("Usage: %s <server_ip>\n", argv[0]);
    printf("\nExamples:\n");
    printf("  %s 127.0.0.1          # Local server\n", argv[0]);
    printf("  %s 192.168.1.100      # Server in local network\n", argv[0]);
    printf("  %s server.example.com # Remote server\n", argv[0]);

    exit(0);
  } else {
    server_ip = argv[1];
  }
  char input_buffer[INPUT_MAX] = {0};
  int cursor_pos = 0;
  int input_len = 0;
  static time_t last_time_update = 0;

  setlocale(LC_ALL, "");
  init_signals();

  printf("UNO Game Client\n");
  printf("Connecting to server: %s:%d\n", server_ip, PORT);

  if (!connect_to_server()) {
    fprintf(stderr, "Failed to connect to server %s:%d\n", server_ip, PORT);
    fprintf(stderr, "Make sure server is running and IP address is correct.\n");
    fprintf(stderr, "You can specify IP: %s <server_ip>\n", argv[0]);
    exit(1);
  }

  printf("Connected successfully! Starting UI...\n");
  sleep(1); // Даем время увидеть сообщение

  selected_players = 4; // Значение по умолчанию

  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(1);

  if (has_colors()) {
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_BLUE, COLOR_BLACK);
    init_pair(5, COLOR_WHITE, COLOR_BLACK);
    init_pair(6, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(7, COLOR_CYAN, COLOR_BLACK);
  }

  if (!check_min_size()) {
    wait_for_proper_size();
    if (should_quit) {
      endwin();
      printf("Program terminated.\n");
      return 0;
    }
  }

  add_message("Welcome to UNO Client!", true, false);
  add_message("Use /rooms to see available rooms", true, false);
  add_message("Use /help for list of commands", true, false);

  while (!should_quit) {
    clear();

    if (resized) {
      resized = 0;
      endwin();
      refresh();

      if (!check_min_size()) {
        wait_for_proper_size();
        if (should_quit)
          break;
      }

      clear();
      refresh();
    }

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    // ====== ЭКРАН ВЫБОРА КОЛИЧЕСТВА ИГРОКОВ ======
    if (waiting_for_player_selection) {
      draw_player_selection_screen(rows, cols);

      timeout(-1);
      int ch = getch();

      handle_player_selection(ch);

      continue;
    }

    time_t now = time(NULL);
    if (my_turn && now - last_time_update >= 1) {
      time_left--;
      if (time_left < 0)
        time_left = 0;
      last_time_update = now;
    }

    draw_interface(rows, cols);

    // ====== СПЕЦИАЛЬНЫЙ ЭКРАН ЛОББИ ======
    if (in_lobby && current_room < 0) {
      // Еще не присоединились к комнате
      attron(A_BOLD | COLOR_PAIR(6));
      mvprintw(rows / 2 - 2, cols / 2 - 20,
               "════════════════════════════════════════");
      mvprintw(rows / 2 - 1, cols / 2 - 20,
               "       Finding suitable game room       ");
      mvprintw(rows / 2, cols / 2 - 20,
               "════════════════════════════════════════");
      attroff(A_BOLD | COLOR_PAIR(6));

      mvprintw(rows / 2 + 1, cols / 2 - 25, "Selected: %d players",
               selected_players);
      mvprintw(rows / 2 + 2, cols / 2 - 25, "Waiting for server response...");

    } else if (in_lobby && current_room >= 0) {
      // Присоединились к комнате, ждем игроков
      attron(A_BOLD | COLOR_PAIR(6));
      mvprintw(rows / 2 - 2, cols / 2 - 25,
               "══════════════════════════════════════════════");
      mvprintw(rows / 2 - 1, cols / 2 - 25,
               "   Waiting for %d-player game to start   ", selected_players);
      mvprintw(rows / 2, cols / 2 - 25,
               "══════════════════════════════════════════════");
      attroff(A_BOLD | COLOR_PAIR(6));

      mvprintw(rows / 2 + 1, cols / 2 - 25, "Room: %d | Players: %d/%d",
               current_room, players_count, selected_players);
      mvprintw(rows / 2 + 2, cols / 2 - 25,
               "Type /start to begin or /leave to find another game");
      mvprintw(rows / 2 + 3, cols / 2 - 25, "Waiting for more players...");
    }

    attron(A_BOLD | COLOR_PAIR(7));
    mvprintw(rows - 2, 0, "> ");
    attroff(A_BOLD | COLOR_PAIR(7));

    attron(COLOR_PAIR(5));
    printw("%s", input_buffer);
    attroff(COLOR_PAIR(5));

    move(rows - 2, 2 + cursor_pos);

    refresh();

    char server_buffer[BUFFER_SIZE];
    if (receive_from_server(server_buffer, sizeof(server_buffer))) {
      add_message(server_buffer, true, true);
      parse_game_state(server_buffer);
    }

    timeout(100);
    int ch = getch();
    timeout(-1);

    if (ch == ERR) {
      continue;
    }

    if (ch == KEY_RESIZE) {
      resized = 1;
    } else if (ch == '\n' || ch == '\r') {
      if (strlen(input_buffer) > 0) {
        add_message(input_buffer, false, false);

        if (input_buffer[0] == '/') {
          if (!handle_command(input_buffer)) {
            if (!send_to_server(input_buffer)) {
              add_message("Failed to send command to server", true, false);
              if (sockfd < 0) {
                add_message("Use /connect to reconnect", true, false);
              }
            }
          }
        } else {
          if (!send_to_server(input_buffer)) {
            add_message("Failed to send message to server", true, false);
          }
        }

        memset(input_buffer, 0, INPUT_MAX);
        cursor_pos = 0;
        input_len = 0;
      }
    } else if (ch == KEY_BACKSPACE || ch == 127) {
      if (cursor_pos > 0) {
        memmove(&input_buffer[cursor_pos - 1], &input_buffer[cursor_pos],
                input_len - cursor_pos + 1);
        cursor_pos--;
        input_len--;
      }
    } else if (ch == KEY_LEFT && cursor_pos > 0) {
      cursor_pos--;
    } else if (ch == KEY_RIGHT && cursor_pos < input_len) {
      cursor_pos++;
    } else if (ch == KEY_HOME) {
      cursor_pos = 0;
    } else if (ch == KEY_END) {
      cursor_pos = input_len;
    } else if (ch == KEY_DC && cursor_pos < input_len) {
      memmove(&input_buffer[cursor_pos], &input_buffer[cursor_pos + 1],
              input_len - cursor_pos);
      input_len--;
    } else if (ch == 27) { // ESC
      memset(input_buffer, 0, INPUT_MAX);
      cursor_pos = 0;
      input_len = 0;
    } else if (isprint(ch) && input_len < INPUT_MAX - 1) {
      if (cursor_pos < input_len) {
        memmove(&input_buffer[cursor_pos + 1], &input_buffer[cursor_pos],
                input_len - cursor_pos);
      }
      input_buffer[cursor_pos] = ch;
      cursor_pos++;
      input_len++;
    } else if (ch == 'q' || ch == 'Q') {
      if (cursor_pos == 0) {
        should_quit = 1;
      }
    }
  }

  disconnect_from_server();
  endwin();

  printf("UNO Client terminated successfully.\n");
  return 0;
}
