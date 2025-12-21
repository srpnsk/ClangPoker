#define USERNAME 16
#define TEXT 1024

#define PORT 8080

typedef enum {
  MSG_ERR,
  MSG_HELLO,
  MSG_LIST_ROOMS,
  MSG_JOIN_ROOM,
  MSG_CHAT,
  MSG_LEAVE_ROOM,
  MSG_EXIT
} msg_type;

typedef struct {
  msg_type type;
  int room_id;
  int participants;
  char username[USERNAME];
  char text[TEXT];
} message;

void init_msg(message *msg);
