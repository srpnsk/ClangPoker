#ifndef CLIENT_UI_H
#define CLIENT_UI_H

#include "card_utils.h"
#include "client_net.h"
#include "common.h"
#include <cjson/cJSON.h>
#include <ctype.h>
#include <locale.h>
#include <ncurses.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#define MIN_WIDTH 100
#define MIN_HEIGHT 30
#define BUFFER_SIZE 4096

typedef struct {
  char text[4096];
  bool from_server;
  bool is_json;
  time_t timestamp;
} Message;

typedef struct {
  char player_name[50];
  char action[100];
  Card card;
  time_t timestamp;
} GameAction;

extern volatile sig_atomic_t resized;
extern int should_quit;
extern Message message_history[100];
extern int message_count;
extern GameAction game_history[100];
extern int game_history_count;
extern Card player_hand[20];

extern PlayerInfo players[6];
extern char player_name[50];
extern Card top_card;
extern Color current_color;
extern bool direction_clockwise;
extern int time_left;

void handle_sigwinch(int sig);
void init_signals(void);
bool check_min_size(void);
void show_size_warning(void);
void wait_for_proper_size(void);

// client_utils.h - добавить в конец
extern bool in_lobby;
extern bool waiting_for_player_selection;
extern int selected_players;
extern char room_status[100];

void add_message(const char *text, bool from_server, bool is_json);
// void add_game_action(const char *player, const char *action, Card card);

void parse_game_state(const char *json_string);

bool handle_command(const char *command);
bool handle_player_selection(int ch);

void print_card_fancy(int x, int y, Card card, int index, bool highlight);
void draw_player_hand(int start_y, int start_x, int width);
void draw_message_history(int start_row, int end_row, int width);
void draw_players(int start_y, int start_x, int width);
void draw_game_state(int start_y, int start_x, int width);
void draw_interface(int rows, int cols);
void draw_player_selection_screen(int rows, int cols);

#endif
