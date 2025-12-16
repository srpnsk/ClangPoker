#include "card_utils.h"
#include <string.h>

Color parse_color(const char *color_str) {
  if (!color_str)
    return COL_WILD;
  if (strcmp(color_str, "red") == 0)
    return COL_RED;
  if (strcmp(color_str, "yellow") == 0)
    return COL_YELLOW;
  if (strcmp(color_str, "green") == 0)
    return COL_GREEN;
  if (strcmp(color_str, "blue") == 0)
    return COL_BLUE;
  return COL_WILD;
}

Value parse_value(const char *value_str) {
  if (!value_str)
    return VAL_0;
  if (strcmp(value_str, "0") == 0)
    return VAL_0;
  if (strcmp(value_str, "1") == 0)
    return VAL_1;
  if (strcmp(value_str, "2") == 0)
    return VAL_2;
  if (strcmp(value_str, "3") == 0)
    return VAL_3;
  if (strcmp(value_str, "4") == 0)
    return VAL_4;
  if (strcmp(value_str, "5") == 0)
    return VAL_5;
  if (strcmp(value_str, "6") == 0)
    return VAL_6;
  if (strcmp(value_str, "7") == 0)
    return VAL_7;
  if (strcmp(value_str, "8") == 0)
    return VAL_8;
  if (strcmp(value_str, "9") == 0)
    return VAL_9;
  if (strcmp(value_str, "skip") == 0)
    return VAL_SKIP;
  if (strcmp(value_str, "reverse") == 0)
    return VAL_REVERSE;
  if (strcmp(value_str, "draw_two") == 0)
    return VAL_DRAW_TWO;
  if (strcmp(value_str, "wild") == 0)
    return VAL_WILD;
  if (strcmp(value_str, "wild_draw_four") == 0)
    return VAL_WILD_DRAW_FOUR;
  return VAL_0;
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

bool can_play_card(Card player_card, Color current_color, Card top_card) {
  if (player_card.color == COL_WILD)
    return true;
  if (player_card.color == current_color)
    return true;
  if (player_card.value == top_card.value)
    return true;
  return false;
}
