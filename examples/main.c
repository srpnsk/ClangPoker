#include "card.h"
#include "deck.h"

int main() {
  Deck deck, discard;
  createUnoDeck(&deck);
  createUnoDeck(&discard);
  shuffleDeck(&deck);

  // Test: print the first 20 cards
  for (int i = 0; i < 20; i++) {
    Card c = drawCard(&deck, &discard);
    printCard(&c);
  }

  return 0;
}
