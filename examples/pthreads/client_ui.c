#include "client_ui.h"
#include "json_utils.h"
#include <ctype.h>
#include <ncurses.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#define INPUT_BUFFER_SIZE 512
#define MAX_MESSAGES 100
#define HISTORY_HEIGHT 20

typedef struct {
  char text[256];
  bool is_system;
  bool is_error;
  char username[32];
} UIMessage;

WINDOW *win_main;
WINDOW *win_input;
WINDOW *win_status;
bool ui_running = false;

char input_buffer[INPUT_BUFFER_SIZE];
int input_pos = 0;
typedef struct {
  int socket_fd;
  char username[32];
  enum { INPUT_NONE, INPUT_USERNAME, INPUT_PLAYERS } input_mode;
} ClientContext;

UIMessage message_history[MAX_MESSAGES];
int message_count = 0;
int scroll_offset = 0;
ClientContext ctx;

void init_windows(void);
void cleanup_windows(void);
void draw_windows(void);
void handle_input(int ch);
void send_current_input(void);
void add_message_to_history(const char *text, bool is_system, bool is_error,
                            const char *username);
void display_messages(void);
void update_status(void);
void process_server_message(void);

bool client_ui_init(int sock) {
  memset(&ctx, 0, sizeof(ctx));
  ctx.socket_fd = sock;
  ctx.input_mode = INPUT_NONE;
  strcpy(ctx.username, "");

  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(1);

  if (has_colors()) {
    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);  // системные сообщения
    init_pair(2, COLOR_CYAN, COLOR_BLACK);   // чат сообщения
    init_pair(3, COLOR_RED, COLOR_BLACK);    // ошибки
    init_pair(4, COLOR_YELLOW, COLOR_BLACK); // статус
    init_pair(5, COLOR_WHITE, COLOR_BLUE);   // ввод
  }

  init_windows();
  refresh();

  ui_running = true;
  return true;
}

void client_ui_run(void) {
  struct pollfd fds[2];
  fds[0].fd = ctx.socket_fd;
  fds[0].events = POLLIN;
  fds[1].fd = STDIN_FILENO;
  fds[1].events = POLLIN;

  while (ui_running) {
    draw_windows();

    int ret = poll(fds, 2, 100); // 100ms timeout

    if (ret > 0) {
      if (fds[1].revents & POLLIN) {
        int ch = getch();
        handle_input(ch);
      }

      if (fds[0].revents & POLLIN) {
        process_server_message();
      }
    } else if (ret < 0) {
      ui_display_error("Poll error");
      break;
    }
  }
}

void client_ui_cleanup(void) {
  cleanup_windows();
  endwin();
}

void ui_display_system_message(const char *message) {
  add_message_to_history(message, true, false, NULL);
}

void ui_display_error(const char *error) {
  add_message_to_history(error, false, true, NULL);
}

void ui_set_input_mode(int mode) {
  ctx.input_mode = mode;
  if (mode == INPUT_USERNAME) {
    strcpy(input_buffer, "Guest");
    input_pos = strlen(input_buffer);
  } else {
    ui_clear_input_buffer();
  }
  update_status();
}

void ui_clear_input_buffer(void) {
  memset(input_buffer, 0, sizeof(input_buffer));
  input_pos = 0;
}

void init_windows(void) {
  int height, width;
  getmaxyx(stdscr, height, width);

  win_main = newwin(height - 3, width, 0, 0);
  scrollok(win_main, TRUE);

  win_status = newwin(1, width, height - 3, 0);

  win_input = newwin(1, width, height - 2, 0);
}

void cleanup_windows(void) {
  delwin(win_main);
  delwin(win_status);
  delwin(win_input);
}

void draw_windows(void) {

  werase(win_main);
  werase(win_status);
  werase(win_input);

  display_messages();

  update_status();

  wattron(win_input, A_REVERSE);
  wprintw(win_input, "> %s", input_buffer);
  wattroff(win_input, A_REVERSE);

  wmove(win_input, 0, input_pos + 2);

  wrefresh(win_main);
  wrefresh(win_status);
  wrefresh(win_input);
}

void handle_input(int ch) {
  switch (ch) {
  case KEY_UP:
    if (scroll_offset < message_count - HISTORY_HEIGHT)
      scroll_offset++;
    break;

  case KEY_DOWN:
    if (scroll_offset > 0)
      scroll_offset--;
    break;

  case KEY_BACKSPACE:
  case 127:
  case '\b':
    if (input_pos > 0) {
      input_buffer[--input_pos] = '\0';
    }
    break;

  case '\n':
  case '\r':
    send_current_input();
    break;

  case KEY_F(1):
  case 27: // ESC
    ui_running = false;
    break;

  default:
    if (isprint(ch) && input_pos < INPUT_BUFFER_SIZE - 1) {
      input_buffer[input_pos++] = ch;
      input_buffer[input_pos] = '\0';
    }
    break;
  }
}

