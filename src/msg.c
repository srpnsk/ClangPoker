#include "msg.h"
#include <string.h>

void init_msg(message *msg) {
  msg->type = MSG_ERR;
  msg->room_id = -1;
  msg->participants = -1;
  memset(msg->username, 0, USERNAME);
  memset(msg->text, 0, TEXT);
}
