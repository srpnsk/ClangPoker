#include <locale.h>
#include <ncurses.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>

#define MIN_WIDTH 80
#define MIN_HEIGHT 24
#define INPUT_MAX 256

static volatile sig_atomic_t resized = 0;
static int should_quit = 0;

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
  mvprintw(rows / 2 + 3, cols / 2 - 24, "Resize terminal or press ESC to quit");

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
    if (ch == 27) {
      should_quit = 1;
      return;
    } else if (ch == ERR) {
      continue;
    }
  }
}

int main(void) {
  char input_buffer[INPUT_MAX] = {0};
  int cursor_pos = 0;
  int input_len = 0;

  setlocale(LC_ALL, "");

  init_signals();

  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(1);

  if (has_colors()) {
    start_color();
    init_pair(1, COLOR_CYAN, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    attron(COLOR_PAIR(1));
  }

  if (!check_min_size()) {
    wait_for_proper_size();
    if (should_quit) {
      endwin();
      printf("Program terminated.\n");
      return 0;
    }
  }

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

    attron(A_BOLD);
    mvprintw(0, 2, "RoomChatServer Client");
    attroff(A_BOLD);

    for (int i = 0; i < cols; i++) {
      mvprintw(1, i, "-");
    }

    for (int i = 0; i < cols; i++) {
      mvprintw(rows - 4, i, "-");
    }

    mvprintw(rows - 2, 0, "> %s", input_buffer);

    move(rows - 2, 2 + cursor_pos);

    refresh();

    int ch = getch();

    if (ch == KEY_RESIZE) {
      resized = 1;
    } else if (ch == '\n' || ch == '\r') {
      if (strcmp(input_buffer, "quit") == 0 ||
          strcmp(input_buffer, "exit") == 0) {
        should_quit = 1;
      }

      memset(input_buffer, 0, INPUT_MAX);
      cursor_pos = 0;
      input_len = 0;
    } else if (ch == KEY_BACKSPACE || ch == 127) {
      if (cursor_pos > 0) {
        memmove(&input_buffer[cursor_pos - 1], &input_buffer[cursor_pos],
                input_len - cursor_pos + 1);
        cursor_pos--;
        input_len--;
      } else {
        input_buffer[0] = '\0';
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
    } else if (ch == 27) {
      memset(input_buffer, 0, INPUT_MAX);
      cursor_pos = 0;
      input_len = 0;
    } else if ((ch >= 32) && (ch <= 126)) {
      if (cursor_pos < input_len) {
        memmove(&input_buffer[cursor_pos + 1], &input_buffer[cursor_pos],
                input_len - cursor_pos);
      }
      input_buffer[cursor_pos] = ch;
      cursor_pos++;
      input_len++;
    } /*else if (ch == 'q' || ch == 'Q') {
     if (cursor_pos == 0) {
        should_quit = 1;
      }
    }*/
  }

  endwin();
  printf("Program terminated successfully.\n");
  return 0;
}
