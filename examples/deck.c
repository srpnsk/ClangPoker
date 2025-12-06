// deck.c
#include "deck.h"
#include <stdlib.h>
#include <time.h>

// Generates our 108 cards according to official UNO rules
// ---- Fisher-Yates shuffle ----
void shuffleDeck(Deck *deck) {
  srand(time(NULL));
  for (int i = UNO_DECK_SIZE - 1; i > 0; i--) {
    int j = rand() % (i + 1);
    Card temp = deck->cards[i];
    deck->cards[i] = deck->cards[j];
    deck->cards[j] = temp;
  }
  deck->top = 0;
}

// // ---- Draw a card ----
// Card drawCard(Deck *deck) {
//     if (deck->top < UNO_DECK_SIZE) {
//         return deck->cards[deck->top++];
//     }
//     // Fallback (should never happen)
//     Card empty = { COLOR_WILD, VALUE_WILD };
//     return empty;
// }

// New! refill with refill from discard deck
Card drawCard(Deck *deck, DiscardPile *discard) {
  if (deck->top >= UNO_DECK_SIZE) {
    // Try to refill from discard pile
    refillDrawPile(deck, discard);

    // If still empty, this is a fatal logic error
    if (deck->top >= UNO_DECK_SIZE) {
      Card empty = {COL_WILD, VAL_WILD};
      return empty; // emergency fallback
    }
  }

  return deck->cards[deck->top++];
}

// ---- Create a full UNO deck (108 cards) ----
void createUnoDeck(Deck *deck) {
  int index = 0;

  // 4 colors (R/Y/G/B)
  for (Color c = COL_RED; c <= COL_BLUE; c++) {

    // One zero
    deck->cards[index++] = (Card){c, VAL_0};

    // Two copies of 1–9
    for (Value v = VAL_1; v <= VAL_9; v++) {
      deck->cards[index++] = (Card){c, v};
      deck->cards[index++] = (Card){c, v};
    }

    // Two Skip, Reverse, Draw Two
    for (int i = 0; i < 2; i++) {
      deck->cards[index++] = (Card){c, VAL_SKIP};
      deck->cards[index++] = (Card){c, VAL_REVERSE};
      deck->cards[index++] = (Card){c, VAL_DRAW_TWO};
    }
  }

  // 4 Wild
  for (int i = 0; i < 4; i++)
    deck->cards[index++] = (Card){COL_WILD, VAL_WILD};

  // 4 Wild Draw Four
  for (int i = 0; i < 4; i++)
    deck->cards[index++] = (Card){COL_WILD, VAL_WILD_DRAW_FOUR};

  deck->top = 0;
}

// Initializing discard pile
void initDiscardPile(DiscardPile *dp) { dp->top = 0; }

// Push a played card onto the discard pile
void discardCard(DiscardPile *dp, Card c) { dp->cards[dp->top++] = c; }

// Deck refill

void refillDrawPile(Deck *draw, DiscardPile *discard) {
  if (discard->top <= 1) {
    // No cards to refill with (impossible in real gameplay)
    return;
  }

  // Save the top discard card
  Card topCard = discard->cards[discard->top - 1];

  // Move all but the top card back into draw pile
  int d = 0;
  for (int i = 0; i < discard->top - 1; i++) {
    draw->cards[d++] = discard->cards[i];
  }

  draw->top = 0; // Reset draw pile position

  // Shuffle the replenished draw pile
  srand(time(NULL));
  for (int i = d - 1; i > 0; i--) {
    int j = rand() % (i + 1);
    Card temp = draw->cards[i];
    draw->cards[i] = draw->cards[j];
    draw->cards[j] = temp;
  }

  // Reset discard pile to only contain the top card
  discard->cards[0] = topCard;
  discard->top = 1;
}
