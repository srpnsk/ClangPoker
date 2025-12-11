// deck.h
#ifndef DECK_H
#define DECK_H

#include "card.h"

#define UNO_DECK_SIZE 108

typedef struct {
  Card cards[UNO_DECK_SIZE];
  int top; // index of the next card to draw
} Deck;

void createUnoDeck(Deck *deck);

void shuffleDeck(Deck *deck);

Card drawCard(Deck *deck, Deck *discard);

void initDiscardPile(Deck *dp);

void refillDrawPile(Deck *draw, Deck *discard);

#endif
