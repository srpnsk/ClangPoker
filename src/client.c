#include "msg.h"
#include <errno.h>
#include <fcntl.h>
#include <ncurses.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MIN_WIDTH 80
#define MIN_HEIGHT 24
#define CHAT_HISTORY 200

volatile sig_atomic_t running = 1;
volatile sig_atomic_t resized = 0;

typedef enum {
  UI_ENTER_NAME,
  UI_LOBBY_DECISION,
  UI_CHOOSE_PARTICIPANTS,
  UI_CHOOSE_ROOM,
  UI_CHAT,
  UI_ERROR,
  UI_EXIT,
  UI_WAIT_SIZE
} ui_state_t;
ui_state_t ui_state;
ui_state_t backup_ui_state;

typedef struct {
  int total_rooms;
  struct {
    int room_id;
    int current_participants;
    int capacity;
    char users[7][USERNAME];
  } room_info[10];
} rooms_info_t;
rooms_info_t rooms_to_print = {0};

typedef struct {
  char lines[CHAT_HISTORY][TEXT];
  int count;
  int scroll;
} chat_history_t;

chat_history_t chat = {0};

bool registered;
char last_ui_error[TEXT] = {[0 ... 1023] = '\0'};

static int selected_room = 0;
static int selected_participants = 0;
static int selected_decision = 0;

static char chat_input[TEXT] = {0};
static int chat_len = 0;
static int chat_cursor = 0;

static char input[USERNAME] = "Guest";
static int len = 5;
static int cursor_pos = 5;

void handle_signal(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
    running = 0;
  }
}

void handle_sigwinch(int sig) {
  (void)sig;
  resized = 1;
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
  mvprintw(rows / 2 + 3, cols / 2 - 24, "Resize terminal or press ESC to quit");

  refresh();
}

void init_signals(void) {
  struct sigaction sa;
  sa.sa_handler = handle_sigwinch;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGWINCH, &sa, NULL);

  struct sigaction act;
  sigemptyset(&act.sa_mask);
  act.sa_handler = handle_signal;
  act.sa_flags = 0;
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGTERM, &act, NULL);
}

void dispatch_server_msg(message *msg) {
  switch (msg->type) {
  case MSG_HELLO:
    registered = false;
    if (ui_state == UI_WAIT_SIZE)
      backup_ui_state = UI_ENTER_NAME;
    else
      ui_state = UI_ENTER_NAME;
    break;
  case MSG_CHAT:
    if (chat.count < CHAT_HISTORY) {
      snprintf(chat.lines[chat.count], TEXT, "%s", msg->text);
      chat.count++;
    } else {
      /* простой сдвиг вверх */
      memmove(chat.lines, chat.lines + 1,
              sizeof(chat.lines[0]) * (CHAT_HISTORY - 1));
      snprintf(chat.lines[CHAT_HISTORY - 1], TEXT, "%s", msg->text);
    }
    if (ui_state == UI_WAIT_SIZE)
      backup_ui_state = UI_CHAT;
    else
      ui_state = UI_CHAT;
    break;
  case MSG_ERR:
    strncpy(last_ui_error, msg->text, TEXT);
    break;
  case MSG_LIST_ROOMS: {
    memset(&rooms_to_print, 0, sizeof(rooms_to_print));
    uint8_t *p = (uint8_t *)msg->text;
    uint8_t room_count = *p++;
    if (!room_count) {
      uint8_t can_create = *p++;
      if (!can_create) {
        if (ui_state == UI_WAIT_SIZE)
          backup_ui_state = UI_ERROR;
        else
          ui_state = UI_ERROR;
      } else {
        if (ui_state == UI_WAIT_SIZE)
          backup_ui_state = UI_CHOOSE_PARTICIPANTS;
        else
          ui_state = UI_CHOOSE_PARTICIPANTS;
      }
      break;
    }
    rooms_to_print.total_rooms = room_count;
    for (int i = 0; i < room_count; i++) {
      uint8_t id = *p++;
      uint8_t cur = *p++;
      uint8_t max = *p++;

      rooms_to_print.room_info[i].room_id = id;
      rooms_to_print.room_info[i].current_participants = cur;
      rooms_to_print.room_info[i].capacity = max;

      for (int u = 0; u < cur; u++) {
        char name[USERNAME];
        memcpy(name, p, USERNAME);
        name[USERNAME - 1] = '\0';
        p += USERNAME;
        memcpy(rooms_to_print.room_info[i].users[u], name, USERNAME);
      }
    }
    selected_room = 0;
    if (ui_state == UI_CHOOSE_ROOM)
      ;
    else if (ui_state == UI_WAIT_SIZE)
      backup_ui_state = UI_LOBBY_DECISION;
    else
      ui_state = UI_LOBBY_DECISION;
    break;
  }
  default:
    ui_state = UI_EXIT;
    break;
  }
}