void send_current_input(void) {
  if (input_pos == 0) {
    return;
  }

  Message msg = {0};

  switch (ctx.input_mode) {
  case INPUT_USERNAME:
    if (strlen(input_buffer) == 0) {
      strcpy(ctx.username, "Guest");
    } else
      strncpy(ctx.username, input_buffer, sizeof(ctx.username) - 1);

    msg.type = MSG_HELLO;
    strncpy(msg.username, ctx.username, sizeof(msg.username) - 1);
    send_json_message(ctx.socket_fd, MSG_HELLO, &msg);
    ctx.input_mode = INPUT_NONE;
    break;

  case INPUT_PLAYERS: {
    int players = 2;
    for (int i = 0; input_buffer[i] != '\0'; i++) {
      if (!isdigit((unsigned char)input_buffer[i])) {
        ui_display_error("Invalid number of players");
        ui_clear_input_buffer();
        return;
      }
    }

    players = atoi(input_buffer);
    if (players < 2)
      players = 2;
    if (players > 6)
      players = 6;

    msg.type = MSG_JOIN_ROOM;
    msg.players = players;
    strncpy(msg.username, ctx.username, sizeof(msg.username) - 1);
    send_json_message(ctx.socket_fd, MSG_JOIN_ROOM, &msg);
    ctx.input_mode = INPUT_NONE;
  } break;

  default:
    msg.type = MSG_CHAT;
    strncpy(msg.text, input_buffer, sizeof(msg.text) - 1);
    strncpy(msg.username, ctx.username, sizeof(msg.username) - 1);
    send_json_message(ctx.socket_fd, MSG_CHAT, &msg);
    break;
  }

  ui_clear_input_buffer();
  update_status();
}

void add_message_to_history(const char *text, bool is_system, bool is_error,
                            const char *username) {
  if (message_count >= MAX_MESSAGES) {
    for (int i = 1; i < MAX_MESSAGES; i++) {
      memcpy(&message_history[i - 1], &message_history[i], sizeof(UIMessage));
    }
    message_count--;
  }

  UIMessage *msg = &message_history[message_count++];
  strncpy(msg->text, text, sizeof(msg->text) - 1);
  msg->is_system = is_system;
  msg->is_error = is_error;

  if (username) {
    strncpy(msg->username, username, sizeof(msg->username) - 1);
  } else {
    msg->username[0] = '\0';
  }

  if (scroll_offset > 0) {
    scroll_offset = 0;
  }
}

void display_messages(void) {
  int height, width;
  getmaxyx(win_main, height, width);

  int start = message_count - height - scroll_offset;
  if (start < 0)
    start = 0;
  int end = start + height;
  if (end > message_count)
    end = message_count;

  for (int i = start; i < end; i++) {
    UIMessage *msg = &message_history[i];

    if (msg->is_error) {
      wattron(win_main, COLOR_PAIR(3));
    } else if (msg->is_system) {
      wattron(win_main, COLOR_PAIR(1));
    } else if (msg->username[0]) {
      wattron(win_main, COLOR_PAIR(2));
    }

    if (msg->username[0]) {
      wprintw(win_main, "%s: %s\n", msg->username, msg->text);
    } else {
      wprintw(win_main, "%s\n", msg->text);
    }

    if (msg->is_error || msg->is_system || msg->username[0]) {
      wattroff(win_main, COLOR_PAIR(1));
      wattroff(win_main, COLOR_PAIR(2));
      wattroff(win_main, COLOR_PAIR(3));
    }
  }
}

void update_status(void) {
  wattron(win_status, COLOR_PAIR(4));

  const char *mode_text;
  switch (ctx.input_mode) {
  case INPUT_USERNAME:
    mode_text = "ENTER USERNAME";
    break;
  case INPUT_PLAYERS:
    mode_text = "ENTER NUMBER OF PLAYERS (2-6)";
    break;
  default:
    mode_text = "CHAT MODE";
    break;
  }

  wprintw(win_status, "Status: %s | Messages: %d | Scroll: UP/DOWN | Exit: ESC",
          mode_text, message_count);

  wattroff(win_status, COLOR_PAIR(4));
}

void process_server_message(void) {
  Message response = {0};

  if (!receive_json_message(ctx.socket_fd, &response)) {
    ui_display_error("Connection to the server is lost");
    ui_running = false;
    return;
  }

  switch (response.type) {
  case MSG_SYSTEM:
  case MSG_ROOM_FORWARD:
    if (response.text[0]) {
      ui_display_system_message(response.text);
    }
    break;

  case MSG_ROOM_READY:
  case MSG_CHAT:
    if (response.text[0]) {
      add_message_to_history(response.text, false, false, response.username);
    }
    break;

  case MSG_SYSTEM_HELLO:
    ui_display_system_message(response.text);
    ui_set_input_mode(INPUT_USERNAME);
    break;

  case MSG_SYSTEM_INVITE:
    ui_display_system_message(response.text);
    ui_set_input_mode(INPUT_PLAYERS);
    break;

  case MSG_ERROR:
    ui_display_error(response.text);
    break;

  default:
    ui_display_system_message("Unknown message type");
    break;
  }
}
