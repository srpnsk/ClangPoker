#include <cjson/cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void send_play_card_request(int sock, int card_index) {
  cJSON *request = cJSON_CreateObject();
  cJSON_AddStringToObject(request, "action", "PLAY_CARD");
  cJSON_AddNumberToObject(request, "card_index", card_index);

  char *json_string = cJSON_PrintUnformatted(request);

  // Отправка JSON и разделителя '\n'
  write(sock, json_string, strlen(json_string));
  write(sock, "\n", 1);

  free(json_string);
  cJSON_Delete(request);
}