void ui_render(void) {
  if (resized) {
    resized = 0;
    endwin();
    refresh();
    clear();
  }

  clear();
  switch (ui_state) {
  case UI_ENTER_NAME:
    mvprintw(1, 2, "Press ESC to exit app");
    mvprintw(2, 2, "Enter your name:");
    mvprintw(4, 2, "> ");
    mvprintw(4, 4, "%s", input);
    move(4, 4 + cursor_pos);
    break;

  case UI_LOBBY_DECISION:
    mvprintw(1, 2, "Press ESC to exit app");
    mvprintw(2, 2, "Lobby:");
    mvprintw(4, 4, "1) Join existing room");
    mvprintw(5, 4, "2) Create new room");
    mvprintw(7, 2, "Choice: %d", selected_decision + 1);
    move(4 + selected_decision, 4);
    break;

  case UI_CHOOSE_PARTICIPANTS:
    mvprintw(1, 2, "ESC to exit app; F1 to return to choise");
    mvprintw(2, 2, "Choose number of participants (2 - 7):");
    mvprintw(4, 2, "2 3 4 5 6 7");
    mvprintw(3, 2, "2-7 to select an option; ENTER to confirm your choise");
    move(4, 2 + (selected_participants * 2));
    break;

  case UI_CHOOSE_ROOM: {
    mvprintw(1, 2, "ESC to exit app; F1 to return to choise.");
    mvprintw(2, 2, "Available rooms:");
    mvprintw(4, 2, "Up/Down/0-9 to select, Enter to join");

    int y = 5;
    for (int i = 0; i < rooms_to_print.total_rooms; i++) {

      if (i == selected_room) {
        attron(A_REVERSE);
      }

      mvprintw(y + i, 4, "[%d] Room #%d  (%d/%d)", i,
               rooms_to_print.room_info[i].room_id,
               rooms_to_print.room_info[i].current_participants,
               rooms_to_print.room_info[i].capacity);

      attroff(A_REVERSE);

      int x = 35;
      for (int u = 0; u < rooms_to_print.room_info[i].current_participants;
           u++) {
        mvprintw(y + i, x, "%s", rooms_to_print.room_info[i].users[u]);
        x += strlen(rooms_to_print.room_info[i].users[u]) + 1;
      }
    }
    move(y + selected_room, 5);
    break;
  }
  case UI_CHAT: {
    mvprintw(2, 2, "Chat:");
    mvprintw(1, 2, "F1 - leave room; ESC - exit app; ENTER - to send message");
    int max_lines = LINES - 5;
    int start = chat.count - 1 - chat.scroll;
    int y = 3;

    for (int i = 0; i < max_lines; i++) {
      int idx = start - i;
      if (idx < 0)
        break;
      mvprintw(y + (max_lines - 1 - i), 2, "%s", chat.lines[idx]);
    }
    mvprintw(LINES - 2, 2, "> ");
    if (chat.scroll == 0) {
      curs_set(1);
      mvprintw(LINES - 2, 4, "%s", chat_input);
      move(LINES - 2, 4 + chat_cursor);
    } else
      curs_set(0);
    break;
  }
  case UI_ERROR:
    attron(A_BOLD | A_REVERSE);
    mvprintw(LINES / 2, (COLS - 20) / 2, "SERVER ERROR");
    attroff(A_BOLD | A_REVERSE);
    mvprintw(LINES / 2 + 2, (COLS - 28) / 2, "Press any key to exit");
    mvprintw(LINES / 2 + 4, (COLS - 30) / 2, "%s", last_ui_error);
    break;
  case UI_WAIT_SIZE:
    if (check_min_size()) {
      ui_state = backup_ui_state;
    } else {
      show_size_warning();
    }
    break;

  default:
    break;
  }
  refresh();
}

