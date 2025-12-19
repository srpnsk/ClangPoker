#include <cjson/cJSON.h>
#include <stdbool.h>

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

typedef enum {
  MSG_INVALID = 0,

  /* client → server */
  MSG_CLI_HELLO,
  MSG_CLI_JOIN,
  MSG_CLI_CHAT,
  MSG_CLI_GAME,

  /* server → client */
  MSG_SER_WELCOME,
  MSG_SER_CHAT,
  MSG_SER_EVENT,
  MSG_SER_STATE,
  MSG_SER_ERROR
} MessageType;

typedef enum {
  ACT_NONE = 0,

  ACT_READY,
  ACT_PLAY_CARD,
  ACT_DRAW_CARD,
  ACT_LEAVE_GAME
} ClientAction;

typedef enum {
  EV_NONE = 0,

  EV_PLAYER_LEFT,

  EV_GAME_STARTED,
  EV_GAME_ENDED,

  EV_CARD_PLAYED,
  EV_CARD_DRAWN,

  EV_TURN_CHANGED,

  EV_ERROR
} ServerEvent;

typedef struct {
  MessageType type;
  char username[32];

  union {
    struct {
      int players;
    } join;

    struct {
      char text[512];
    } chat;

    struct {
      ClientAction action;
      int card_index; // только для ACT_PLAY_CARD
    } game;
  };
} MsgCliSer;

typedef struct {
  MessageType type;

  union {
    /* ----------- CHAT ----------- */
    struct {
      char username[32];
      char text[512];
    } chat;

    /* ----------- EVENT ----------- */
    struct {
      ServerEvent event;

      union {
        struct {
          char username[32];
        } player;

        struct {
          char username[32];
          Card card;
        } card_played;

        struct {
          char username[32];
          int count;
        } card_drawn;

        struct {
          char current_player[32];
        } turn;

        struct {
          char message[256];
        } error;
      };
    } event;

    /* ----------- GAME STATE ----------- */
    struct {
      bool your_turn;
      Card top_card;

      Card your_hand[20];
      int hand_count;

      char current_player[32];
      int players_in_game;
    } state;

    /* ----------- SYSTEM / ERROR ----------- */
    struct {
      char message[256];
    } system;
  };
} MsgSer;

const char *color_to_string(Color color);

Color string_to_color(const char *str);

const char *value_to_string(Value value);
Value string_to_value(const char *str);
const char *message_type_to_string(MessageType type);
MessageType string_to_message_type(const char *str);
const char *client_action_to_string(ClientAction action);
ClientAction string_to_client_action(const char *str);
const char *server_event_to_string(ServerEvent event);
ServerEvent string_to_server_event(const char *str);
cJSON *card_to_json(const Card *card);
bool json_to_card(const cJSON *json, Card *card);
char *serialize_msg_cliser(const MsgCliSer *msg);
bool deserialize_msg_cliser(const char *json_str, MsgCliSer *out);
char *serialize_msg_ser(const MsgSer *msg);
bool deserialize_msg_ser(const char *json_str, MsgSer *out);
