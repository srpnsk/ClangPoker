// card.h
#ifndef CARD_H
#define CARD_H
#include <stdio.h>

// ----- COLOR ENUM -----
typedef enum {
  COL_RED,
  COL_YELLOW,
  COL_GREEN,
  COL_BLUE,
  COL_WILD // Only for wild cards
} Color;

// ----- VALUE / TYPE ENUM -----
typedef enum {
  // Number cards
  VAL_0,
  VAL_1,
  VAL_2,
  VAL_3,
  VAL_4,
  VAL_5,
  VAL_6,
  VAL_7,
  VAL_8,
  VAL_9,
  // Action cards
  VAL_SKIP,
  VAL_REVERSE,
  VAL_DRAW_TWO,

  // Wilds
  VAL_WILD,
  VAL_WILD_DRAW_FOUR
} Value;

// ----- CARD STRUCT -----
typedef struct {
  Color color;
  Value value;
} Card;

#endif

void printCard(const Card *c);

int isValidMove(const Card *played, const Card *top);