void ui_handle_input(int sock) {
  int ch = getch();
  if (ch == ERR)
    return;
  if (ch == KEY_RESIZE) {
    resized = 1;
  }
  if (ui_state == UI_ERROR) {
    ui_state = UI_EXIT;
    return;
  }
  if (ch == 27) { // ESC
    message *exit_request = malloc(sizeof(message));
    init_msg(exit_request);
    exit_request->type = MSG_EXIT;
    write(sock, exit_request, sizeof(message));
    free(exit_request);
    exit_request = NULL;
    ui_state = UI_EXIT;
  }
  switch (ui_state) {
  case UI_CHOOSE_ROOM:
    if (ch == KEY_UP) {
      if (selected_room > 0)
        selected_room--;
      else
        selected_room = rooms_to_print.total_rooms - 1;
    } else if (ch == KEY_DOWN) {
      if (selected_room < rooms_to_print.total_rooms - 1)
        selected_room++;
      else
        selected_room = 0;
    } else if (ch >= '0' && ch <= '9') {
      int idx = ch - '0';
      if (idx < rooms_to_print.total_rooms)
        selected_room = idx;
    } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
      message msg;
      init_msg(&msg);
      msg.type = MSG_JOIN_ROOM;
      msg.room_id = rooms_to_print.room_info[selected_room].room_id;
      write(sock, &msg, sizeof(message));
      ui_state = UI_CHAT;
    } else if (ch == KEY_F(1)) {
      if (ui_state == UI_WAIT_SIZE)
        backup_ui_state = UI_LOBBY_DECISION;
      else
        ui_state = UI_LOBBY_DECISION;
    }
    break;
  case UI_ENTER_NAME: {
    if (registered)
      break;
    if ((ch == '\n') || (ch == '\r') || (ch == KEY_ENTER)) {
      if (len > 0) {
        message msg;
        init_msg(&msg);
        msg.type = MSG_HELLO;
        strncpy(msg.username, input, USERNAME - 1);
        msg.username[USERNAME - 1] = '\0';
        write(sock, &msg, sizeof(message));
        len = 0;
        cursor_pos = 0;
        memset(input, 0, USERNAME);
        registered = true;
      }
    } else if (ch == KEY_BACKSPACE || ch == 127) {
      if (cursor_pos > 0) {
        memmove(&input[cursor_pos - 1], &input[cursor_pos],
                len - cursor_pos + 1);
        cursor_pos--;
        len--;
      } else {
        input[0] = '\0';
      }
    } else if (ch == KEY_DC && cursor_pos < len) {
      memmove(&input[cursor_pos], &input[cursor_pos + 1], len - cursor_pos);
      len--;
    } else if (ch == KEY_LEFT) {
      if (cursor_pos > 0)
        cursor_pos--;
    } else if (ch == KEY_RIGHT) {
      if (cursor_pos < len)
        cursor_pos++;
    } else if (ch == KEY_HOME) {
      cursor_pos = 0;
    } else if (ch == KEY_END) {
      cursor_pos = len;
    } else if (len < USERNAME - 1 && ch >= 32 && ch < 127) {
      if (cursor_pos < len) {
        memmove(&input[cursor_pos + 1], &input[cursor_pos], len - cursor_pos);
      }
      input[cursor_pos] = ch;
      cursor_pos++;
      len++;
      input[len] = '\0';
    }
    break;
  }

  case UI_LOBBY_DECISION:
    if (ch == '1') {
      selected_decision = 0;
    } else if (ch == '2') {
      selected_decision = 1;
    } else if ((ch == '\n') || (ch == '\r') || (ch == KEY_ENTER)) {
      if (!selected_decision) {
        if (ui_state == UI_WAIT_SIZE)
          backup_ui_state = UI_CHOOSE_ROOM;
        else
          ui_state = UI_CHOOSE_ROOM;
      } else {
        if (ui_state == UI_WAIT_SIZE)
          backup_ui_state = UI_CHOOSE_PARTICIPANTS;
        else
          ui_state = UI_CHOOSE_PARTICIPANTS;
      }
    }
    break;

  case UI_CHOOSE_PARTICIPANTS:
    if (ch >= '2' && ch <= '7') {
      selected_participants = ch - '2';
    } else if (ch == KEY_RIGHT) {
      selected_participants = (selected_participants + 1) % 6;
    } else if (ch == KEY_LEFT) {
      if ((selected_participants - 1) < 0)
        selected_participants = 5;
      else
        selected_participants--;
    } else if ((ch == '\n') || (ch == '\r') || (ch == KEY_ENTER)) {
      message msg;
      init_msg(&msg);
      msg.type = MSG_JOIN_ROOM;
      msg.participants = selected_participants + 2;
      write(sock, &msg, sizeof(message));
      ui_state = UI_CHAT;
    } else if (ch == KEY_F(1)) {
      if (ui_state == UI_WAIT_SIZE)
        backup_ui_state = UI_LOBBY_DECISION;
      else
        ui_state = UI_LOBBY_DECISION;
    }
    break;

  case UI_CHAT: {
    if ((ch == '\n') || (ch == '\r') || (ch == KEY_ENTER)) {
      if (chat_len > 0) {
        message msg;
        init_msg(&msg);
        msg.type = MSG_CHAT;
        strncpy(msg.text, chat_input, TEXT - 1);
        msg.text[TEXT - 1] = '\0';
        write(sock, &msg, sizeof(message));
        chat_len = 0;
        chat_cursor = 0;
        chat_input[0] = '\0';
      }
    } else if (ch == KEY_F(1)) {
      message msg;
      init_msg(&msg);
      msg.type = MSG_LEAVE_ROOM;
      write(sock, &msg, sizeof(message));

      chat.count = 0;
      chat.scroll = 0;

      break;
    } else if (ch == KEY_BACKSPACE || ch == 127) {
      if (chat_cursor > 0) {
        memmove(&chat_input[chat_cursor - 1], &chat_input[chat_cursor],
                chat_len - chat_cursor + 1);
        chat_cursor--;
        chat_len--;
      }
    } else if (ch == KEY_DC && chat_cursor < chat_len) {
      memmove(&chat_input[chat_cursor], &chat_input[chat_cursor + 1],
              chat_len - chat_cursor);
      chat_len--;
    } else if (ch == KEY_LEFT) {
      if (chat_cursor > 0)
        chat_cursor--;
    } else if (ch == KEY_RIGHT) {
      if (chat_cursor < chat_len)
        chat_cursor++;
    } else if (chat_len < TEXT - 1 && ch >= 32 && ch < 127) {
      chat.scroll = 0;
      if (chat_cursor < chat_len) {
        memmove(&chat_input[chat_cursor + 1], &chat_input[chat_cursor],
                chat_len - chat_cursor);
      }
      chat_input[chat_cursor] = ch;
      chat_cursor++;
      chat_len++;
      chat_input[chat_len] = '\0';
    } else if (ch == KEY_UP) {
      if (chat.count > 0) {
        int max_scroll = chat.count - 1;
        if (chat.scroll < max_scroll)
          chat.scroll++;
      }
    } else if (ch == KEY_DOWN)
      if (chat.scroll > 0)
        chat.scroll--;

    break;
  }
  case UI_WAIT_SIZE:
    break;
  default:
    break;
  }
  refresh();
}

