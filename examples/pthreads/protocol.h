#include <stdbool.h>

typedef enum {
  MSG_HELLO,
  MSG_JOIN_ROOM,
  MSG_CHAT,
  MSG_SYSTEM,
  MSG_SYSTEM_HELLO,
  MSG_SYSTEM_INVITE,
  MSG_ERROR,
  MSG_ROOM_READY,
  MSG_ROOM_FORWARD,
  MSG_ROOM_EVENT
} MessageType;

typedef struct {
  MessageType type;
  char username[32];
  int players;
  char text[512];
} Message;

MessageType type_from_string(const char *s);
const char *type_to_string(MessageType type);
bool parse_json_message(const char *json, Message *out);
char *build_json_message(MessageType type, const Message *msg);
