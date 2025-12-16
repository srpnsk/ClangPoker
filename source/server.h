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

#include "common.h"

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_EVENTS 1000
#define MAX_CONNECTIONS 10000
#define UNO_DECK_SIZE 108
#define MAX_ROOMS 10
#define MAX_PLAYERS_PER_ROOM 6
#define MIN_PLAYERS_PER_ROOM 2
#define TURN_TIMEOUT 30

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

// Глобальные структуры
extern pthread_mutex_t rooms_mutex;
extern volatile sig_atomic_t running;

#endif
