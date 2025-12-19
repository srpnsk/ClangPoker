#include "protocol.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define BUFFER_SIZE 8192

bool send_server_json(int socket, const MsgSer *msg);
bool send_server_simple_message(int socket, MessageType type,
                                const char *message);
bool send_server_game_state(int socket, bool your_turn, const Card *top_card,
                            const Card *hand, int hand_count,
                            const char *current_player, int players_in_game);
bool send_server_event(int socket, ServerEvent event, const char *username,
                       const Card *card, int card_count, const char *message);

bool receive_client_json(int socket, MsgCliSer *out);

bool send_client_json(int socket, const MsgCliSer *msg);
bool send_client_simple_message(int socket, MessageType type,
                                const char *username);
bool send_client_chat_message(int socket, const char *username,
                              const char *text);
bool send_client_game_action(int socket, const char *username,
                             ClientAction action, int card_index);

bool send_client_join_game(int socket, const char *username, int players);

bool receive_server_json(int socket, MsgSer *out);
