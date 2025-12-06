#include "utils.h"

void printCard(const Card *c) {
  const char *colors[] = {"Red", "Yellow", "Green", "Blue", "Wild"};
  const char *values[] = {
      "0",    "1",       "2",        "3",    "4",
      "5",    "6",       "7",        "8",    "9",
      "Skip", "Reverse", "Draw Two", "Wild", "Wild Draw Four"};

  printf("%s %s\n", colors[c->color], values[c->value]);
}

void printCardFancy(int x, int y, const Card *c) {
  const char *value_symbols[] = {" 0 ", " 1 ", " 2 ", " 3 ", " 4 ",
                                 " 5 ", " 6 ", " 7 ", " 8 ", " 9 ",
                                 "SKP", "RVS", "+2 ", "WLD", "+4 "};

  char color_char;
  int color_pair;

  switch (c->color) {
  case 0:
    color_char = 'R';
    color_pair = COLOR_PAIR(1) | A_BOLD;
    break;
  case 1:
    color_char = 'Y';
    color_pair = COLOR_PAIR(2) | A_BOLD;
    break;
  case 2:
    color_char = 'G';
    color_pair = COLOR_PAIR(3) | A_BOLD;
    break;
  case 3:
    color_char = 'B';
    color_pair = COLOR_PAIR(4) | A_BOLD;
    break;
  case 4:
    color_char = 'W';
    color_pair = COLOR_PAIR(5) | A_BOLD;
    break;
  default:
    color_char = '?';
    color_pair = A_NORMAL;
    break;
  }

  attron(color_pair);
  mvprintw(y, x, "╔═══════╗");
  mvprintw(y + 1, x, "║%c      ║", color_char);
  mvprintw(y + 2, x, "║  %s  ║", value_symbols[c->value]);
  mvprintw(y + 3, x, "║      %c║", color_char);
  mvprintw(y + 4, x, "╚═══════╝");
  attroff(color_pair);
}

void printCardsInRow(Card cards[], int count, int start_x, int start_y) {
  for (int i = 0; i < count; i++) {
    printCardFancy(start_x + i * 9, start_y, &cards[i]);
  }
}
