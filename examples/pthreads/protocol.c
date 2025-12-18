#include "protocol.h"
#include <cjson/cJSON.h>
#include <string.h>

MessageType type_from_string(const char *s) {
  if (!s)
    return MSG_ERROR;
  if (!strcmp(s, "hello"))
    return MSG_HELLO;
  if (!strcmp(s, "join_room"))
    return MSG_JOIN_ROOM;
  if (!strcmp(s, "chat"))
    return MSG_CHAT;
  if (!strcmp(s, "system"))
    return MSG_SYSTEM;
  if (!strcmp(s, "system_hello"))
    return MSG_SYSTEM_HELLO;
  if (!strcmp(s, "system_invite"))
    return MSG_SYSTEM_INVITE;
  if (!strcmp(s, "room_ready"))
    return MSG_ROOM_READY;
  if (!strcmp(s, "room_forward"))
    return MSG_ROOM_FORWARD;
  if (!strcmp(s, "room_event"))
    return MSG_ROOM_EVENT;
  return MSG_ERROR;
}

const char *type_to_string(MessageType type) {
  switch (type) {
  case MSG_HELLO:
    return "hello";
  case MSG_JOIN_ROOM:
    return "join_room";
  case MSG_CHAT:
    return "chat";
  case MSG_SYSTEM:
    return "system";
  case MSG_SYSTEM_HELLO:
    return "system_hello";
  case MSG_SYSTEM_INVITE:
    return "system_invite";
  case MSG_ROOM_READY:
    return "room_ready";
  case MSG_ROOM_FORWARD:
    return "room_forward";
  case MSG_ROOM_EVENT:
    return "room_event";
  default:
    return "error";
  }
}

bool parse_json_message(const char *json, Message *out) {
  if (!json || !out)
    return false;

  memset(out, 0, sizeof(*out));

  cJSON *root = cJSON_Parse(json);
  if (!root)
    return false;

  cJSON *type = cJSON_GetObjectItem(root, "type");
  if (!cJSON_IsString(type)) {
    cJSON_Delete(root);
    return false;
  }
  out->type = type_from_string(type->valuestring);

  cJSON *payload = cJSON_GetObjectItem(root, "payload");
  if (cJSON_IsObject(payload)) {
    cJSON *u = cJSON_GetObjectItem(payload, "username");
    if (cJSON_IsString(u))
      strncpy(out->username, u->valuestring, sizeof(out->username) - 1);

    cJSON *p = cJSON_GetObjectItem(payload, "players");
    if (cJSON_IsNumber(p))
      out->players = p->valueint;

    cJSON *t = cJSON_GetObjectItem(payload, "text");
    if (cJSON_IsString(t))
      strncpy(out->text, t->valuestring, sizeof(out->text) - 1);
  }

  cJSON_Delete(root);
  return true;
}

char *build_json_message(MessageType type, const Message *msg) {
  cJSON *root = cJSON_CreateObject();
  cJSON *payload = cJSON_CreateObject();

  cJSON_AddStringToObject(root, "type", type_to_string(type));

  if (msg) {
    if (msg->username[0])
      cJSON_AddStringToObject(payload, "username", msg->username);
    if (msg->players > 0)
      cJSON_AddNumberToObject(payload, "players", msg->players);
    if (msg->text[0])
      cJSON_AddStringToObject(payload, "text", msg->text);
  }

  cJSON_AddItemToObject(root, "payload", payload);
  char *json_str = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);

  return json_str;
}
