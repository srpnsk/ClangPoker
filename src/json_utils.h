#include "protocol.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define BUFFER_SIZE 2048

void send_json_message(int socket, MessageType type, const Message *msg);
bool receive_json_message(int socket, Message *msg);
