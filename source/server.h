#ifndef SERVER_H
#define SERVER_H
#include <arpa/inet.h>
#include <cjson/cJSON.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_EVENTS 1000
#define MAX_CONNECTIONS 10000
#define UNO_DECK_SIZE 108
#define MAX_ROOMS 10
#define MAX_PLAYERS_PER_ROOM 6
#define MIN_PLAYERS_PER_ROOM 2
#define TURN_TIMEOUT 30

// Структуры для карт
typedef enum { COL_RED, COL_YELLOW, COL_GREEN, COL_BLUE, COL_WILD } Color;

typedef enum {
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
  VAL_SKIP,
  VAL_REVERSE,
  VAL_DRAW_TWO,
  VAL_WILD,
  VAL_WILD_DRAW_FOUR
} Value;

typedef struct {
  Color color;
  Value value;
} Card;

typedef struct {
  Card cards[UNO_DECK_SIZE];
  int top;
} Deck;

typedef struct PlayerStruct {
  int fd;
  struct sockaddr_in addr;
  char name[50];
  Card hand[20];
  int hand_size;
  bool connected;
  bool in_game;
  bool is_turn;
  int score;
  time_t last_action;
} Player;

typedef enum {
  WAITING_FOR_PLAYERS,
  IN_PROGRESS,
  ROUND_ENDED,
  GAME_ENDED
} GameState;

typedef struct {
  GameState state;
  int current_player;
  bool direction_clockwise;
  Card top_card;
  Deck draw_pile;
  Deck discard_pile;
  int players_count;
  Player players[MAX_PLAYERS_PER_ROOM];
  Color current_color;
  int draw_accumulator;
  int player_to_draw;
} Game;

// Структура комнаты
typedef struct Room {
  int id;
  Game game;
  pthread_mutex_t lock;
  time_t last_activity;
  bool is_active;
  int connected_players;
  char save_file[256];
  struct Room *next;
} Room;

// Глобальные переменные
typedef struct {
  int fd;
  struct sockaddr_in addr;
} ClientInfo;

// Прототипы функций
void create_uno_deck(Deck *deck);
void shuffle_deck(Deck *deck);
Card draw_card(Deck *deck);
const char *color_to_string(Color color);
const char *value_to_string(Value value);
cJSON *create_card_json(Card card);
void save_game_state(Room *room);
void load_game_state(Room *room, const char *filename);
Room *create_room(void);
Room *find_room_by_id(int room_id);
Room *find_player_room(int player_fd);
void *timeout_monitor(void *arg);
void broadcast_to_room(Room *room, const char *message, int exclude_fd);
void next_player_room(Room *room);
bool can_play_card(Card player_card, Color current_color, Card top_card);
char *handle_client_command(int client_fd, const char *command);
char *handle_room_command(Room *room, int client_fd, const char *command);
void handle_signal(int sig);
int set_keepalive(int sockfd);
void close_client(int client_fd, int epoll_fd);
void cleanup_rooms(void);

#include <pthread.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_EVENTS 1000
#define MAX_CONNECTIONS 10000
#define UNO_DECK_SIZE 108
#define MAX_ROOMS 10

#define MAX_PLAYERS_PER_ROOM 6
#define MIN_PLAYERS_PER_ROOM 2
#define TURN_TIMEOUT 30 // секунд

// Глобальные структуры
extern pthread_mutex_t rooms_mutex;

// Прототипы функций
Room *find_available_room(void);
Room *find_room_by_id(int room_id);
Room *find_player_room(int player_fd);
void save_game_state(Room *room);
void load_game_state(Room *room, const char *filename);
void *timeout_monitor(void *arg);
void cleanup_empty_rooms(void);
void broadcast_to_room(Room *room, const char *message, int exclude_fd);

static volatile sig_atomic_t running = 1;
#endif
