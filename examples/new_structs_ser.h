#include <time.h>
#define MAX_ROOMS 10
#define MAX_PLAYERS_PER_ROOM 6
#define BUFFER_LIMIT 255

// Структура для игрового состояния (колода, сброс, направление и т.д.)
typedef struct {
  // ... Данные игры UNO (Deck, Discard pile, Turn direction, etc.)
  int current_player_index;
  time_t last_move_time; // Для таймаута
  int player_count;
} GameState;

// Игрок теперь будет частью комнаты
typedef struct {
  // Player structure from gameplay.h (Network, Hand, Name)
  int sockfd;
  char inbuf[BUFFER_LIMIT];
  // bool ready; // Статус готовности
  // ...
} Player;

// Структура Игровой Комнаты
typedef struct {
  int room_id;
  int status; // LOBBY, IN_GAME, FINISHED
  Player players[MAX_PLAYERS_PER_ROOM];
  GameState game_state;
  // ... Возможно, очередь JSON-сообщений для отправки (out_queue)
} Room;

// Главный массив всех комнат
// Room rooms[MAX_ROOMS]; // создать в основном файле программы
