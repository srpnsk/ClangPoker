#include "../examples/card.h"
#include "../examples/deck.h"
#include <ncurses.h>

void printCard(const Card *c);
void printCardFancy(int x, int y, const Card *c);
void printCardsInRow(Card cards[], int count, int start_x, int start_y);
