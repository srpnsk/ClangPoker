#include "card.h"

// Returns 1 if the played card can be legally placed on top of `top`
int isValidMove(const Card *played, const Card *top) {

  // Wild cards are always valid
  if (played->value == VAL_WILD || played->value == VAL_WILD_DRAW_FOUR)
    return 1;

  // If top card is a wild, allow anything that matches the chosen color
  // (you will set chosenColor externally)
  if (top->color == COL_WILD) {
    // If color was set during wild effect, the top card's color will be that
    // chosen color
    if (played->color == top->color)
      return 1;
  }

  // Match by color
  if (played->color == top->color)
    return 1;

  // Match by number or action type
  if (played->value == top->value)
    return 1;

  // Otherwise not valid
  return 0;
}

// A card is valid if:

//  -It is wild (always playable)

//  -It matches the color of the top card

//  -Same number or same action as previously placed card

//  -It matches a color chosen by a previous wild

// helper for readability
int isWild(const Card *c) {
  return c->value == VAL_WILD || c->value == VAL_WILD_DRAW_FOUR;
}

int isActionCard(const Card *c) {
  return c->value == VAL_SKIP || c->value == VAL_REVERSE ||
         c->value == VAL_DRAW_TWO;
}

int isNumberCard(const Card *c) { return c->value <= VAL_9; }
