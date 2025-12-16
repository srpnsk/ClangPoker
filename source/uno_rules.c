#include "uno_rules.h"

int next_player_index(Game *game) {
  if (game->direction_clockwise) {
    return (game->current_player + 1) % game->players_count;
  } else {
    return (game->current_player - 1 + game->players_count) %
           game->players_count;
  }
}

void apply_card_effect(Game *game, Card played_card) {
  switch (played_card.value) {
  case VAL_SKIP:
    game->current_player = next_player_index(game);
    break;

  case VAL_REVERSE:
    game->direction_clockwise = !game->direction_clockwise;

    if (game->players_count == 2) {
      game->current_player = next_player_index(game);
    }
    break;

  case VAL_DRAW_TWO:
    game->draw_accumulator += 2;
    game->player_to_draw = next_player_index(game);
    break;

  case VAL_WILD_DRAW_FOUR:
    game->draw_accumulator += 4;
    game->player_to_draw = next_player_index(game);
    break;

  default:
    break;
  }
}

void advance_turn(Game *game) {
  game->players[game->current_player].is_turn = false;
  game->current_player = next_player_index(game);
  game->players[game->current_player].is_turn = true;
}

void create_uno_deck(Deck *deck) {
  int index = 0;

  for (Color c = COL_RED; c <= COL_BLUE; c++) {
    deck->cards[index++] = (Card){c, VAL_0};

    for (Value v = VAL_1; v <= VAL_9; v++) {
      deck->cards[index++] = (Card){c, v};
      deck->cards[index++] = (Card){c, v};
    }

    for (int i = 0; i < 2; i++) {
      deck->cards[index++] = (Card){c, VAL_SKIP};
      deck->cards[index++] = (Card){c, VAL_REVERSE};
      deck->cards[index++] = (Card){c, VAL_DRAW_TWO};
    }
  }

  for (int i = 0; i < 4; i++)
    deck->cards[index++] = (Card){COL_WILD, VAL_WILD};

  for (int i = 0; i < 4; i++)
    deck->cards[index++] = (Card){COL_WILD, VAL_WILD_DRAW_FOUR};

  deck->top = 0;
}

void shuffle_deck(Deck *deck) {
  for (int i = UNO_DECK_SIZE - 1; i > 0; i--) {
    int j = rand() % (i + 1);
    Card temp = deck->cards[i];
    deck->cards[i] = deck->cards[j];
    deck->cards[j] = temp;
  }
}

// FIXME: что будет, если колода кончится?..
Card draw_card(Deck *deck) {
  if (deck->top < UNO_DECK_SIZE) {
    return deck->cards[deck->top++];
  }
  return (Card){COL_WILD, VAL_WILD};
}
