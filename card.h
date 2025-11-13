#ifndef CARD_H
#define CARD_H
enum ranks {
  two = 2,
  three,
  four,
  five,
  six,
  seven,
  eight,
  nine,
  ten,
  jack,
  queen,
  king,
  ace
};

enum suits { spades, hearts, diamonds, clubs };

typedef struct {
  enum ranks rank;
  enum suits suit;
} Card;
#endif