void init_curses(void) {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(1);
  nodelay(stdscr, TRUE);
  refresh();
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <server hostname>\n", argv[0]);
    return EXIT_FAILURE;
  }
  init_signals();

  struct addrinfo hints, *res, *rp;
  memset(&hints, 0, sizeof(hints));
  int sock = -1;
  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%d", PORT);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  int err = getaddrinfo(argv[1], port_str, &hints, &res);
  if (err != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
    return -1;
  }

  for (rp = res; rp != NULL; rp = rp->ai_next) {
    sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sock == -1)
      continue;
    if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0)
      break;
    close(sock);
    sock = -1;
  }
  freeaddrinfo(res);
  if (sock == -1) {
    perror("connect");
    return -1;
  }
  fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);

  struct pollfd pfd = (struct pollfd){.fd = sock, .events = POLLIN};

  init_curses();

  while (running && (ui_state != UI_EXIT)) {
    if (resized) {
      resized = 0;
      endwin();
      refresh();
      clear();

      if (!check_min_size()) {
        if (ui_state != UI_WAIT_SIZE)
          backup_ui_state = ui_state;
        ui_state = UI_WAIT_SIZE;
      } else
        ui_state = backup_ui_state;
    }
    ui_render();
    int ready = poll(&pfd, 1, 50);
    if (ready < 0) {
      if (errno == EINTR)
        continue;
      perror("poll");
      break;
    }
    if (pfd.revents & POLLIN) {
      message *msg = malloc(sizeof(message));
      ssize_t r = read(pfd.fd, msg, sizeof(message));
      if (r != sizeof(message)) {
        ui_state = UI_EXIT;
        continue;
      }
      dispatch_server_msg(msg);
      free(msg);
      msg = NULL;
    }
    if (pfd.revents & (POLLHUP | POLLERR)) {
      ui_state = UI_EXIT;
      continue;
    }
    ui_handle_input(pfd.fd);
  }

  close(sock);
  endwin();
  return 0;
}
